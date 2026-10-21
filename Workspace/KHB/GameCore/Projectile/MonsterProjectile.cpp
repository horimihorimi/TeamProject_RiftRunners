#include "MonsterProjectile.h"

#include "Components/SphereComponent.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "AbilitySystemInterface.h"
#include "GameplayCueManager.h"
#include "GameCore/Ability/DamageSystemLibrary.h"
#include "DrawDebugHelpers.h"
#include "GameCore/Character/BaseCharacter.h"
#include "GameCore/Combat/MonsterDamageLibrary.h"
#include "GameCore/Tags/GameplayTagSystem.h"
#include "Kismet/GameplayStatics.h"
#include "MonsterProjectileMovementComponent.h"
#include "Net/UnrealNetwork.h"

namespace
{
	// 월드별로 걸러 사용한다. 소유 몬스터와 무관하게 모든 활성 유도탄을 대상으로 한다.
	TSet<TWeakObjectPtr<AMonsterProjectile>> ActiveMonsterProjectiles;
}

AMonsterProjectile::AMonsterProjectile()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	// 유도·분열의 실제 궤적은 서버만 결정한다. 클라이언트가 기본 속도로 별도
	// 시뮬레이션하면 타깃을 모르므로 직진해 서버 탄과 갈라진다.
	SetReplicateMovement(true);

	CollisionComponent = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionComponent"));
	SetRootComponent(CollisionComponent);

	CollisionComponent->InitSphereRadius(CollisionRadius);
	CollisionComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);

	/*
	 * 직접 공격, 보스 링/폭발과 같은 공격 조회 채널을 쓴다. 실제 피해 대상은
	 * 충돌 채널이 아닌 Character.Player 태그로만 걸러서 판정한다.
	 */
	CollisionComponent->SetCollisionObjectType(ECC_TraceAttack);
	CollisionComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
	CollisionComponent->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);

	NiagaraComponent = CreateDefaultSubobject<UNiagaraComponent>(TEXT("NiagaraComponent"));
	NiagaraComponent->SetupAttachment(CollisionComponent);
	NiagaraComponent->SetAutoActivate(true);

	ProjectileMovement =
		CreateDefaultSubobject<UMonsterProjectileMovementComponent>(TEXT("ProjectileMovement"));
	ProjectileMovement->SetUpdatedComponent(CollisionComponent);
	ProjectileMovement->InitialSpeed = 1200.0f;
	ProjectileMovement->MaxSpeed = 1800.0f;
	ProjectileMovement->HomingAccelerationMagnitude = 2000.0f;
}

void AMonsterProjectile::BeginPlay()
{
	Super::BeginPlay();
	ActiveMonsterProjectiles.Add(this);

	if (!HasAuthority() && IsValid(ProjectileMovement))
	{
		// 위치/회전은 ReplicatedMovement가 보간한다. 클라이언트 로컬 ProjectileMovement는
		// 서버에서만 받은 유도 대상·분열 상태를 알 수 없으므로 끈다.
		ProjectileMovement->StopMovementImmediately();
		ProjectileMovement->Deactivate();
	}

	/*
	 * 생성자의 InitSphereRadius는 CDO 기본값으로 돌아가므로, 블루프린트에서
	 * 고친 CollisionRadius를 여기서 한 번 더 반영한다.
	 */
	ApplyProjectileScale();

	// BP 자식의 Collision Preset이 C++ 생성자 기본값을 덮어쓸 수 있다.
	// 몬스터 탄은 플레이어 Pawn만 충돌 후보로 두고, 월드/몬스터/탄은 항상 관통한다.
	CollisionComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	CollisionComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
	CollisionComponent->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);

	CollisionComponent->OnComponentHit.AddDynamic(this, &AMonsterProjectile::OnProjectileHit);

	if (ProjectileLifeSpan > 0.0f)
	{
		SetLifeSpan(ProjectileLifeSpan);
	}
}

void AMonsterProjectile::PostNetReceiveVelocity(const FVector& NewVelocity)
{
	Super::PostNetReceiveVelocity(NewVelocity);

	// 무브먼트가 꺼져 있어 아무도 갱신하지 않으므로, 여기서 넣은 값이 다음 복제까지 유지된다.
	if (IsValid(CollisionComponent))
	{
		CollisionComponent->ComponentVelocity = NewVelocity;
	}

	if (IsValid(ProjectileMovement))
	{
		ProjectileMovement->Velocity = NewVelocity;
	}
}

