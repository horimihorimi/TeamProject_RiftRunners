#include "BossWaveProjectile.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "GameplayEffect.h"
#include "Net/UnrealNetwork.h"
#include "GameCore/Ability/DamageSystemLibrary.h"
#include "GameCore/Character/BaseCharacter.h"
#include "GameCore/Combat/MonsterDamageLibrary.h"
#include "GameCore/Tags/GameplayTagSystem.h"

ABossWaveProjectile::ABossWaveProjectile()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	SetReplicateMovement(true);

	HitBox = CreateDefaultSubobject<UBoxComponent>(TEXT("HitBox"));
	SetRootComponent(HitBox);
	HitBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);

	/*
	 * 공격 판정 채널만 열고, 아군인지 적인지는 태그로 가른다.
	 *
	 * 프로젝트가 이미 그 방식이다. MonsterDamageLibrary도 ECC_TraceAttack으로
	 * 훑은 뒤 Character.Player 태그가 있는 대상만 남긴다. 채널을 새로 만들면
	 * 같은 판단이 두 군데로 갈라진다.
	 *
	 * 실제 판정은 Tick의 ECC_TraceAttack Sweep이 수행한다. HitBox는 판정 크기와
	 * 연출의 기준만 들고 있으므로 이벤트 충돌은 만들지 않는다.
	 */
	HitBox->SetCollisionObjectType(ECC_TraceAttack);
	HitBox->SetCollisionResponseToAllChannels(ECR_Ignore);
	HitBox->SetGenerateOverlapEvents(false);

	WaveMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WaveMesh"));
	WaveMesh->SetupAttachment(HitBox);
	WaveMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void ABossWaveProjectile::BeginPlay()
{
	Super::BeginPlay();
}

void ABossWaveProjectile::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ABossWaveProjectile, WaveExtent);
}

void ABossWaveProjectile::InitializeWave(
	const FVector& InBoxExtent,
	const FVector& InDirection,
	const float InSpeed,
	const FGameplayTag& InAttackTag,
	TSubclassOf<UGameplayEffect> InDamageEffectClass)
{
	Direction = InDirection.GetSafeNormal();
	Speed = InSpeed;
	AttackTag = InAttackTag;

	if (InDamageEffectClass)
	{
		DamageEffectClass = InDamageEffectClass;
	}

	WaveExtent = InBoxExtent;
	ApplyWaveExtent();
}

void ABossWaveProjectile::OnRep_WaveExtent()
{
	ApplyWaveExtent();
}

void ABossWaveProjectile::ApplyWaveExtent()
{
	if (IsValid(HitBox))
	{
		HitBox->SetBoxExtent(WaveExtent, true);
	}

	/*
	 * 메시는 판마다 다른 치수에 맞춰 늘린다.
	 *
	 * BoxExtent는 절반 크기라 두 배로 되돌린 뒤 원본 치수로 나눈다.
	 * MeshBaseSize가 실제 메시와 다르면 콜리전과 보이는 크기가 어긋난다.
	 */
	if (IsValid(WaveMesh) && !MeshBaseSize.IsNearlyZero())
	{
		const FVector FullSize = WaveExtent * 2.0f;
		WaveMesh->SetRelativeScale3D(FVector(
			MeshBaseSize.X > KINDA_SMALL_NUMBER ? FullSize.X / MeshBaseSize.X : 1.0f,
			MeshBaseSize.Y > KINDA_SMALL_NUMBER ? FullSize.Y / MeshBaseSize.Y : 1.0f,
			MeshBaseSize.Z > KINDA_SMALL_NUMBER ? FullSize.Z / MeshBaseSize.Z : 1.0f));
	}
}

void ABossWaveProjectile::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// 발사 방향과 속도는 InitializeWave를 호출한 서버만 안다. 클라이언트가
	// 기본 ForwardVector로 별도 이동하면 서버 벽과 방향이 갈라진다.
	// 위치/회전은 ReplicatedMovement만으로 표시한다.
	if (!HasAuthority() || Speed <= 0.0f)
	{
		return;
	}

	const FVector Start = GetActorLocation();
	const FVector End = Start + Direction * Speed * DeltaSeconds;

	/*
	 * AddActorWorldOffset(..., false)만 쓰면 빠른 벽이 플레이어를 한 프레임에
	 * 통과해 BeginOverlap을 놓칠 수 있다. 이동한 구간 전체를 TraceAttack으로
	 * 검사하면 판의 폭/높이를 유지하면서도 누락 없이 관통형 피격을 낸다.
	 */
	if (IsValid(HitBox))
	{
		FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(BossWaveProjectile), false, this);
		QueryParams.AddIgnoredActor(GetOwner());

		TArray<FHitResult> HitResults;
		GetWorld()->SweepMultiByChannel(
			HitResults,
			Start,
			End,
			GetActorQuat(),
			ECC_TraceAttack,
			FCollisionShape::MakeBox(HitBox->GetScaledBoxExtent()),
			QueryParams);

		for (const FHitResult& Hit : HitResults)
		{
			ApplyWaveDamage(Hit);
		}
	}

	// 벽은 첫 피격이나 월드 충돌에서 멈추지 않고 아레나를 관통한다.
	AddActorWorldOffset(End - Start, false);
}

void ABossWaveProjectile::ApplyWaveDamage(const FHitResult& Hit)
{
	AActor* OtherActor = Hit.GetActor();
	AActor* InstigatorActor = GetOwner();
	if (!HasAuthority()
		|| !IsValid(OtherActor)
		|| OtherActor == this
		|| OtherActor == InstigatorActor
		|| DamagedActors.Contains(OtherActor))
	{
		return;
	}

	/*
	 * 대상 판별은 태그로 한다.
	 *
	 * 채널만으로 거르면 몬스터끼리도 겹쳐서 자기편을 때린다. 프로젝트가 이미
	 * Character.Player 태그를 기준으로 삼고 있으므로 같은 규칙을 따른다.
	 */
	const UAbilitySystemComponent* TargetASC =
		UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(OtherActor);
	if (!IsValid(TargetASC)
		|| !TargetASC->HasMatchingGameplayTag(GameplayTagSystem::Character_Player))
	{
		return;
	}

	DamagedActors.Add(OtherActor);

	FDamagePayload Payload = MonsterDamageLibrary::MakeAttackPayload(AttackTag, DamageEffectClass);
	FHitResult DamageHit = Hit;
	DamageHit.HitObjectHandle = FActorInstanceHandle(OtherActor);
	Payload.HitResult = DamageHit;

	UDamageSystemLibrary::LogDamageApplied(InstigatorActor, OtherActor, TEXT("BossWaveProjectile"));

	/*
	 * 가해자는 벽을 쏜 보스다.
	 *
	 * 보스가 먼저 죽으면 GetOwner()가 무효가 되어 날아가던 벽이 데미지를 잃는다.
	 * 자체 ASC를 두지 않은 것은 기존 몬스터 투사체와 같은 선택이다.
	 */
	UDamageSystemLibrary::ApplyGameDamage(this, InstigatorActor, Payload);
}
