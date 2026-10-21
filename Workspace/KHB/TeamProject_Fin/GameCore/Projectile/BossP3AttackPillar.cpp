#include "BossP3AttackPillar.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameplayEffect.h"
#include "Net/UnrealNetwork.h"
#include "GameCore/Character/BaseCharacter.h"
#include "GameCore/Tags/GameplayTagSystem.h"

ABossP3AttackPillar::ABossP3AttackPillar()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	SetReplicateMovement(true);

	PillarCollision = CreateDefaultSubobject<UCapsuleComponent>(TEXT("PillarCollision"));
	SetRootComponent(PillarCollision);
	PillarCollision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	PillarCollision->SetCollisionObjectType(ECC_TraceAttack);
	PillarCollision->SetCollisionResponseToAllChannels(ECR_Ignore);
	PillarCollision->SetGenerateOverlapEvents(false);

	PillarMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PillarMesh"));
	PillarMesh->SetupAttachment(PillarCollision);
	PillarMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void ABossP3AttackPillar::BeginPlay()
{
	Super::BeginPlay();
}

void ABossP3AttackPillar::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ABossP3AttackPillar, PillarRadius);
	DOREPLIFETIME(ABossP3AttackPillar, PillarLength);
}

void ABossP3AttackPillar::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (HasAuthority())
	{
		RefreshSlowTargets();
	}
}

void ABossP3AttackPillar::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 수명 종료나 파괴로 EndOverlap이 오지 않는 경우에도 Infinite GE를 남기지 않는다.
	for (const TPair<TObjectPtr<AActor>, FActiveGameplayEffectHandle>& Pair : AppliedSlowEffects)
	{
		if (UAbilitySystemComponent* TargetASC =
			UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Pair.Key.Get()))
		{
			TargetASC->RemoveActiveGameplayEffect(Pair.Value);
		}
	}
	AppliedSlowEffects.Empty();

	Super::EndPlay(EndPlayReason);
}

void ABossP3AttackPillar::InitializePillar(const FVector& Top, const FVector& Bottom, const float Radius)
{
	const FVector Segment = Bottom - Top;
	const float Length = Segment.Length();
	if (Length <= KINDA_SMALL_NUMBER)
	{
		Destroy();
		return;
	}

	const FVector Direction = Segment / Length;
	const FQuat Rotation = FQuat::FindBetweenNormals(FVector::UpVector, Direction);
	PillarRadius = FMath::Max(Radius, 1.0f);
	PillarLength = Length;

	SetActorLocationAndRotation((Top + Bottom) * 0.5f, Rotation, false, nullptr, ETeleportType::TeleportPhysics);
	ApplyPillarDimensions();
}

void ABossP3AttackPillar::OnRep_PillarDimensions()
{
	ApplyPillarDimensions();
}

void ABossP3AttackPillar::ApplyPillarDimensions()
{
	const float SafeRadius = FMath::Max(PillarRadius, 1.0f);
	const float CapsuleHalfHeight = FMath::Max(PillarLength * 0.5f, SafeRadius);

	if (IsValid(PillarCollision))
	{
		PillarCollision->SetCapsuleSize(SafeRadius, CapsuleHalfHeight, true);
	}

	if (IsValid(PillarMesh))
	{
		const float RadiusScale = SafeRadius / FMath::Max(MeshBaseRadius, KINDA_SMALL_NUMBER);
		const float LengthScale = PillarLength / FMath::Max(MeshBaseLength, KINDA_SMALL_NUMBER);
		PillarMesh->SetRelativeScale3D(FVector(RadiusScale, RadiusScale, LengthScale));
	}
}

void ABossP3AttackPillar::RefreshSlowTargets()
{
	if (!IsValid(PillarCollision))
	{
		return;
	}

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(BossPillarSlow), false, this);
	QueryParams.AddIgnoredActor(GetOwner());

	TArray<FOverlapResult> Overlaps;
	GetWorld()->OverlapMultiByChannel(
		Overlaps,
		GetActorLocation(),
		GetActorQuat(),
		ECC_TraceAttack,
		FCollisionShape::MakeCapsule(
			PillarCollision->GetScaledCapsuleRadius(),
			PillarCollision->GetScaledCapsuleHalfHeight()),
		QueryParams);

	TSet<TObjectPtr<AActor>> CurrentPlayers;
	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* Target = Overlap.GetActor();
		UAbilitySystemComponent* TargetASC =
			UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Target);
		if (IsValid(Target)
			&& IsValid(TargetASC)
			&& TargetASC->HasMatchingGameplayTag(GameplayTagSystem::Character_Player))
		{
			CurrentPlayers.Add(Target);
			ApplySlowToPlayer(Target);
		}
	}

	TArray<TObjectPtr<AActor>> ExitedPlayers;
	for (const TPair<TObjectPtr<AActor>, FActiveGameplayEffectHandle>& Pair : AppliedSlowEffects)
	{
		if (!CurrentPlayers.Contains(Pair.Key))
		{
			ExitedPlayers.Add(Pair.Key);
		}
	}

	for (AActor* Target : ExitedPlayers)
	{
		RemoveSlowFromPlayer(Target);
	}
}

void ABossP3AttackPillar::ApplySlowToPlayer(AActor* Target)
{
	if (!IsValid(Target)
		|| AppliedSlowEffects.Contains(Target)
		|| !SlowEffectClass)
	{
		return;
	}

	UAbilitySystemComponent* TargetASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Target);
	UAbilitySystemComponent* SourceASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetOwner());
	if (!IsValid(TargetASC) || !IsValid(SourceASC))
	{
		return;
	}

	FGameplayEffectContextHandle EffectContext = SourceASC->MakeEffectContext();
	EffectContext.AddSourceObject(this);
	const FGameplayEffectSpecHandle SpecHandle = SourceASC->MakeOutgoingSpec(SlowEffectClass, 1.0f, EffectContext);
	if (!SpecHandle.IsValid())
	{
		return;
	}

	const FActiveGameplayEffectHandle EffectHandle = TargetASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
	if (EffectHandle.IsValid())
	{
		AppliedSlowEffects.Add(Target, EffectHandle);
	}
}

void ABossP3AttackPillar::RemoveSlowFromPlayer(AActor* Target)
{
	FActiveGameplayEffectHandle EffectHandle;
	if (!AppliedSlowEffects.RemoveAndCopyValue(Target, EffectHandle))
	{
		return;
	}

	if (UAbilitySystemComponent* TargetASC =
		UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Target))
	{
		TargetASC->RemoveActiveGameplayEffect(EffectHandle);
	}
}
