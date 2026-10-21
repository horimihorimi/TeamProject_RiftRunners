#include "GA_BossWhackTwist.h"

#include "GameCore/Character/BossMonsterCharacter.h"
#include "GameCore/Tags/GameplayTagSystem.h"
#include "MotionWarpingComponent.h"

UGA_BossWhackTwist::UGA_BossWhackTwist()
{
	DefaultDamageAttackTag = GameplayTagSystem::Action_AI_Boss_WhackTwist;
	SetAssetTags(FGameplayTagContainer(GameplayTagSystem::Ability_Enemy_Boss_WhackTwist));
}

void UGA_BossWhackTwist::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	if (!RegisterAnchorWarpTarget(ActorInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
}

void UGA_BossWhackTwist::EndAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	const bool bReplicateEndAbility, const bool bWasCancelled)
{
	ClearAnchorWarpTarget();
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

bool UGA_BossWhackTwist::RegisterAnchorWarpTarget(const FGameplayAbilityActorInfo* ActorInfo)
{
	ABossMonsterCharacter* Boss = ActorInfo ? Cast<ABossMonsterCharacter>(ActorInfo->AvatarActor.Get()) : nullptr;
	AActor* Anchor = IsValid(Boss) ? Boss->GetArenaCenter() : nullptr;
	UMotionWarpingComponent* MotionWarping = IsValid(Boss) ? Boss->GetMotionWarpingComponent() : nullptr;
	if (!IsValid(Boss) || !IsValid(Anchor) || !IsValid(MotionWarping) || WarpTargetName.IsNone())
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[BossWhackTwist] Anchor warp setup failed | Boss=%s Anchor=%s MotionWarping=%s TargetName=%s"),
			*GetNameSafe(Boss), *GetNameSafe(Anchor), *GetNameSafe(MotionWarping), *WarpTargetName.ToString());
		return false;
	}

	FVector TargetLocation = Anchor->GetActorLocation();
	TargetLocation.Z += AnchorHeightFromFloor;
	MotionWarping->AddOrUpdateWarpTargetFromTransform(
		WarpTargetName,
		FTransform(Boss->GetActorRotation(), TargetLocation));

	RegisteredMotionWarpingComponent = MotionWarping;
	bWarpTargetRegistered = true;
	return true;
}

void UGA_BossWhackTwist::ClearAnchorWarpTarget()
{
	if (bWarpTargetRegistered)
	{
		if (UMotionWarpingComponent* MotionWarping = RegisteredMotionWarpingComponent.Get())
		{
			MotionWarping->RemoveWarpTarget(WarpTargetName);
		}
	}

	RegisteredMotionWarpingComponent = nullptr;
	bWarpTargetRegistered = false;
}
