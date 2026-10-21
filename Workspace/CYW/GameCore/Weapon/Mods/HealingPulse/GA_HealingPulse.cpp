#include "GA_HealingPulse.h"

#include "GameCore/Ability/CombatTargetLibrary.h"
#include "GameCore/Attribute/CharacterAttributeSet.h"
#include "GameCore/Tags/GameplayTagSystem.h"
#include "GameCore/Weapon/Core/WeaponBase.h"
#include "GameCore/Weapon/Core/WeaponComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"

UGA_HealingPulse::UGA_HealingPulse()
{
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
	bReplicateInputDirectly = false;
	ModCooldownTags.AddTag(GameplayTagSystem::Cooldown_WeaponMod_HealingPulse);
	RequiredWeaponModifier = EWeaponModiferType::HealingNova;
	FGameplayTagContainer DefaultAbilityTags;
	DefaultAbilityTags.AddTag(GameplayTagSystem::Ability_Weapon_HealingPulse);
	SetAssetTags(DefaultAbilityTags);
	ActivationBlockedTags.AddTag(GameplayTagSystem::State_Dead);

	FAbilityTriggerData Trigger;
	Trigger.TriggerTag = GameplayTagSystem::Event_Combat_DamageDealt;
	Trigger.TriggerSource = EGameplayAbilityTriggerSource::GameplayEvent;
	AbilityTriggers.Add(Trigger);

	PulseCueTag = GameplayTagSystem::GameplayCue_Weapon_HealingPulse;
}

void UGA_HealingPulse::OnAvatarSet(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec)
{
	Super::OnAvatarSet(ActorInfo, Spec);
	UnbindWeapon();
	if (!ActorInfo->IsNetAuthority())
	{
		return;
	}

	UWeaponComponent* Weapons = FindAvatarWeaponComponent(ActorInfo);
	BoundWeapons = Weapons;
	if (Weapons)
	{
		Weapons->OnWeaponEquipped.AddDynamic(this, &UGA_HealingPulse::OnWeaponChanged);
	}
	OnWeaponChanged(Weapons ? Weapons->GetEquippedWeapon() : nullptr);
}

void UGA_HealingPulse::OnRemoveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec)
{
	UnbindWeapon();
	ResetCharge();
	Super::OnRemoveAbility(ActorInfo, Spec);
}

void UGA_HealingPulse::UnbindWeapon()
{
	if (UWeaponComponent* Weapons = BoundWeapons.Get())
	{
		Weapons->OnWeaponEquipped.RemoveDynamic(this, &UGA_HealingPulse::OnWeaponChanged);
	}
	if (AWeaponBase* Weapon = BoundWeapon.Get())
	{
		Weapon->OnModifiersChanged.Remove(ModifiersChangedHandle);
	}

	BoundWeapons.Reset();
	BoundWeapon.Reset();
	ModifiersChangedHandle.Reset();
}

void UGA_HealingPulse::OnWeaponChanged(AWeaponBase* NewWeapon)
{
	if (AWeaponBase* Weapon = BoundWeapon.Get())
	{
		Weapon->OnModifiersChanged.Remove(ModifiersChangedHandle);
	}
	BoundWeapon = NewWeapon;
	ModifiersChangedHandle.Reset();
	if (NewWeapon)
	{
		ModifiersChangedHandle = NewWeapon->OnModifiersChanged.AddUObject(this, &UGA_HealingPulse::ResetCharge);
	}
	ResetCharge();
}

void UGA_HealingPulse::ResetCharge()
{
	DebugMessage(FString::Printf(TEXT("damage reset (%.1f -> 0), cooldown cleared"), AccumulatedDamage));
	AccumulatedDamage = 0.0f;
	ClearModCooldown();
}

void UGA_HealingPulse::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	AActor* AvatarActor = GetAvatarActor(ActorInfo);
	UAbilitySystemComponent* SourceASC = GetAbilitySystemComponentFromInfo(ActorInfo);
	// Count actual hostile damage, including the killing blow; healing/self-damage cannot charge it.
	if (ActorInfo->IsNetAuthority() && TriggerEventData && TriggerEventData->EventMagnitude > 0.0f
		&& HasRequiredWeaponModifier(ActorInfo)
		&& CombatTargetLibrary::IsHostileTarget(TriggerEventData->Target.Get(), SourceASC, AvatarActor, true))
	{
		AccumulatedDamage += TriggerEventData->EventMagnitude;
		DebugMessage(FString::Printf(TEXT("damage %.1f / %.1f"),
			AccumulatedDamage, DamageThreshold));
		if (AccumulatedDamage >= DamageThreshold && HealEffectClass)
		{
			TriggerPulse(SourceASC, AvatarActor);
		}
	}
	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}

void UGA_HealingPulse::TriggerPulse(UAbilitySystemComponent* SourceASC, AActor* AvatarActor)
{
	const FVector Origin = AvatarActor->GetActorLocation();
	FGameplayEffectSpecHandle Heal = MakeOutgoingGameplayEffectSpec(HealEffectClass, GetAbilityLevel());
	if (!Heal.IsValid() || !CommitAbilityCooldown(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, false))
	{
		return;
	}
	Heal.Data->SetSetByCallerMagnitude(GameplayTagSystem::Data_Heal, HealAmount);

	// Reset before applying effects: callbacks must not trigger another pulse.
	AccumulatedDamage = 0.0f;
	int32 HealedPlayers = 0;
	// Player controllers avoid duplicate skeletal/capsule overlaps and exclude friendly NPCs.
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		const APlayerController* PlayerController = It->Get();
		APawn* Player = PlayerController ? PlayerController->GetPawn() : nullptr;
		if (!Player || FVector::DistSquared(Origin, Player->GetActorLocation()) > FMath::Square(HealRadius))
		{
			continue;
		}

		UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Player);
		if (!CombatTargetLibrary::AreFriendly(SourceASC, TargetASC)
			|| TargetASC->HasMatchingGameplayTag(GameplayTagSystem::State_Dead)
			|| TargetASC->GetNumericAttribute(UCharacterAttributeSet::GetHealthAttribute()) <= 0.0f)
		{
			continue;
		}
		SourceASC->ApplyGameplayEffectSpecToTarget(*Heal.Data.Get(), TargetASC);
		++HealedPlayers;
	}
	FGameplayCueParameters CueParameters;
	CueParameters.Instigator = AvatarActor;
	CueParameters.EffectCauser = AvatarActor;
	CueParameters.Location = Origin;
	CueParameters.TargetAttachComponent = AvatarActor->GetRootComponent();
	CueParameters.RawMagnitude = HealRadius;
	SourceASC->ExecuteGameplayCue(PulseCueTag, CueParameters);
	if (bShowDebug)
	{
		DrawDebugSphere(GetWorld(), Origin, HealRadius, 32, FColor::Green, false, 2.0f);
	}
	DebugMessage(FString::Printf(TEXT("triggered: %d players, heal %.1f, damage reset to 0"), HealedPlayers, HealAmount));
}

void UGA_HealingPulse::DebugMessage(const FString& Message) const
{
	if (bShowDebug && GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Green, TEXT("[HealingPulse] ") + Message);
	}
}