void AMonsterProjectile::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ActiveMonsterProjectiles.Remove(this);
	if (InstigatorCharacter.IsValid())
	{
		InstigatorCharacter->OnDeathStarted.RemoveAll(this);
	}
	InstigatorCharacter = nullptr;
	Super::EndPlay(EndPlayReason);
}

FVector AMonsterProjectile::GetHomingSeparationAcceleration() const
{
	if (!bEnableHomingSeparation
		|| HomingSeparationRadius <= KINDA_SMALL_NUMBER
		|| HomingSeparationAcceleration <= 0.0f
		|| !IsValid(ProjectileMovement)
		|| !ProjectileMovement->IsHomingActive())
	{
		return FVector::ZeroVector;
	}

	const float RadiusSquared = FMath::Square(HomingSeparationRadius);
	const FVector SelfLocation = GetActorLocation();
	FVector SeparationDirection = FVector::ZeroVector;

	for (auto It = ActiveMonsterProjectiles.CreateIterator(); It; ++It)
	{
		AMonsterProjectile* Other = It->Get();
		if (!IsValid(Other))
		{
			It.RemoveCurrent();
			continue;
		}
		if (Other == this
			|| Other->GetWorld() != GetWorld()
			|| !IsValid(Other->ProjectileMovement)
			|| !Other->ProjectileMovement->IsHomingActive())
		{
			continue;
		}

		const FVector Offset = SelfLocation - Other->GetActorLocation();
		const float DistanceSquared = Offset.SizeSquared();
		if (DistanceSquared >= RadiusSquared)
		{
			continue;
		}

		if (DistanceSquared <= KINDA_SMALL_NUMBER)
		{
			// 완전히 같은 위치에서 정규화가 0이 되지 않도록 객체 ID로 반대 방향을 고정한다.
			const float Sign = GetUniqueID() < Other->GetUniqueID() ? 1.0f : -1.0f;
			SeparationDirection += GetActorRightVector() * Sign;
			continue;
		}

		const float Distance = FMath::Sqrt(DistanceSquared);
		const float Strength = 1.0f - (Distance / HomingSeparationRadius);
		SeparationDirection += (Offset / Distance) * Strength;
	}

	return SeparationDirection.GetClampedToMaxSize(1.0f) * HomingSeparationAcceleration;
}

bool AMonsterProjectile::ShouldPassThrough(AActor* OtherActor) const
{
	if (!IsValid(OtherActor) || OtherActor == this || OtherActor == GetOwner())
	{
		return true;
	}

	const UAbilitySystemComponent* TargetASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(OtherActor);
	return !IsValid(TargetASC) || !TargetASC->HasMatchingGameplayTag(GameplayTagSystem::Character_Player);
}

void AMonsterProjectile::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AMonsterProjectile, bUseSplitPattern);
	DOREPLIFETIME(AMonsterProjectile, bIsSplitChild);
	DOREPLIFETIME(AMonsterProjectile, bIsEnhancedProjectile);
	DOREPLIFETIME(AMonsterProjectile, RuntimeScaleMultiplier);
}

void AMonsterProjectile::OnRep_UseSplitPattern()
{
	/*
	 * 서버는 LaunchAtTarget에서 모탄을 세우지만 그 호출 자체는 복제되지 않는다.
	 * 클라이언트의 ProjectileMovement는 스폰 즉시 InitialSpeed로 날아가버리므로,
	 * 복제된 상태를 받는 이 시점에 똑같이 세워야 양쪽이 같은 그림이 된다.
	 */
	if (bUseSplitPattern && IsValid(ProjectileMovement))
	{
		ProjectileMovement->StopMovementImmediately();
		ProjectileMovement->Deactivate();
	}
}

void AMonsterProjectile::OnRep_ProjectileVisualState()
{
	if (bIsSplitChild)
	{
		ApplySplitVisual();
		return;
	}

	if (bIsEnhancedProjectile
		&& IsValid(NiagaraComponent)
		&& IsValid(EnhancedNiagaraEffect))
	{
		NiagaraComponent->SetAsset(EnhancedNiagaraEffect);
		NiagaraComponent->Activate(true);
	}

	ApplyProjectileScale();
}

