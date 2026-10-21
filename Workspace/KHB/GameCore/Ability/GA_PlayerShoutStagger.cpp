#include "GameCore/Ability/GA_PlayerShoutStagger.h"

#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "GameCore/Tags/GameplayTagSystem.h"

UGA_PlayerShoutStagger::UGA_PlayerShoutStagger()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerInitiated;
	AbilityTags.AddTag(GameplayTagSystem::Ability_Player_ShoutStagger);

	FAbilityTriggerData TriggerData;
	TriggerData.TriggerTag = GameplayTagSystem::State_Debuff_Staggered;
	TriggerData.TriggerSource = EGameplayAbilityTriggerSource::OwnedTagAdded;
	AbilityTriggers.Add(TriggerData);

	// 경직이 시작되는 즉시 진행 중인 행동을 중단합니다.
	CancelAbilitiesWithTag.AddTag(GameplayTagSystem::Ability_Weapon_Fire);
	CancelAbilitiesWithTag.AddTag(GameplayTagSystem::Ability_Weapon_Aim);
	CancelAbilitiesWithTag.AddTag(GameplayTagSystem::Ability_Weapon_Reload);
	CancelAbilitiesWithTag.AddTag(GameplayTagSystem::Ability_Skill_Main);
	CancelAbilitiesWithTag.AddTag(GameplayTagSystem::Ability_Movement_Dodge);
	CancelAbilitiesWithTag.AddTag(GameplayTagSystem::Ability_Movement_Sprint);

	// 태그가 제거될 때까지 같은 행동의 재시도를 막습니다.
	BlockAbilitiesWithTag.AddTag(GameplayTagSystem::Ability_Weapon_Fire);
	BlockAbilitiesWithTag.AddTag(GameplayTagSystem::Ability_Weapon_Aim);
	BlockAbilitiesWithTag.AddTag(GameplayTagSystem::Ability_Weapon_Reload);
	BlockAbilitiesWithTag.AddTag(GameplayTagSystem::Ability_Skill_Main);
	BlockAbilitiesWithTag.AddTag(GameplayTagSystem::Ability_Movement_Dodge);
	BlockAbilitiesWithTag.AddTag(GameplayTagSystem::Ability_Movement_Sprint);
}

void UGA_PlayerShoutStagger::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	/*
	 * 태그 확인은 서버만 한다.
	 *
	 * 서버는 태그가 붙어서 이 GA를 발동시켰으므로 확실히 들고 있다. 클라이언트에는 활성화 RPC가
	 * 태그(GE) 복제보다 먼저 도착할 수 있는데, 거기서 태그가 없다고 끝내면 그 종료가 서버로 전달되어
	 * 서버 GA와 경직 몽타주까지 함께 끊긴다.
	 */
	UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	const bool bIsAuthority = ActorInfo && ActorInfo->IsNetAuthority();
	if (!IsValid(ASC)
		|| (bIsAuthority && !ASC->HasMatchingGameplayTag(GameplayTagSystem::State_Debuff_Staggered)))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("[ShoutStagger] PlayerAbilityActivated | Player=%s | Authority=%s | HasStaggeredTag=%s | Montage=%s"),
		*GetNameSafe(GetAvatarActorFromActorInfo()),
		bIsAuthority ? TEXT("True") : TEXT("False"),
		ASC->HasMatchingGameplayTag(GameplayTagSystem::State_Debuff_Staggered) ? TEXT("True") : TEXT("False"),
		*GetNameSafe(StaggerMontage));

	BoundAbilitySystemComponent = ASC;
	ASC->RegisterGameplayTagEvent(
		GameplayTagSystem::State_Debuff_Staggered,
		EGameplayTagEventType::NewOrRemoved).AddUObject(this, &ThisClass::OnStaggeredTagChanged);

	// 경직 시간의 기준은 이 몽타주입니다. 끝나면 Staggered GE도 즉시 제거합니다.
	if (StaggerMontage)
	{
		StaggerMontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
			this, TEXT("ShoutStaggerMontage"), StaggerMontage, StaggerMontagePlayRate);
		if (StaggerMontageTask)
		{
			StaggerMontageTask->OnCompleted.AddDynamic(this, &ThisClass::OnStaggerMontageFinished);
			StaggerMontageTask->OnInterrupted.AddDynamic(this, &ThisClass::OnStaggerMontageFinished);
			StaggerMontageTask->OnCancelled.AddDynamic(this, &ThisClass::OnStaggerMontageFinished);
			StaggerMontageTask->ReadyForActivation();
		}
	}
}

void UGA_PlayerShoutStagger::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const bool bReplicateEndAbility,
	const bool bWasCancelled)
{
	if (UAbilitySystemComponent* ASC = BoundAbilitySystemComponent.Get())
	{
		ASC->RegisterGameplayTagEvent(
			GameplayTagSystem::State_Debuff_Staggered,
			EGameplayTagEventType::NewOrRemoved).RemoveAll(this);
	}
	BoundAbilitySystemComponent = nullptr;
	StaggerMontageTask = nullptr;

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UGA_PlayerShoutStagger::OnStaggerMontageFinished()
{
	UE_LOG(LogTemp, Warning, TEXT("[ShoutStagger] PlayerMontageFinished | Player=%s | Authority=%s"),
		*GetNameSafe(GetAvatarActorFromActorInfo()),
		CurrentActorInfo && CurrentActorInfo->IsNetAuthority() ? TEXT("True") : TEXT("False"));
	if (UAbilitySystemComponent* ASC = BoundAbilitySystemComponent.Get())
	{
		FGameplayTagContainer StaggeredTags;
		StaggeredTags.AddTag(GameplayTagSystem::State_Debuff_Staggered);
		ASC->RemoveActiveEffectsWithGrantedTags(StaggeredTags);
	}
}

void UGA_PlayerShoutStagger::OnStaggeredTagChanged(const FGameplayTag Tag, const int32 NewCount)
{
	if (NewCount == 0 && IsActive())
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
	}
}
