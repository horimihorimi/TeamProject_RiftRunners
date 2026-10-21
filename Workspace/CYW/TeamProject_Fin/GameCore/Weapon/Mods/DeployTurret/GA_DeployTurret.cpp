#include "GA_DeployTurret.h"

#include "TurretActor.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameCore/Tags/GameplayTagSystem.h"
#include "GameCore/Weapon/Core/WeaponBase.h"
#include "GameCore/Weapon/Core/WeaponComponent.h"

UGA_DeployTurret::UGA_DeployTurret()
{
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
	bReplicateInputDirectly = false;
	ModCooldownTags.AddTag(GameplayTagSystem::Cooldown_WeaponMod_DeployTurret);
	RequiredWeaponModifier = EWeaponModiferType::DeployTurret;
	ActivationBlockedTags.AddTag(GameplayTagSystem::State_Dead);

	FAbilityTriggerData Trigger;
	Trigger.TriggerTag = GameplayTagSystem::Event_Weapon_ReloadStarted;
	Trigger.TriggerSource = EGameplayAbilityTriggerSource::GameplayEvent;
	AbilityTriggers.Add(Trigger);
	TurretClass = ATurretActor::StaticClass();
}

void UGA_DeployTurret::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	if (ActorInfo->IsNetAuthority() && TriggerEventData
		&& TriggerEventData->EventTag == GameplayTagSystem::Event_Weapon_ReloadStarted
		&& HasRequiredWeaponModifier(ActorInfo))
	{
		OnTriggered();
	}
	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}

void UGA_DeployTurret::OnTriggered()
{
	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	UWeaponComponent* Weapons = FindAvatarWeaponComponent(ActorInfo);
	AWeaponBase* Weapon = GetEquippedWeapon(ActorInfo);
	AActor* Avatar = GetAvatarActor(ActorInfo);
	if (!Weapon || !Weapons || !Avatar || !TurretClass || !Weapons->GetFireDamageEffectClass()
		|| !TurretClass->GetDefaultObject<ATurretActor>()->IsConfigured(Weapon))
	{
		return;
	}
	const FTransform Transform(Avatar->GetActorRotation(), Avatar->GetActorLocation());

	ATurretActor* Turret = GetWorld()->SpawnActorDeferred<ATurretActor>(
		TurretClass, 
		Transform,
		Avatar, 
		Cast<APawn>(Avatar), 
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);

	if (!Turret)
	{
		return;
	}

	if (!CommitAbilityCooldown(CurrentSpecHandle, ActorInfo, CurrentActivationInfo, false))
	{
		Turret->Destroy();
		return;
	}

	const FWeaponRuntimeStats& Stats = Weapon->GetRuntimeStats();
	const int32 SacrificedAmmo = Weapon->SacrificeMagazineAmmo();
	const float SacrificeRatio = static_cast<float>(SacrificedAmmo) / FMath::Max(Stats.MagazineSize, 1);
	const float Damage = Stats.Damage * DamageMultiplier * (1.0f + SacrificeRatio * DamageAmpMultiplier);
	const int32 Ammo = FMath::Max(1, FMath::RoundToInt(Stats.MagazineSize * (1.0f + SacrificeRatio * AmmoCapacityAmpMultiplier)));
	Turret->Initialize(Weapon, 
		GetAbilitySystemComponentFromInfo(ActorInfo), 
		Weapons->GetFireDamageEffectClass(),
		GetAbilityLevel(), 
		Damage, 
		Ammo);
	// Sacrifice happens once on deployment; cancelling the reload does not refund a deployed turret.
	Turret->FinishSpawning(Transform);
}