void AMonsterProjectile::LaunchAtTarget(AActor* InTarget, AActor* InInstigatorActor)
{
	HomingTargetActor = InTarget;

	if (IsValid(InInstigatorActor))
	{
		SetOwner(InInstigatorActor);
		SetInstigator(Cast<APawn>(InInstigatorActor));

		// 발사자 본인과는 절대 충돌하지 않는다.
		CollisionComponent->IgnoreActorWhenMoving(InInstigatorActor, true);

		ApplyOwnerProjectileEffect();
	}

	/*
	 * 속도 프로파일은 셋이 완전히 분리되어 있다.
	 * 일반탄은 컴포넌트 기본값을 그대로 쓰고, 모탄과 자탄은 각자 값으로 덮어쓴다.
	 */
	if (bUseSplitPattern && !bHasSplit)
	{
		// 분열 모탄은 소켓에서 나간 그 자리에 머문다. SplitHoverDuration 뒤,
		// 타깃의 수평 축을 따라 자탄을 늘어놓는다.
		SplitForwardDirection = GetActorForwardVector().GetSafeNormal();
		if (SplitForwardDirection.IsNearlyZero())
		{
			SplitForwardDirection = FVector::ForwardVector;
		}

		ProjectileMovement->StopMovementImmediately();
		ProjectileMovement->Deactivate();
		// 모탄은 피격체가 아니라 분열 위치를 보관하는 연출용 컨테이너다.
		// 대기 중 발사자/동료와의 접촉이 분열 타이머를 끊지 않도록 판정을 끈다.
		CollisionComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		GetWorldTimerManager().SetTimer(
			SplitHoverTimerHandle,
			this,
			&AMonsterProjectile::SplitIntoProjectiles,
			FMath::Max(SplitHoverDuration, KINDA_SMALL_NUMBER),
			false);
		return;
	}

	float LaunchSpeed = 0.0f;
	float LaunchHomingDuration = HomingDuration;

	if (bIsSplitChild)
	{
		LaunchSpeed = SplitChildInitialSpeed;
		LaunchHomingDuration = SplitChildHomingDuration;
	}
	ProjectileMovement->StartHomingToTarget(InTarget, LaunchHomingDuration, LaunchSpeed);
}

void AMonsterProjectile::LaunchStraight(const FVector& InDirection, AActor* InInstigatorActor)
{
	HomingTargetActor = nullptr;

	if (IsValid(InInstigatorActor))
	{
		SetOwner(InInstigatorActor);
		SetInstigator(Cast<APawn>(InInstigatorActor));
		CollisionComponent->IgnoreActorWhenMoving(InInstigatorActor, true);
		ApplyOwnerProjectileEffect();
		if (ABaseCharacter* Character = Cast<ABaseCharacter>(InInstigatorActor))
		{
			InstigatorCharacter = Character;
			Character->OnDeathStarted.AddUObject(this, &AMonsterProjectile::OnInstigatorDeathStarted);
		}
	}

	const FVector Direction = InDirection.GetSafeNormal();
	const float LaunchSpeed = bIsSplitChild ? SplitChildInitialSpeed : ProjectileMovement->InitialSpeed;
	ProjectileMovement->Velocity = (Direction.IsNearlyZero() ? FVector::ForwardVector : Direction) * LaunchSpeed;
	ProjectileMovement->MaxSpeed = bIsSplitChild ? SplitChildMaxSpeed : ProjectileMovement->MaxSpeed;
	ProjectileMovement->Activate(true);
}

void AMonsterProjectile::SetupAsSplitChild()
{
	bIsSplitChild = true;
	RuntimeScaleMultiplier = FMath::Max(SplitChildScale, KINDA_SMALL_NUMBER);
	if (!SplitChildScaleParameterName.IsNone())
	{
		BulletScaleParameterName = SplitChildScaleParameterName;
	}

	if (IsValid(ProjectileMovement))
	{
		ProjectileMovement->HomingAccelerationMagnitude =
			FMath::Max(SplitChildHomingAcceleration, 0.0f);
		ProjectileMovement->MaxSpeed = FMath::Max(SplitChildMaxSpeed, 0.0f);
	}
}

