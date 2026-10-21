#include "GA_Evade.h"

#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "GameCore/AI/MonsterAIController.h"
#include "GameCore/Tags/GameplayTagSystem.h"
#include "GameFramework/Character.h"

UGA_Evade::UGA_Evade()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;

	/*
	 * 식별은 Ability.Enemy.* 로 한다. 데미지를 주지 않는 GA라 데미지 키가 없다.
	 * Action.AI.* 는 ST_Enemy_Melee1, ST_Enemy_Range 이(가) 아직 이 태그로 GA를 찾아서 남겨 둔다. 옮기면 지운다.
	 */
	FGameplayTagContainer DefaultAssetTags;
	DefaultAssetTags.AddTag(GameplayTagSystem::Ability_Enemy_Common_Evade);
	DefaultAssetTags.AddTag(GameplayTagSystem::Action_AI_Evade);
	SetAssetTags(DefaultAssetTags);
	ActivationOwnedTags.AddTag(GameplayTagSystem::State_AI_ActionCommitted);
	ActivationOwnedTags.AddTag(GameplayTagSystem::State_AI_SuperArmor);
	ActivationBlockedTags.AddTag(GameplayTagSystem::Cooldown_AI_Evade);
}

void UGA_Evade::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	if (!IsValid(EvadeMontage))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	ACharacter* AvatarCharacter =
		ActorInfo ? Cast<ACharacter>(ActorInfo->AvatarActor.Get()) : nullptr;

	const AMonsterAIController* MonsterController =
		IsValid(AvatarCharacter)
		? Cast<AMonsterAIController>(AvatarCharacter->GetController())
		: nullptr;

	if (!IsValid(MonsterController))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// 쿨다운은 몽타주가 정상 완료된 시점부터 시작하므로 여기서는 코스트만 커밋한다.
	if (!CommitAbilityCost(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// 총성이 오른쪽에서 들리면 왼쪽으로, 왼쪽에서 들리면 오른쪽으로 회피한다.
	const FName StartSection =
		MonsterController->GetGunfireDirectionSign() >= 0.0f
		? LeftSectionName
		: RightSectionName;

	PlayMontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
		this,
		NAME_None,
		EvadeMontage,
		1.0f,
		StartSection);

	if (!IsValid(PlayMontageTask))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	PlayMontageTask->OnCompleted.AddDynamic(this, &UGA_Evade::OnMontageCompleted);
	PlayMontageTask->OnInterrupted.AddDynamic(this, &UGA_Evade::OnMontageInterrupted);
	PlayMontageTask->OnCancelled.AddDynamic(this, &UGA_Evade::OnMontageInterrupted);
	PlayMontageTask->ReadyForActivation();
}

void UGA_Evade::OnMontageCompleted()
{
	// 회피 동작이 끝난 시점부터 Cooldown GE의 전체 지속시간을 적용한다.
	CommitAbilityCooldown(
		CurrentSpecHandle,
		CurrentActorInfo,
		CurrentActivationInfo,
		true);

	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UGA_Evade::OnMontageInterrupted()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

void UGA_Evade::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	PlayMontageTask = nullptr;

	Super::EndAbility(
		Handle,
		ActorInfo,
		ActivationInfo,
		bReplicateEndAbility,
		bWasCancelled);
}
