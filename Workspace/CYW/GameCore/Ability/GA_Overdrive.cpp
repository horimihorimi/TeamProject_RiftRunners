#include "GA_Overdrive.h"
#include "OverdriveDischarge.h"

#include "CombatTargetLibrary.h"
#include "../Tags/GameplayTagSystem.h"
#include "../Effect/GE_WeaponTimingModifier.h"
#include "../Weapon/Core/WeaponBase.h"
#include "../Weapon/Core/WeaponComponent.h"
#include "../Weapon/Libraries/WeaponCueLibrary.h"
#include "GameplayCues/GameplayCueFxLibrary.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Engine/World.h"

UGA_Overdrive::UGA_Overdrive()
{
	ActivationOwnedTags.AddTag(GameplayTagSystem::State_Skill_Overdrive);
	ActivationBlockedTags.AddTag(GameplayTagSystem::State_Skill_Overdrive);
}

void UGA_Overdrive::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	if (!DurationEffectClass || Duration <= 0.0f
		|| DurationEffectClass->GetDefaultObject<UGameplayEffect>()->DurationPolicy != EGameplayEffectDurationType::HasDuration
		|| !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	if (!ASC)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (ActorInfo->IsNetAuthority())
	{
		ActiveTimingEffectHandle = UGE_WeaponTimingModifier::ApplyTo(
			ASC,
			FireIntervalMultiplier,
			ReloadMontageRateMultiplier,
			GetAbilityLevel(Handle, ActorInfo));
		if (!ActiveTimingEffectHandle.IsValid())
		{
			EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
			return;
		}

		if (!ApplyDurationEffect(Handle, ActorInfo, ActivationInfo))
		{
			EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
			return;
		}
	}

	AActor* AvatarActor = GetAvatarActor(ActorInfo);
	ExecuteActivationCue(ASC, AvatarActor);
	AddWeaponCue(ASC, AvatarActor);

	HitCountsByTarget.Reset();

	if (ActorInfo->IsNetAuthority())
	{
		HitEventTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
			this,
			GameplayTagSystem::Event_Weapon_Hit,
			nullptr,
			false,
			true
		);
		if (HitEventTask)
		{
			HitEventTask->EventReceived.AddDynamic(this, &UGA_Overdrive::OnOverdriveHit);
			HitEventTask->ReadyForActivation();
		}
	}
}

void UGA_Overdrive::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	if (HitEventTask)
	{
		HitEventTask->EndTask();
		HitEventTask = nullptr;
	}

	if (UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr)
	{
		// Unbind before removing the effect to avoid recursively ending the ability.
		if (FOnActiveGameplayEffectRemoved_Info* Delegate = ASC->OnGameplayEffectRemoved_InfoDelegate(ActiveDurationEffectHandle))
		{
			Delegate->Remove(DurationRemovedHandle);
		}
		DurationRemovedHandle.Reset();
		RemoveWeaponCue(ASC);
		if (ActorInfo && ActorInfo->IsNetAuthority() && ActiveTimingEffectHandle.IsValid())
		{
			ASC->RemoveActiveGameplayEffect(ActiveTimingEffectHandle);
		}
		if (ActorInfo && ActorInfo->IsNetAuthority() && ActiveDurationEffectHandle.IsValid())
		{
			ASC->RemoveActiveGameplayEffect(ActiveDurationEffectHandle);
		}
	}
	ActiveTimingEffectHandle.Invalidate();
	ActiveDurationEffectHandle.Invalidate();
	HitCountsByTarget.Reset();

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UGA_Overdrive::OnDurationEffectRemoved(const FGameplayEffectRemovalInfo& RemovalInfo)
{
	if (IsActive())
	{
		EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(),
			true, RemovalInfo.bPrematureRemoval);
	}
}

bool UGA_Overdrive::ApplyDurationEffect(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo)
{
	if (!DurationEffectClass || Duration <= 0.0f)
	{
		return false;
	}

	FGameplayEffectSpecHandle SpecHandle = MakeOutgoingGameplayEffectSpec(
		Handle,
		ActorInfo,
		ActivationInfo,
		DurationEffectClass,
		GetAbilityLevel(Handle, ActorInfo));
	if (!SpecHandle.IsValid())
	{
		return false;
	}

	SpecHandle.Data->SetDuration(Duration, true);
	ActiveDurationEffectHandle = ApplyGameplayEffectSpecToOwner(
		Handle,
		ActorInfo,
		ActivationInfo,
		SpecHandle);
	UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();
	FOnActiveGameplayEffectRemoved_Info* Removed = ASC
		? ASC->OnGameplayEffectRemoved_InfoDelegate(ActiveDurationEffectHandle) : nullptr;
	if (!Removed)
	{
		return false;
	}
	DurationRemovedHandle = Removed->AddUObject(this, &UGA_Overdrive::OnDurationEffectRemoved);
	return true;
}

