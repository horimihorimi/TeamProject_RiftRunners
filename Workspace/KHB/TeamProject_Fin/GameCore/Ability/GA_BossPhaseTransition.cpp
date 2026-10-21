#include "GA_BossPhaseTransition.h"

#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "GameCore/AI/CommonEnemyAIController.h"
#include "GameCore/Character/BossMonsterCharacter.h"
#include "GameCore/Tags/GameplayTagSystem.h"
#include "GameplayEffect.h"

UGA_BossPhaseTransition::UGA_BossPhaseTransition()
{
	SetAssetTags(FGameplayTagContainer(GameplayTagSystem::Ability_Enemy_Boss_Phase2Transition));
	RingAttackTag = GameplayTagSystem::Action_AI_Boss_Phase2Transition_Ring;
}

void UGA_BossPhaseTransition::SetupTransitionSpecificEvents()
{
	// 순간이동과 상승으로 시야를 잃기 전에 붙잡는다. 이후 링 방향은 이 값을 먼저 쓴다.
	CachedRingTarget = nullptr;
	CachedRingTarget = ResolveRingTarget();

	CurrentBurstIndex = 0;
	PendingBurstRings.Reset();
	ActiveTransitionRings.Reset();
	RingSpawnEventTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, GameplayTagSystem::Event_AI_Boss_RingSpawn, nullptr, false, true);
	RingSpawnEventTask->EventReceived.AddDynamic(this, &ThisClass::OnRingSpawnEvent);
	RingSpawnEventTask->ReadyForActivation();
	RingLaunchEventTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, GameplayTagSystem::Event_AI_Boss_RingLaunch, nullptr, false, true);
	RingLaunchEventTask->EventReceived.AddDynamic(this, &ThisClass::OnRingLaunchEvent);
	RingLaunchEventTask->ReadyForActivation();
}

