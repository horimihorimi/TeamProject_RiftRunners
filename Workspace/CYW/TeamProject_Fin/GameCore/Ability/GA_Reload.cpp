// Fill out your copyright notice in the Description page of Project Settings.


#include "GameCore/Ability/GA_Reload.h"
#include "AbilityTask_ReloadSequence.h"
#include "../Tags/GameplayTagSystem.h"
#include "../Weapon/Core/WeaponComponent.h"
#include "../Weapon/Core/WeaponBase.h"
#include "AbilitySystemComponent.h"
#include "Animation/AnimMontage.h"
#include "Engine/Engine.h"

UGA_Reload::UGA_Reload() 
{
	FGameplayTagContainer DefaultAbilityTags;
	DefaultAbilityTags.AddTag(GameplayTagSystem::Ability_Weapon_Reload);
	SetAssetTags(DefaultAbilityTags);

	ActivationOwnedTags.AddTag(GameplayTagSystem::State_Weapon_Reloading);
	ActivationBlockedTags.AddTag(GameplayTagSystem::State_Dead);
	ActivationBlockedTags.AddTag(GameplayTagSystem::State_Weapon_Reloading);
	ActivationBlockedTags.AddTag(GameplayTagSystem::State_Weapon_SmartTracking);
	ActivationBlockedTags.AddTag(GameplayTagSystem::State_Weapon_SmartTrackingVolley);
	ActivationBlockedTags.AddTag(GameplayTagSystem::State_Movement_Dodging);
}

bool UGA_Reload::CanActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayTagContainer* SourceTags,
	const FGameplayTagContainer* TargetTags,
	FGameplayTagContainer* OptionalRelevantTags) const
{
	const UWeaponComponent* WeaponComponent = FindAvatarWeaponComponent(ActorInfo);
	const bool bCanActivate = Super::CanActivateAbility(
		Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags)
		&& ReloadMontage && WeaponComponent && WeaponComponent->CanReload();
	if (!bCanActivate && GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Red, TEXT("[Reload] Failed: Ability activation blocked"));
	}
	return bCanActivate;
}

void UGA_Reload::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData) 
{
	Super::ActivateAbility(
		Handle,
		ActorInfo,
		ActivationInfo,
		TriggerEventData
	);

	bReloadApplied = false;

	UWeaponComponent* WeaponComponent = FindAvatarWeaponComponent(ActorInfo);

	ReloadWeapon = WeaponComponent ? WeaponComponent->GetEquippedWeapon() : nullptr;
	const bool bCanStartReload = ActorInfo && ReloadMontage && ReloadWeapon.IsValid()
		&& WeaponComponent && WeaponComponent->CanReload();
	if (!bCanStartReload)
	{
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Red, TEXT("[Reload] Failed: CanStartReload is false"));
		}
		EndAbility(
			Handle,
			ActorInfo,
			ActivationInfo,
			true,
			true
		);
		return;
	}

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo)) 
	{
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Red, TEXT("[Reload] Failed: CommitAbility"));
		}
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	ReloadSequence = UAbilityTask_ReloadSequence::CreateReloadSequence(this, ReloadMontage, ReloadWeapon.Get());
	if (!ReloadSequence)
	{
		FinishAbility(true);
		return;
	}
	ReloadSequence->OnCommitRequested.AddDynamic(this, &UGA_Reload::OnReloadReady);
	ReloadSequence->OnCancelled.AddDynamic(this, &UGA_Reload::OnReloadCancelled);
	ReloadSequence->ReadyForActivation();
}

void UGA_Reload::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	if (ReloadSequence)
	{
		UAbilityTask_ReloadSequence* Sequence = ReloadSequence;
		ReloadSequence = nullptr;
		Sequence->EndTask();
	}
	ReloadWeapon.Reset();

	Super::EndAbility(
		Handle,
		ActorInfo,
		ActivationInfo,
		bReplicateEndAbility,
		bWasCancelled
	);
}

bool UGA_Reload::TryApplyReload()
{
	if (bReloadApplied)
	{
		return false;
	}

	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	UWeaponComponent* WeaponComponent = FindAvatarWeaponComponent(ActorInfo);

	if (!WeaponComponent || !ReloadWeapon.IsValid()
		|| WeaponComponent->GetEquippedWeapon() != ReloadWeapon.Get())
	{
		return false;
	}

	if (!WeaponComponent->ApplyReload())
	{
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Red, TEXT("[Reload] Failed: ApplyReload"));
		}
		return false;
	}

	bReloadApplied = true;

	UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	AActor* AvatarActor = GetAvatarActor(ActorInfo);

	BroadcastReloadedEvent(ASC, AvatarActor);

	return true;
}

void UGA_Reload::BroadcastReloadedEvent(UAbilitySystemComponent* ASC, AActor* AvatarActor) const
{
	BroadcastSelfGameplayEvent(ASC, AvatarActor, GameplayTagSystem::Event_Weapon_Reloaded);
}

void UGA_Reload::OnReloadReady()
{
	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	if (!ActorInfo || !ActorInfo->IsNetAuthority())
	{
		return;
	}

	TryApplyReload();
	FinishAbility(false);
}

void UGA_Reload::OnReloadCancelled()
{
	FinishAbility(true);
}
