// Copyright Epic Games, Inc. All Rights Reserved.

#include "GA_Sprint.h"
#include "AbilitySystemComponent.h"
#include "../Tags/GameplayTagSystem.h"
#include "../Attribute/CharacterAttributeSet.h"

UGA_Sprint::UGA_Sprint()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	AbilityTags.AddTag(GameplayTagSystem::Ability_Movement_Sprint);
	ActivationOwnedTags.AddTag(GameplayTagSystem::State_Movement_Sprinting);
	ActivationBlockedTags.AddTag(GameplayTagSystem::State_Dead);
	ActivationBlockedTags.AddTag(GameplayTagSystem::State_Debuff_CannotSprint);
}

void UGA_Sprint::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();

	// 스태미나가 0 이하면 달리기 시작 불가
	if (ASC)
	{
		float CurrentStamina = ASC->GetNumericAttribute(UCharacterAttributeSet::GetStaminaAttribute());
		if (CurrentStamina <= 0.0f)
		{
			EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
			return;
		}
	}

	// 코스트(스태미나 등) 및 쿨다운 검사 후 커밋
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (ASC)
	{
		// 달리기 시작 시 진행 중인 사격 취소
		FGameplayTagContainer FireTags;
		FireTags.AddTag(GameplayTagSystem::Ability_Weapon_Fire);

		ASC->CancelAbilities(&FireTags);
	}

	if (ASC && SprintEffectClass)
	{
		// 이속 증가 및 태그 부여용 GE(Gameplay Effect) 적용
		FGameplayEffectContextHandle ContextHandle = ASC->MakeEffectContext();
		ContextHandle.AddInstigator(ActorInfo->AvatarActor.Get(), ActorInfo->AvatarActor.Get());
		
		FGameplayEffectSpecHandle SpecHandle = ASC->MakeOutgoingSpec(SprintEffectClass, GetAbilityLevel(), ContextHandle);
		if (SpecHandle.IsValid())
		{
			ActiveSprintEffectHandle = ASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
		}

		if (SprintDrainEffectClass)
		{
			FGameplayEffectSpecHandle DrainSpecHandle = ASC->MakeOutgoingSpec(SprintDrainEffectClass, GetAbilityLevel(), ContextHandle);
			if (DrainSpecHandle.IsValid())
			{
				ActiveSprintDrainEffectHandle = ASC->ApplyGameplayEffectSpecToSelf(*DrainSpecHandle.Data.Get());
			}
		}

		// 스태미나가 0이 되는지 감지하기 위해 델리게이트 바인딩
		StaminaChangeDelegateHandle = ASC->GetGameplayAttributeValueChangeDelegate(UCharacterAttributeSet::GetStaminaAttribute()).AddUObject(this, &UGA_Sprint::OnStaminaChanged);
	}
}

void UGA_Sprint::OnStaminaChanged(const FOnAttributeChangeData& Data)
{
	if (Data.NewValue <= 0.0f)
	{
		// 스태미나가 고갈되면 달리기 강제 종료 (취소)
		CancelAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true);
	}
}

void UGA_Sprint::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();
	if (ASC)
	{
		// 이 어빌리티가 정상적으로 실행되어 GE가 붙어있었는지 확인
		bool bWasActuallySprinting = ActiveSprintEffectHandle.IsValid();

		if (ActiveSprintEffectHandle.IsValid())
		{
			// 어빌리티 취소/종료 시 달리기 효과(GE) 제거 -> 즉시 기본 걷기 속도 및 태그로 복귀
			ASC->RemoveActiveGameplayEffect(ActiveSprintEffectHandle);
			ActiveSprintEffectHandle.Invalidate();
		}

		if (ActiveSprintDrainEffectHandle.IsValid())
		{
			ASC->RemoveActiveGameplayEffect(ActiveSprintDrainEffectHandle);
			ActiveSprintDrainEffectHandle.Invalidate();
		}

		// 달리기를 실제로 수행했던 경우에만 스태미나 회복 지연(Pause) 이펙트 발동
		if (bWasActuallySprinting && SprintRegenPauseEffectClass)
		{
			FGameplayEffectContextHandle ContextHandle = ASC->MakeEffectContext();
			ContextHandle.AddInstigator(ActorInfo->AvatarActor.Get(), ActorInfo->AvatarActor.Get());
			
			FGameplayEffectSpecHandle PauseSpecHandle = ASC->MakeOutgoingSpec(SprintRegenPauseEffectClass, GetAbilityLevel(), ContextHandle);
			if (PauseSpecHandle.IsValid())
			{
				ASC->ApplyGameplayEffectSpecToSelf(*PauseSpecHandle.Data.Get());
			}
		}

		if (StaminaChangeDelegateHandle.IsValid())
		{
			ASC->GetGameplayAttributeValueChangeDelegate(UCharacterAttributeSet::GetStaminaAttribute()).Remove(StaminaChangeDelegateHandle);
			StaminaChangeDelegateHandle.Reset();
		}
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
