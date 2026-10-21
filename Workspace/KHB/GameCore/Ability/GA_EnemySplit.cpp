#include "GA_EnemySplit.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "GameCore/Combat/EnemySplitComponent.h"
#include "GameCore/Tags/GameplayTagSystem.h"
#include "GameFramework/Actor.h"

UGA_EnemySplit::UGA_EnemySplit()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
	SetAssetTags(FGameplayTagContainer(GameplayTagSystem::Ability_Enemy_Split));
	ActivationOwnedTags.AddTag(GameplayTagSystem::State_AI_ActionCommitted);
	ActivationBlockedTags.AddTag(GameplayTagSystem::State_Dead);
	ActivationBlockedTags.AddTag(GameplayTagSystem::State_Groggy);
	ActivationBlockedTags.AddTag(GameplayTagSystem::State_AI_Dormant);
	ActivationBlockedTags.AddTag(GameplayTagSystem::State_AI_ActionCommitted);
}

bool UGA_EnemySplit::CanActivateAbility(const FGameplayAbilitySpecHandle Handle,
                                        const FGameplayAbilityActorInfo* ActorInfo,
                                        const FGameplayTagContainer* SourceTags,
                                        const FGameplayTagContainer* TargetTags,
                                        FGameplayTagContainer* OptionalRelevantTags) const
{
	const AActor* Avatar = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
	const UEnemySplitComponent* Split = Avatar ? Avatar->FindComponentByClass<UEnemySplitComponent>() : nullptr;
	return Split && SplitMontage && Split->IsSplitReady() &&
	       Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags);
}

void UGA_EnemySplit::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
                                     const FGameplayAbilityActorInfo* ActorInfo,
                                     const FGameplayAbilityActivationInfo ActivationInfo,
                                     const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
	if (!SplitMontage || !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	bCommitReceived = false;
	auto* EventTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, GameplayTagSystem::Event_AI_Split_Commit,
	                                                                    nullptr, true, true);
	if (!EventTask)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	EventTask->EventReceived.AddDynamic(this, &ThisClass::OnSplitCommit);
	EventTask->ReadyForActivation();
	auto* Task = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, SplitMontage);
	if (!Task)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	Task->OnCompleted.AddDynamic(this, &ThisClass::OnCompleted);
	Task->OnInterrupted.AddDynamic(this, &ThisClass::OnInterrupted);
	Task->OnCancelled.AddDynamic(this, &ThisClass::OnInterrupted);
	Task->ReadyForActivation();
}

void UGA_EnemySplit::OnSplitCommit(FGameplayEventData Payload)
{
	if (!IsActive() || bCommitReceived)
	{
		return;
	}
	bCommitReceived = true;
	AActor* Avatar = GetAvatarActorFromActorInfo();
	UEnemySplitComponent* Split = IsValid(Avatar) ? Avatar->FindComponentByClass<UEnemySplitComponent>() : nullptr;
	const bool bSuccess = Split && Split->TrySplit();
	// 부모 반환은 다음 틱에 예약되므로 반환 전에 분열 어빌리티를 종료합니다.
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, !bSuccess);
}

void UGA_EnemySplit::OnCompleted()
{
	// 확정 노티파이가 없으면 몽타주가 끝나도 자식을 생성하지 않습니다.
	if (IsActive())
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
	}
}

void UGA_EnemySplit::OnInterrupted()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}
