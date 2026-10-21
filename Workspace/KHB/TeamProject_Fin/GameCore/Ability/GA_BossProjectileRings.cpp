#include "GA_BossProjectileRings.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "GameFramework/Character.h"
#include "GameCore/Tags/GameplayTagSystem.h"

UGA_BossProjectileRings::UGA_BossProjectileRings()
{
	SetAssetTags(FGameplayTagContainer(GameplayTagSystem::Ability_Enemy_Boss_ProjectileRings));
}

void UGA_BossProjectileRings::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
	if (!IsActive()) return;
	NextPlane = InitialPlane;
	LastSpawnedRing.Reset();
	ActiveRings.Reset();
	RingSpawnEventTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, GameplayTagSystem::Event_AI_Boss_RingSpawn, nullptr, false, true);
	if (IsValid(RingSpawnEventTask))
	{
		RingSpawnEventTask->EventReceived.AddDynamic(this, &UGA_BossProjectileRings::OnRingSpawnEvent);
		RingSpawnEventTask->ReadyForActivation();
	}
	RingLaunchEventTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, GameplayTagSystem::Event_AI_Boss_RingLaunch, nullptr, false, true);
	if (IsValid(RingLaunchEventTask))
	{
		RingLaunchEventTask->EventReceived.AddDynamic(this, &UGA_BossProjectileRings::OnRingLaunchEvent);
		RingLaunchEventTask->ReadyForActivation();
	}
}

void UGA_BossProjectileRings::EndAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	const bool bReplicateEndAbility, const bool bWasCancelled)
{
	for (const TWeakObjectPtr<ABossRingWave>& Ring : ActiveRings)
	{
		if (ABossRingWave* RingActor = Ring.Get())
		{
			RingActor->Destroy();
		}
	}

	ActiveRings.Reset();
	LastSpawnedRing.Reset();
	RingSpawnEventTask = nullptr;
	RingLaunchEventTask = nullptr;
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UGA_BossProjectileRings::OnRingSpawnEvent(FGameplayEventData Payload)
{
	SpawnRing(NextPlane);
	NextPlane = NextPlane == EBossRingWavePlane::Horizontal ? EBossRingWavePlane::Vertical : EBossRingWavePlane::Horizontal;
}

void UGA_BossProjectileRings::OnRingLaunchEvent(FGameplayEventData Payload)
{
	if (ABossRingWave* Ring = LastSpawnedRing.Get()) Ring->LaunchRing();
}

void UGA_BossProjectileRings::SpawnRing(EBossRingWavePlane Plane)
{
	const FGameplayAbilityActorInfo* Info = GetCurrentActorInfo();
	ACharacter* Boss = Info ? Cast<ACharacter>(Info->AvatarActor.Get()) : nullptr;
	if (!IsValid(Boss) || !Boss->HasAuthority() || !RingWaveClass) return;
	FActorSpawnParameters Params; Params.Owner = Boss; Params.Instigator = Boss;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ABossRingWave* Ring = Boss->GetWorld()->SpawnActor<ABossRingWave>(RingWaveClass, Boss->GetActorLocation(), FRotator::ZeroRotator, Params);
	if (!IsValid(Ring)) return;
	Ring->InitializeRing(Plane, ChargeRadius, MaxRadius, ChargeExpansionSpeed, LaunchExpansionSpeed, BandHalfWidth, PlaneHalfThickness,
		Boss->GetActorRotation().Yaw,
		RingAttackTag.IsValid() ? RingAttackTag : GameplayTagSystem::Action_AI_Boss_ProjectileRing,
		RingDamageEffectClass);
	LastSpawnedRing = Ring;
	ActiveRings.Add(Ring);
}