void UGA_Overdrive::OnOverdriveHit(FGameplayEventData Payload)
{
	const FHitResult HitResult = UAbilitySystemBlueprintLibrary::GetHitResultFromTargetData(Payload.TargetData, 0);
	AActor* HitActor = HitResult.GetActor();
	const UAbilitySystemComponent* SourceASC = GetCurrentActorInfo()
		? GetCurrentActorInfo()->AbilitySystemComponent.Get()
		: nullptr;
	if (Payload.EventMagnitude <= 0.0f
		|| !CombatTargetLibrary::IsHostileTarget(HitActor, SourceASC, GetAvatarActor(GetCurrentActorInfo()), true))
	{
		return;
	}

	for (auto Iterator = HitCountsByTarget.CreateIterator(); Iterator; ++Iterator)
	{
		if (!Iterator.Key().IsValid())
		{
			Iterator.RemoveCurrent();
		}
	}

	const int32 RequiredHits = FMath::Max(HitsToExplosion, 1);
	const TWeakObjectPtr<AActor> TargetKey(HitActor);
	int32& TargetHitCount = HitCountsByTarget.FindOrAdd(TargetKey);
	TargetHitCount++;

	if (TargetHitCount < RequiredHits)
	{
		return;
	}

	HitCountsByTarget.Remove(TargetKey);
	TriggerElectricExplosion(HitActor, HitResult);
}

void UGA_Overdrive::ExecuteActivationCue(UAbilitySystemComponent* ASC, AActor* AvatarActor) const
{
	if (!ASC)
	{
		return;
	}

	FGameplayCueParameters CueParameters;
	CueParameters.Instigator = AvatarActor;
	CueParameters.EffectCauser = AvatarActor;
	CueParameters.Location = GameplayCueFxLibrary::GetActorFeetLocation(AvatarActor);
	ASC->ExecuteGameplayCue(GameplayTagSystem::GameplayCue_Skill_Overdrive_Activate, CueParameters);
}

void UGA_Overdrive::AddWeaponCue(UAbilitySystemComponent* ASC, AActor* AvatarActor) const
{
	if (!ASC)
	{
		return;
	}

	const UWeaponComponent* WeaponComponent = AvatarActor
		? AvatarActor->FindComponentByClass<UWeaponComponent>()
		: nullptr;
	AWeaponBase* Weapon = WeaponComponent ? WeaponComponent->GetEquippedWeapon() : nullptr;

	FGameplayCueParameters CueParameters;
	if (Weapon)
	{
		const FTransform FireTransform = Weapon->GetFireTransform(AvatarActor);
		CueParameters = WeaponCueLibrary::MakeWeaponCueParameters(
			Weapon,
			AvatarActor,
			FireTransform.GetLocation(),
			FireTransform.GetRotation().GetForwardVector()
		);
	}
	else
	{
		CueParameters.Instigator = AvatarActor;
		CueParameters.EffectCauser = AvatarActor;
		CueParameters.Location = AvatarActor ? AvatarActor->GetActorLocation() : FVector::ZeroVector;
	}

	ASC->AddGameplayCue(GameplayTagSystem::GameplayCue_Skill_Overdrive_Weapon, CueParameters);
}

void UGA_Overdrive::RemoveWeaponCue(UAbilitySystemComponent* ASC) const
{
	if (!ASC)
	{
		return;
	}

	ASC->RemoveGameplayCue(GameplayTagSystem::GameplayCue_Skill_Overdrive_Weapon);
}

void UGA_Overdrive::TriggerElectricExplosion(AActor* PrimaryTarget, const FHitResult& HitResult)
{
	const FGameplayAbilityActorInfo* Info = GetCurrentActorInfo();
	if (!Info || !Info->IsNetAuthority())
	{
		return;
	}
	FOverdriveDischarge Discharge;
	Discharge.World = GetWorld();
	Discharge.SourceAbilitySystem = Info->AbilitySystemComponent.Get();
	Discharge.OwnerActor = GetAvatarActor(Info);
	Discharge.AbilityLevel = GetAbilityLevel();
	Discharge.ExplosionRadius = ExplosionRadius;
	Discharge.ExplosionDamage = ExplosionDamage;
	Discharge.ExplosionDamageEffectClass = ExplosionDamageEffectClass;
	Discharge.ChainSearchRadius = ChainSearchRadius;
	Discharge.ChainMaxDistance = ChainMaxDistance;
	Discharge.MaxChainTargets = MaxChainTargets;
	Discharge.ChainDamage = ChainDamage;
	Discharge.ChainDamageEffectClass = ChainDamageEffectClass;
	Discharge.bRequireChainLineOfSight = bRequireChainLineOfSight;
	Discharge.ChainTargetSocketName = ChainTargetSocketName;
	Discharge.bShowDebug = bShowDebug;
	Discharge.Execute(PrimaryTarget, HitResult);
}

AActor* UGA_Overdrive::GetAvatarActor(const FGameplayAbilityActorInfo* ActorInfo) const
{
	return ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
}