void AMonsterProjectile::SetUseSplitPattern(const bool bInUseSplitPattern)
{
	bUseSplitPattern = bInUseSplitPattern;
	bHasSplit = false;
}

void AMonsterProjectile::BeginSplitHover()
{
	if (bHasSplit || !IsValid(ProjectileMovement))
	{
		return;
	}

	// 이전 에셋의 타이머가 남아도, 평행 진행 방향은 스폰 시점 값을 유지한다.
	SplitForwardDirection = ProjectileMovement->Velocity.GetSafeNormal();

	if (SplitForwardDirection.IsNearlyZero())
	{
		SplitForwardDirection = GetActorForwardVector();
	}

	ProjectileMovement->StopMovementImmediately();
	ProjectileMovement->Deactivate();
	CollisionComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// 멈춘 채로 버티는 동안 플레이어가 회피를 넣을 수 있다.
	GetWorldTimerManager().SetTimer(
		SplitHoverTimerHandle,
		this,
		&AMonsterProjectile::SplitIntoProjectiles,
		FMath::Max(SplitHoverDuration, KINDA_SMALL_NUMBER),
		false);
}

void AMonsterProjectile::SplitIntoProjectiles()
{
	if (bHasSplit)
	{
		return;
	}

	bHasSplit = true;

	UWorld* World = GetWorld();

	if (!World || !IsValid(HomingTargetActor))
	{
		Destroy();
		return;
	}

	// 자탄 열의 중심은 모탄이 멈춘 자리다. 여기서 좌우로 벌어져 나간다.
	const FVector SplitLocation = GetActorLocation();

	FVector Forward = SplitForwardDirection.GetSafeNormal();
	if (Forward.IsNearlyZero())
	{
		Forward = GetActorForwardVector().GetSafeNormal();
	}

	/*
	 * 진행 방향에 수직인 수평축이다. 타깃이 어느 쪽을 보고 있든 자탄은 항상
	 * 진행 방향 기준 가로로 늘어서므로, 플레이어 시점에서 좌우 3발로 보인다.
	 *
	 * 타깃의 우측 벡터를 쓰면 플레이어가 옆을 볼 때 열이 진행 방향과 나란해져
	 * 자탄이 앞뒤로 겹친다. 그건 모탄 전방이 거의 수직일 때의 폴백으로만 남긴다.
	 */
	FVector Lateral = FVector::CrossProduct(FVector::UpVector, Forward).GetSafeNormal();
	if (Lateral.IsNearlyZero())
	{
		Lateral = HomingTargetActor->GetActorRightVector();
		Lateral.Z = 0.0f;
		Lateral = Lateral.GetSafeNormal();
	}

	if (Lateral.IsNearlyZero())
	{
		Destroy();
		return;
	}

	PlaySplitBurstCue(SplitLocation);

	const int32 ProjectileCount = FMath::Max(SplitProjectileCount, 1);
	const float CenterIndex = (static_cast<float>(ProjectileCount) - 1.0f) * 0.5f;

	for (int32 Index = 0; Index < ProjectileCount; ++Index)
	{
		const float Offset = (static_cast<float>(Index) - CenterIndex) * SplitLateralSpacing;
		const FVector SpawnLocation = SplitLocation + Lateral * Offset;

		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = GetOwner();
		SpawnParams.Instigator = GetInstigator();
		SpawnParams.SpawnCollisionHandlingOverride =
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		AMonsterProjectile* Child = World->SpawnActor<AMonsterProjectile>(
			GetClass(),
			SpawnLocation,
			Forward.Rotation(),
			SpawnParams);

		if (!IsValid(Child))
		{
			continue;
		}

		Child->SetDamageEffectClass(DamageEffectClass);

		// 자탄은 모탄과 같은 공격이다. 약해지는 몴은 크기 축소분이 따로 맡는다.
		Child->SetAttackTag(AttackTag);

		// 자탄은 다시 분열하지 않는다.
		Child->SetUseSplitPattern(false);
		Child->SplitChildScale = SplitChildScale;
		Child->SplitChildScaleParameterName = SplitChildScaleParameterName;
		Child->SetupAsSplitChild();

		/*
		 * 자탄은 유도하지 않는다. 셋이 끝까지 평행하게 날아가 진행 방향에
		 * 수직인 탄막을 이룬다. 플레이어는 그 열의 바깥으로 빠져 피한다.
		 * 유도로 모으면 피할 방향이 사라지므로 여기서는 직진만 쓴다.
		 */
		Child->LaunchStraight(Forward, GetOwner());
		Child->ApplySplitVisual();
	}
	Destroy();
}