void UGA_BossPhaseTransition::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const bool bReplicateEndAbility, const bool bWasCancelled)
{
	for (const TWeakObjectPtr<ABossRingWave>& Ring : ActiveTransitionRings)
	{
		if (ABossRingWave* RingActor = Ring.Get())
		{
			RingActor->Destroy();
		}
	}
	PendingBurstRings.Reset();
	ActiveTransitionRings.Reset();
	CachedRingTarget = nullptr;
	RingSpawnEventTask = nullptr;
	RingLaunchEventTask = nullptr;
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UGA_BossPhaseTransition::HandleTransitionMoveCompleted()
{
	QueueNextMontageSection(RiseLoopSectionName, RingBurstSectionName);
}

void UGA_BossPhaseTransition::OnRingSpawnEvent(FGameplayEventData)
{
	SpawnCurrentBurst();
}

void UGA_BossPhaseTransition::OnRingLaunchEvent(FGameplayEventData)
{
	for (const TWeakObjectPtr<ABossRingWave>& Ring : PendingBurstRings)
	{
		if (ABossRingWave* RingActor = Ring.Get()) RingActor->LaunchRing();
	}
	PendingBurstRings.Reset();
	++CurrentBurstIndex;
	if (CurrentBurstIndex >= RingBursts.Num()) QueueNextMontageSection(RingBurstLoopSectionName, NextSectionAfterRingBursts);
}

void UGA_BossPhaseTransition::SpawnCurrentBurst()
{
	PendingBurstRings.Reset();
	if (!RingBursts.IsValidIndex(CurrentBurstIndex) || !RingWaveClass || !CurrentActorInfo) return;
	ABossMonsterCharacter* Boss = Cast<ABossMonsterCharacter>(CurrentActorInfo->AvatarActor.Get());
	if (!IsValid(Boss) || !Boss->HasAuthority()) return;
	for (const FPhaseTransitionRingSpec& Spec : RingBursts[CurrentBurstIndex].Rings)
	{
		FActorSpawnParameters Params;
		Params.Owner = Boss;
		Params.Instigator = Boss;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		ABossRingWave* Ring = Boss->GetWorld()->SpawnActor<ABossRingWave>(RingWaveClass, Boss->GetActorLocation(), FRotator::ZeroRotator, Params);
		if (!IsValid(Ring)) continue;
		const bool bVerticalPlane = Spec.Orientation == EBossRingWaveOrientation::TargetFacingVertical || Spec.Orientation == EBossRingWaveOrientation::TargetFacingVerticalSideways;
		const EBossRingWavePlane Plane = bVerticalPlane ? EBossRingWavePlane::Vertical : EBossRingWavePlane::Horizontal;
		Ring->InitializeRingWithRotation(Plane, GetRingRotation(Spec.Orientation), ChargeRadius, MaxRadius,
			ChargeExpansionSpeed, LaunchExpansionSpeed, BandHalfWidth, PlaneHalfThickness,
			RingAttackTag.IsValid() ? RingAttackTag : GameplayTagSystem::Action_AI_Boss_Phase2Transition_Ring,
			RingDamageEffectClass);
		PendingBurstRings.Add(Ring);
		ActiveTransitionRings.Add(Ring);
	}
}

AActor* UGA_BossPhaseTransition::ResolveRingTarget() const
{
	const ABossMonsterCharacter* Boss = CurrentActorInfo ? Cast<ABossMonsterCharacter>(CurrentActorInfo->AvatarActor.Get()) : nullptr;
	const ACommonEnemyAIController* Controller = IsValid(Boss) ? Cast<ACommonEnemyAIController>(Boss->GetController()) : nullptr;
	if (!IsValid(Controller))
	{
		return nullptr;
	}

	// 저장해 둔 타깃이 그 사이 죽거나 은신했으면 버린다. 조건은 컨트롤러의 후보 판정과 같다.
	if (AActor* Cached = CachedRingTarget.Get(); Controller->IsSelectableCombatPlayer(Cached))
	{
		return Cached;
	}

	if (AActor* Current = Controller->GetPerceivedTarget())
	{
		return Current;
	}

	return Controller->FindNearestCombatPlayer();
}

FRotator UGA_BossPhaseTransition::GetRingRotation(const EBossRingWaveOrientation Orientation) const
{
	const ABossMonsterCharacter* Boss = CurrentActorInfo ? Cast<ABossMonsterCharacter>(CurrentActorInfo->AvatarActor.Get()) : nullptr;
	if (!IsValid(Boss) || Orientation == EBossRingWaveOrientation::WorldHorizontal) return FRotator::ZeroRotator;
	const AActor* TargetActor = ResolveRingTarget();
	const FVector ToTarget = IsValid(TargetActor) ? TargetActor->GetActorLocation() - Boss->GetActorLocation() : Boss->GetActorForwardVector();
	if (Orientation == EBossRingWaveOrientation::TargetFacingVertical || Orientation == EBossRingWaveOrientation::TargetFacingVerticalSideways)
	{
		const float YawOffset = Orientation == EBossRingWaveOrientation::TargetFacingVerticalSideways ? 90.0f : 0.0f;
		const FQuat Yaw(FVector::UpVector, FMath::DegreesToRadians(ToTarget.Rotation().Yaw + YawOffset));
		return (Yaw * FQuat(FVector::ForwardVector, HALF_PI)).Rotator();
	}
	const FVector TargetDirection = ToTarget.GetSafeNormal();
	FVector SideAxis = FVector::CrossProduct(FVector::UpVector, TargetDirection).GetSafeNormal();
	if (SideAxis.IsNearlyZero()) SideAxis = Boss->GetActorRightVector().GetSafeNormal();
	const FVector PlaneNormal = FVector::CrossProduct(TargetDirection, SideAxis).GetSafeNormal();
	return FRotationMatrix::MakeFromXZ(TargetDirection, PlaneNormal).Rotator();
}
