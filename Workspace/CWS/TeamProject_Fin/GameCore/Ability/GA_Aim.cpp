// Copyright Epic Games, Inc. All Rights Reserved.

#include "GA_Aim.h"
#include "AbilitySystemComponent.h"
#include "GameCore/Tags/GameplayTagSystem.h"

UGA_Aim::UGA_Aim()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	ActivationBlockedTags.AddTag(GameplayTagSystem::State_Dead);
}

void UGA_Aim::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();
	if (ASC && AimEffectClass)
	{
		FGameplayEffectContextHandle ContextHandle = ASC->MakeEffectContext();
		ContextHandle.AddInstigator(ActorInfo->AvatarActor.Get(), ActorInfo->AvatarActor.Get());
		
		FGameplayEffectSpecHandle SpecHandle = ASC->MakeOutgoingSpec(AimEffectClass, GetAbilityLevel(), ContextHandle);
		if (SpecHandle.IsValid())
		{
			ActiveAimEffectHandle = ASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
		}
	}
}

void UGA_Aim::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();
	if (ASC && ActiveAimEffectHandle.IsValid())
	{
		ASC->RemoveActiveGameplayEffect(ActiveAimEffectHandle);
		ActiveAimEffectHandle.Invalidate();
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
