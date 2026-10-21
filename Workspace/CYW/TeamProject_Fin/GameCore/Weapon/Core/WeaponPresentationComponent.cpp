#include "WeaponPresentationComponent.h"

#include "WeaponComponent.h"
#include "WeaponBase.h"
#include "../../Ability/GA_Fire.h"
#include "../../Combat/CombatFeedbackComponent.h"
#include "../../Tags/GameplayTagSystem.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "GameplayCueManager.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

UWeaponPresentationComponent::UWeaponPresentationComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UWeaponPresentationComponent::Initialize(UWeaponComponent* InWeapons)
{
	Weapons = InWeapons;
	BindReloadSoundEvents();
}

void UWeaponPresentationComponent::PlayEquipSound(AWeaponBase* EquippedWeapon)
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	// Starting weapons use raw DataAssets; pickups (including swapped-out starters) use generated data.
	if (!Pawn || !Pawn->IsLocallyControlled() || !EquippedWeapon || !EquippedWeapon->IsGeneratedWeapon())
	{
		return;
	}

	const UWeaponDataAsset* WeaponData = EquippedWeapon->GetWeaponData();
	if (WeaponData)
	{
		const FWeaponResolvedSound Sound = WeaponData->EquipSound.Resolve();
		if (Sound.IsValid())
		{
			UGameplayStatics::PlaySound2D(this, Sound.Sound, Sound.Volume, Sound.Pitch);
		}
	}
}

UAnimMontage* UWeaponPresentationComponent::ResolveMontage(const FGameplayTag& CueTag) const
{
	UAnimMontage* Montage = nullptr;
	if (CueTag == GameplayTagSystem::GameplayCue_Weapon_Fire)
	{
		if (UAbilitySystemComponent* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(GetOwner()))
		{
			for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
			{
				const UGA_Fire* FireAbility = Cast<UGA_Fire>(Spec.Ability);
				if (FireAbility)
				{
					Montage = FireAbility->GetFireMontage();
					break;
				}
			}
		}
	}
	return Montage;
}

void UWeaponPresentationComponent::PlayConfirmedCue(FGameplayTag CueTag, FGameplayCueParameters Parameters,
	UAnimMontage* Montage, AWeaponBase* EquippedWeapon)
{
	AWeaponBase* ShotWeapon = Cast<AWeaponBase>(Parameters.EffectCauser.Get());
	const bool bCurrentWeapon = IsValid(ShotWeapon) && ShotWeapon == EquippedWeapon;
	if (!bCurrentWeapon)
	{
		// Keep world-space effects, but never attach an old shot to a new weapon.
		Parameters.EffectCauser.Reset();
		Parameters.TargetAttachComponent.Reset();
	}

	if (CueTag == GameplayTagSystem::GameplayCue_Weapon_Fire && bCurrentWeapon)
	{
		ACharacter* Character = Cast<ACharacter>(GetOwner());
		UAbilitySystemComponent* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(GetOwner());
		UAnimInstance* Anim = Character && Character->GetMesh() ? Character->GetMesh()->GetAnimInstance() : nullptr;
		const bool bBlocked = ASC && (
			ASC->HasMatchingGameplayTag(GameplayTagSystem::State_Dead)
			|| ASC->HasMatchingGameplayTag(GameplayTagSystem::State_Weapon_Reloading)
			|| ASC->HasMatchingGameplayTag(GameplayTagSystem::State_Movement_Dodging)
			|| ASC->HasMatchingGameplayTag(GameplayTagSystem::State_Movement_Sprinting)
			|| ASC->HasMatchingGameplayTag(GameplayTagSystem::State_Skill_Bomber_MineThrowing));
		const bool bOtherMontage = Anim && Anim->GetCurrentActiveMontage()
			&& Anim->GetCurrentActiveMontage() != Montage;
		if (!bBlocked && !bOtherMontage)
		{
			if (Anim && Montage)
			{
				// Cosmetic playback only: ASC montage replication would deliver this twice.
				Anim->Montage_Play(Montage);
			}
			if (Character && Character->IsLocallyControlled())
			{
				if (APlayerController* PC = Cast<APlayerController>(Character->GetController()))
				{
					if (UCombatFeedbackComponent* Feedback = PC->FindComponentByClass<UCombatFeedbackComponent>())
					{
						Feedback->ApplyWeaponRecoil(ShotWeapon->GetRuntimeStats(), ASC);
					}
				}
			}
		}
	}

	// This RPC is the delivery path. Handle the cue locally without GAS prediction suppression.
	if (UGameplayCueManager* Manager = UAbilitySystemGlobals::Get().GetGameplayCueManager())
	{
		Manager->HandleGameplayCue(GetOwner(), CueTag, EGameplayCueEvent::Executed, Parameters);
	}
}

void UWeaponPresentationComponent::BindReloadSoundEvents()
{
	BindReloadSoundEvent(GameplayTagSystem::Event_Weapon_ReloadSound_Start);
	BindReloadSoundEvent(GameplayTagSystem::Event_Weapon_ReloadSound_Insert);
	BindReloadSoundEvent(GameplayTagSystem::Event_Weapon_ReloadCommit);
}

void UWeaponPresentationComponent::BindReloadSoundEvent(const FGameplayTag& EventTag)
{
	UAbilitySystemComponent* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(GetOwner());
	if (!ASC || !EventTag.IsValid())
	{
		return;
	}

	FGameplayEventMulticastDelegate& Delegate = ASC->GenericGameplayEventCallbacks.FindOrAdd(EventTag);
	Delegate.RemoveAll(this);
	Delegate.AddUObject(this, &UWeaponPresentationComponent::OnReloadSoundEvent);
}

void UWeaponPresentationComponent::OnReloadSoundEvent(const FGameplayEventData* Payload)
{
	AWeaponBase* EquippedWeapon = Weapons.IsValid() ? Weapons->GetEquippedWeapon() : nullptr;
	if (!Payload || !EquippedWeapon)
	{
		return;
	}

	if (Payload->EventTag == GameplayTagSystem::Event_Weapon_ReloadSound_Start)
	{
		EquippedWeapon->PlayReloadSound(EWeaponReloadSoundPhase::Start);
	}
	else if (Payload->EventTag == GameplayTagSystem::Event_Weapon_ReloadSound_Insert)
	{
		EquippedWeapon->PlayReloadSound(EWeaponReloadSoundPhase::Insert);
	}
	else if (Payload->EventTag == GameplayTagSystem::Event_Weapon_ReloadCommit)
	{
		EquippedWeapon->PlayReloadSound(EWeaponReloadSoundPhase::Complete);
	}
}