void AMonsterProjectile::PlaySplitBurstCue(const FVector& InLocation)
{
	if (!SplitBurstCueTag.IsValid())
	{
		return;
	}

	FGameplayCueParameters CueParameters;
	CueParameters.Location = InLocation;
	CueParameters.Instigator = GetOwner();
	CueParameters.EffectCauser = this;

	/*
	 * 재생할 이펙트와 사운드를 큐에 실어 보낸다. 데이터에셋은 영구 에셋이라
	 * 이 투사체가 분열 직후 파괴돼도 클라이언트에서 안전하게 해석된다.
	 */
	CueParameters.SourceObject = SplitBurstCueData;

	const IAbilitySystemInterface* AbilitySystemInterface =
		Cast<IAbilitySystemInterface>(GetOwner());

	UAbilitySystemComponent* OwnerASC =
		AbilitySystemInterface
			? AbilitySystemInterface->GetAbilitySystemComponent()
			: nullptr;

	if (IsValid(OwnerASC))
	{
		OwnerASC->ExecuteGameplayCue(SplitBurstCueTag, CueParameters);
		return;
	}

	/*
	 * 분열까지 1초 넘게 걸리므로 그 사이 발사자가 죽어 ASC가 사라질 수 있다.
	 * 그때는 큐 매니저로 직접 재생한다. Static 큐라 타깃에 ASC가 없어도 되고,
	 * 복제만 빠질 뿐 이펙트와 사운드는 같은 큐 에셋을 그대로 쓴다.
	 */
	if (UGameplayCueManager* CueManager =
			UAbilitySystemGlobals::Get().GetGameplayCueManager())
	{
		CueManager->HandleGameplayCue(
			this,
			SplitBurstCueTag,
			EGameplayCueEvent::Executed,
			CueParameters);
	}
}

void AMonsterProjectile::ApplyOwnerProjectileEffect()
{
	bIsEnhancedProjectile = false;

	if (EnhancedEffectRequiredTag.IsValid())
	{
		const IAbilitySystemInterface* AbilitySystemInterface =
			Cast<IAbilitySystemInterface>(GetOwner());

		UAbilitySystemComponent* OwnerASC =
			AbilitySystemInterface
				? AbilitySystemInterface->GetAbilitySystemComponent()
				: nullptr;

		bIsEnhancedProjectile = IsValid(OwnerASC)
			&& OwnerASC->HasMatchingGameplayTag(EnhancedEffectRequiredTag);
	}

	// 자탄은 데미지도 크기와 같은 배율로 줄인다. 모탄은 배율이 1이라 무영향.
	RuntimeDamageMultiplier =
		(bIsEnhancedProjectile ? FMath::Max(EnhancedDamageMultiplier, 0.0f) : 1.0f)
		* RuntimeScaleMultiplier;

	if (bIsEnhancedProjectile
		&& IsValid(NiagaraComponent.Get())
		&& IsValid(EnhancedNiagaraEffect.Get()))
	{
		NiagaraComponent->SetAsset(EnhancedNiagaraEffect.Get());
		NiagaraComponent->Activate(true);
	}

	ApplyProjectileScale();
}

