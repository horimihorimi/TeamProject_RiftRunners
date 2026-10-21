// Copyright Epic Games, Inc. All Rights Reserved.

#include "GA_Crouch.h"
#include "AbilitySystemComponent.h"
#include "GameFramework/Character.h"

UGA_Crouch::UGA_Crouch()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
}

void UGA_Crouch::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// ?�리???�진 기본 ?�기(캡슐 ?�기 변�?�?물리 ?�태 ?�환) ?�출
	ACharacter* Character = Cast<ACharacter>(ActorInfo->AvatarActor.Get());
	if (Character)
	{
		Character->Crouch();
	}

	UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();
	if (ASC && CrouchEffectClass)
	{
		// ?�속 감소??GE(Gameplay Effect) ?�용
		FGameplayEffectContextHandle ContextHandle = ASC->MakeEffectContext();
		ContextHandle.AddInstigator(ActorInfo->AvatarActor.Get(), ActorInfo->AvatarActor.Get());
		
		FGameplayEffectSpecHandle SpecHandle = ASC->MakeOutgoingSpec(CrouchEffectClass, GetAbilityLevel(), ContextHandle);
		if (SpecHandle.IsValid())
		{
			ActiveCrouchEffectHandle = ASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
		}
	}
}

void UGA_Crouch::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	// ?�리???�진 기본 ?�기 ?�제
	ACharacter* Character = Cast<ACharacter>(ActorInfo->AvatarActor.Get());
	if (Character)
	{
		Character->UnCrouch();
	}

	UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();
	if (ASC && ActiveCrouchEffectHandle.IsValid())
	{
		// ?�빌리티 종료 ???�기 ?�과(GE) ?�거
		ASC->RemoveActiveGameplayEffect(ActiveCrouchEffectHandle);
		ActiveCrouchEffectHandle.Invalidate();
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
