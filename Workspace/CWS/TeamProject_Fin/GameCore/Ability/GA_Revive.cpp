#include "GA_Revive.h"
#include "AbilitySystemComponent.h"
#include "GameCore/Character/BaseCharacter.h"
#include "GameCore/Tags/GameplayTagSystem.h"
#include "GameCore/Attribute/CharacterAttributeSet.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "GameCore/Weapon/Core/WeaponComponent.h"
#include "GameCore/Weapon/Core/WeaponBase.h"

UGA_Revive::UGA_Revive()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerInitiated;

	FAbilityTriggerData TriggerData;
	TriggerData.TriggerTag = GameplayTagSystem::Event_Revive;
	TriggerData.TriggerSource = EGameplayAbilityTriggerSource::GameplayEvent;
	AbilityTriggers.Add(TriggerData);

	ActivationOwnedTags.AddTag(GameplayTagSystem::State_Reviving);
}

void UGA_Revive::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	ABaseCharacter* Character = Cast<ABaseCharacter>(ActorInfo->AvatarActor.Get());
	UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();

	if (!Character || !ASC)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	// Apply GE_Revive is delayed to OnSetupMontageCompleted so State.Dead is maintained during the revive animation

	// Restore Health
	if (UCharacterAttributeSet* AttributeSet = const_cast<UCharacterAttributeSet*>(Character->GetCharacterAttributeSet()))
	{
		float MaxHealth = AttributeSet->GetMaxHealth();
		float HealAmount = MaxHealth * ReviveHealthPercentage;
		AttributeSet->SetHealth(HealAmount);
	}

	if (ReviveMontage)
	{
		UAbilityTask_PlayMontageAndWait* PlayMontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, ReviveMontage);
		if (PlayMontageTask)
		{
			PlayMontageTask->OnCompleted.AddDynamic(this, &UGA_Revive::OnMontageCompleted);
			PlayMontageTask->OnBlendOut.AddDynamic(this, &UGA_Revive::OnMontageCompleted);
			PlayMontageTask->OnInterrupted.AddDynamic(this, &UGA_Revive::OnMontageCompleted);
			PlayMontageTask->OnCancelled.AddDynamic(this, &UGA_Revive::OnMontageCompleted);
			PlayMontageTask->ReadyForActivation();
		}
		else
		{
			EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		}
	}
	else
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
	}

	if (UWeaponComponent* WeaponComp = Character->FindComponentByClass<UWeaponComponent>())
	{
		if (AWeaponBase* Weapon = WeaponComp->GetEquippedWeapon())
		{
			Weapon->SetActorHiddenInGame(true);
		}
	}
}

void UGA_Revive::OnMontageCompleted()
{
	if (ABaseCharacter* Character = Cast<ABaseCharacter>(GetAvatarActorFromActorInfo()))
	{
		// 1. 첫 몽타주(부활)가 끝나면 무기를 다시 보이게 합니다.
		if (UWeaponComponent* WeaponComp = Character->FindComponentByClass<UWeaponComponent>())
		{
			if (AWeaponBase* Weapon = WeaponComp->GetEquippedWeapon())
			{
				Weapon->SetActorHiddenInGame(false);
			}
		}
	}

	// 2. 부활 몽타주의 "setup" 섹션을 실행합니다.
	if (ReviveMontage)
	{
		UAbilityTask_PlayMontageAndWait* PlaySetupTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, ReviveMontage, 1.0f, FName("setup"));
		if (PlaySetupTask)
		{
			PlaySetupTask->OnCompleted.AddDynamic(this, &UGA_Revive::OnSetupMontageCompleted);
			PlaySetupTask->OnBlendOut.AddDynamic(this, &UGA_Revive::OnSetupMontageCompleted);
			PlaySetupTask->OnInterrupted.AddDynamic(this, &UGA_Revive::OnSetupMontageCompleted);
			PlaySetupTask->OnCancelled.AddDynamic(this, &UGA_Revive::OnSetupMontageCompleted);
			PlaySetupTask->ReadyForActivation();
			return; // 셋업 몽타주가 끝날 때까지 어빌리티 종료를 대기합니다.
		}
	}
	
	// 만약 "setup" 실행에 실패하거나 몽타주가 없으면 즉시 종료합니다.
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UGA_Revive::OnSetupMontageCompleted()
{
	if (ABaseCharacter* Character = Cast<ABaseCharacter>(GetAvatarActorFromActorInfo()))
	{
		if (UAbilitySystemComponent* ASC = Character->GetAbilitySystemComponent())
		{
			// 몽타주가 완전히 끝난 뒤에 이펙트를 적용하여 State.Dead 태그를 제거합니다.
			// 이를 통해 부활 몽타주가 재생되는 동안 AnimGraph의 스냅샷(데스 포즈)이 풀리지 않고 완벽하게 블렌딩됩니다.
			if (ReviveEffectClass)
			{
				FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
				Context.AddInstigator(Character, Character);
				FGameplayEffectSpecHandle SpecHandle = ASC->MakeOutgoingSpec(ReviveEffectClass, 1.0f, Context);
				if (SpecHandle.IsValid())
				{
					ASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
				}
			}
		}
	}

	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UGA_Revive::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (ABaseCharacter* Character = Cast<ABaseCharacter>(GetAvatarActorFromActorInfo()))
	{
		if (UWeaponComponent* WeaponComp = Character->FindComponentByClass<UWeaponComponent>())
		{
			if (AWeaponBase* Weapon = WeaponComp->GetEquippedWeapon())
			{
				Weapon->SetActorHiddenInGame(false);
			}
		}
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