void AMonsterProjectile::ApplyProjectileScale()
{
	/*
	 * 시각과 콜리전은 기준 단위가 달라서(스케일 배수 vs 반경 cm) 각자
	 * 기준값을 쓰되, 런타임 배율은 반드시 같은 값을 곱한다. 이렇게 해야
	 * 보이는 크기와 맞는 판정이 어긋나지 않는다.
	 */
	if (IsValid(CollisionComponent.Get()))
	{
		const float BaseRadius = (bIsEnhancedProjectile && EnhancedCollisionRadius > 0.0f)
			? EnhancedCollisionRadius
			: CollisionRadius;

		CollisionComponent->SetSphereRadius(BaseRadius * RuntimeScaleMultiplier, true);
	}

	if (IsValid(NiagaraComponent.Get()) && !BulletScaleParameterName.IsNone())
	{
		const float BaseNiagaraScale =
			bIsEnhancedProjectile ? EnhancedNiagaraScale : NormalNiagaraScale;

		// 컴포넌트 스케일은 건드리지 않는다. 크기는 사용자 파라미터가 전담한다.
		NiagaraComponent->SetVariableFloat(
			BulletScaleParameterName,
			BaseNiagaraScale * RuntimeScaleMultiplier);
	}
}

void AMonsterProjectile::ApplySplitVisual()
{
	if (IsValid(NiagaraComponent.Get()) && IsValid(SplitNiagaraEffect.Get()))
	{
		NiagaraComponent->SetAsset(SplitNiagaraEffect.Get());
		NiagaraComponent->Activate(true);
	}

	// 에셋을 갈아끼우면 사용자 파라미터가 초기화되므로 크기를 다시 넣는다.
	ApplyProjectileScale();
}

void AMonsterProjectile::SetDamageEffectClass(TSubclassOf<UGameplayEffect> InDamageEffectClass)
{
	DamageEffectClass = InDamageEffectClass;
}

void AMonsterProjectile::OnInstigatorDeathStarted(ABaseCharacter* DeadCharacter)
{
	if (HasAuthority() && DeadCharacter == InstigatorCharacter.Get())
	{
		Destroy();
	}
}

void AMonsterProjectile::OnProjectileHit(
	UPrimitiveComponent* HitComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	FVector NormalImpulse,
	const FHitResult& Hit)
{
	if (bDebugDrawHit && IsValid(CollisionComponent))
	{
		/*
		 * 벽에 맞았을 때도 그린다. 플레이어에게 맞았을 때만 그리면 빗나간
		 * 이유가 반경 때문인지 궤도 때문인지 구분할 수 없다.
		 */
		DrawDebugSphere(
			GetWorld(),
			GetActorLocation(),
			CollisionComponent->GetScaledSphereRadius(),
			16,
			FColor::Red,
			false,
			DebugDrawDuration,
			0,
			2.0f);
	}

	/*
	 * 판정과 파괴는 서버가 정한다. 클라 사본은 서버와 다른 위치를 날고 있어서,
	 * 여기서 스스로 터지면 아무것도 없는 허공에서 탄이 사라진다.
	 */
	if (!HasAuthority())
	{
		return;
	}

	const UAbilitySystemComponent* TargetASC =
		UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(OtherActor);
	if (!IsValid(OtherActor)
		|| OtherActor == this
		|| OtherActor == GetOwner()
		|| !IsValid(TargetASC)
		|| !TargetASC->HasMatchingGameplayTag(GameplayTagSystem::Character_Player))
	{
		// 몬스터, 발사자, 월드는 투사체의 피격/파괴 대상이 아니다.
		// 수명 종료까지 관통하며, 플레이어 태그를 가진 대상만 아래 경로를 탄다.
		return;
	}

	FDamagePayload Payload =
		MonsterDamageLibrary::MakeAttackPayload(AttackTag, DamageEffectClass);

	// 자탄 축소분. 표가 정한 계수 위에 이번 발사만의 사정을 얹는다.
	Payload.BaseDamage *= RuntimeDamageMultiplier;
	FHitResult DamageHit = Hit;
	DamageHit.HitObjectHandle = FActorInstanceHandle(OtherActor);
	Payload.HitResult = DamageHit;

	UDamageSystemLibrary::LogDamageApplied(GetOwner(), OtherActor, TEXT("MonsterProjectile"));

	/*
	 * 가해자는 발사한 몬스터다. 그가 먼저 죽으면 GetOwner()가 무효가 되어
	 * 날아가던 탄이 데미지를 잃는다. 드론과 달리 자체 ASC를 두지 않았다.
	 */
	UDamageSystemLibrary::ApplyGameDamage(this, GetOwner(), Payload);

	// 플레이어 명중일 때만 이번 투사체를 종료한다.
	Destroy();
}
