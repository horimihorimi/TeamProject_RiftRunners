#include "GA_BossGroundSlam.h"

#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "DrawDebugHelpers.h"
#include "GameCore/Combat/MonsterDamageLibrary.h"
#include "GameCore/Projectile/BossRingWave.h"
#include "GameCore/Tags/GameplayTagSystem.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"

UGA_BossGroundSlam::UGA_BossGroundSlam()
{
	ImpactAttackTag = GameplayTagSystem::Action_AI_Boss_GroundSlam_Impact;
	RingAttackTag = GameplayTagSystem::Action_AI_Boss_GroundSlam_Ring;
	SetAssetTags(FGameplayTagContainer(GameplayTagSystem::Ability_Enemy_Boss_GroundSlam));
}

void UGA_BossGroundSlam::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
	if (!IsActive()) return;
	bHasAppliedImpactDamage = false;
	ActiveRing.Reset();

	ImpactEventTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, GameplayTagSystem::Event_AI_Boss_GroundSlam_Impact, nullptr, false, true);
	if (IsValid(ImpactEventTask))
	{
		ImpactEventTask->EventReceived.AddDynamic(this, &UGA_BossGroundSlam::OnImpactEvent);
		ImpactEventTask->ReadyForActivation();
	}

	RetractEventTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, GameplayTagSystem::Event_AI_Boss_GroundSlam_Retract, nullptr, false, true);
	if (IsValid(RetractEventTask))
	{
		RetractEventTask->EventReceived.AddDynamic(this, &UGA_BossGroundSlam::OnRetractEvent);
		RetractEventTask->ReadyForActivation();
	}
}

void UGA_BossGroundSlam::EndAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	const bool bReplicateEndAbility, const bool bWasCancelled)
{
	if (ABossRingWave* Ring = ActiveRing.Get())
	{
		Ring->Destroy();
	}

	ActiveRing.Reset();
	ImpactEventTask = nullptr;
	RetractEventTask = nullptr;
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UGA_BossGroundSlam::OnImpactEvent(FGameplayEventData Payload)
{
	AActor* Avatar = GetAvatarActorFromActorInfo();
	if (!IsValid(Avatar) || !Avatar->HasAuthority()) return;

	// 링과 별개인 내려찍기 본체 충격. 같은 이벤트가 중복 전송되어도 한 번만 적용한다.
	if (!bHasAppliedImpactDamage)
	{
		ApplyImpactDamage(Avatar);
		bHasAppliedImpactDamage = true;
	}

	if (!GroundSlamRingClass || ActiveRing.IsValid()) return;

	FActorSpawnParameters Params;
	Params.Owner = Avatar;
	Params.Instigator = Cast<APawn>(Avatar);
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	const FVector RingSpawnLocation = Avatar->GetActorLocation() + FVector::UpVector * RingZOffset;
	ABossRingWave* Ring = Avatar->GetWorld()->SpawnActor<ABossRingWave>(
		GroundSlamRingClass, RingSpawnLocation, FRotator::ZeroRotator, Params);
	if (!IsValid(Ring)) return;

	Ring->InitializeGroundSlamRing(RingMaxRadius, RingExpandSpeed, RingRetractSpeed,
		RingBandHalfWidth, RingBandHalfWidth,
		RingAttackTag.IsValid() ? RingAttackTag : GameplayTagSystem::Action_AI_Boss_GroundSlam_Ring,
		RingDamageEffectClass);
	ActiveRing = Ring;
}

void UGA_BossGroundSlam::ApplyImpactDamage(AActor* Avatar)
{
	if (!IsValid(Avatar) || ImpactRadius <= 0.0f || !ImpactDamageEffectClass || !ImpactAttackTag.IsValid())
	{
		return;
	}

	const FVector Origin = Avatar->GetActorLocation();
	if (bDebugDrawImpact)
	{
		DrawDebugSphere(
			Avatar->GetWorld(), Origin, ImpactRadius,
			24, FColor::Orange, false, DebugImpactDrawDuration, 0, 2.0f);
	}

	TMap<AActor*, FHitResult> DamagedHits;
	MonsterDamageLibrary::GatherTargets(Avatar, Origin, Origin, ImpactRadius, { Avatar }, DamagedHits);
	MonsterDamageLibrary::ApplyDamageToTargets(
		Avatar,
		MonsterDamageLibrary::MakeAttackPayload(ImpactAttackTag, ImpactDamageEffectClass),
		DamagedHits,
		TEXT("GroundSlamImpact"));
}

void UGA_BossGroundSlam::OnRetractEvent(FGameplayEventData Payload)
{
	if (ABossRingWave* Ring = ActiveRing.Get()) Ring->BeginRetract();
}
