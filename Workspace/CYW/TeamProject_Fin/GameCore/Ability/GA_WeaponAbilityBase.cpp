#include "GA_WeaponAbilityBase.h"
#include "../Tags/GameplayTagSystem.h"

#include "../Weapon/Core/WeaponBase.h"
#include "../Weapon/Core/WeaponComponent.h"
#include "AbilitySystemComponent.h"

UGA_WeaponAbilityBase::UGA_WeaponAbilityBase()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	bReplicateInputDirectly = true;
}

bool UGA_WeaponAbilityBase::CanActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayTagContainer* SourceTags,
	const FGameplayTagContainer* TargetTags,
	FGameplayTagContainer* OptionalRelevantTags) const
{
	return Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags)
		&& HasRequiredWeaponModifier(ActorInfo);
}

AActor* UGA_WeaponAbilityBase::GetAvatarActor(const FGameplayAbilityActorInfo* ActorInfo) const
{
	return ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
}

UAbilitySystemComponent* UGA_WeaponAbilityBase::GetAbilitySystemComponentFromInfo(const FGameplayAbilityActorInfo* ActorInfo) const
{
	return ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
}

UWeaponComponent* UGA_WeaponAbilityBase::FindAvatarWeaponComponent(const FGameplayAbilityActorInfo* ActorInfo) const
{
	AActor* AvatarActor = GetAvatarActor(ActorInfo);
	return AvatarActor ? AvatarActor->FindComponentByClass<UWeaponComponent>() : nullptr;
}

AWeaponBase* UGA_WeaponAbilityBase::GetEquippedWeapon(const FGameplayAbilityActorInfo* ActorInfo) const
{
	UWeaponComponent* WeaponComponent = FindAvatarWeaponComponent(ActorInfo);
	return WeaponComponent ? WeaponComponent->GetEquippedWeapon() : nullptr;
}

bool UGA_WeaponAbilityBase::HasRequiredWeaponModifier(const FGameplayAbilityActorInfo* ActorInfo) const
{
	if (RequiredWeaponModifier == EWeaponModiferType::None)
	{
		return true;
	}

	const AWeaponBase* Weapon = GetEquippedWeapon(ActorInfo);
	return Weapon && Weapon->HasModifier(RequiredWeaponModifier);
}

void UGA_WeaponAbilityBase::BroadcastSelfGameplayEvent(
	UAbilitySystemComponent* ASC,
	AActor* AvatarActor,
	const FGameplayTag& EventTag) const
{
	if (!ASC || !EventTag.IsValid())
	{
		return;
	}

	FGameplayEventData EventData;
	EventData.EventTag = EventTag;
	EventData.Instigator = AvatarActor;
	EventData.Target = AvatarActor;
	ASC->HandleGameplayEvent(EventTag, &EventData);
}

void UGA_WeaponAbilityBase::FinishAbility(bool bWasCancelled)
{
	EndAbility(
		GetCurrentAbilitySpecHandle(),
		GetCurrentActorInfo(),
		GetCurrentActivationInfo(),
		true,
		bWasCancelled
	);
}

const FGameplayTagContainer* UGA_WeaponAbilityBase::GetCooldownTags() const
{
	return ModCooldownTags.IsEmpty() ? Super::GetCooldownTags() : &ModCooldownTags;
}

void UGA_WeaponAbilityBase::ApplyCooldown(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo) const
{
	if (ModCooldownTags.IsEmpty())
	{
		Super::ApplyCooldown(Handle, ActorInfo, ActivationInfo);
		return;
	}
	if (!CooldownGameplayEffectClass || InternalCooldown <= 0.0f)
	{
		return;
	}
	FGameplayEffectSpecHandle Spec = MakeOutgoingGameplayEffectSpec(
		Handle, ActorInfo, ActivationInfo, CooldownGameplayEffectClass, GetAbilityLevel(Handle, ActorInfo));
	if (!Spec.IsValid())
	{
		return;
	}
	Spec.Data->SetSetByCallerMagnitude(GameplayTagSystem::Data_Cooldown, InternalCooldown);
	Spec.Data->DynamicGrantedTags.AppendTags(ModCooldownTags);
	ModCooldownHandle = ApplyGameplayEffectSpecToOwner(Handle, ActorInfo, ActivationInfo, Spec);
}

void UGA_WeaponAbilityBase::ClearModCooldown()
{
	if (UAbilitySystemComponent* ASC = ModCooldownHandle.GetOwningAbilitySystemComponent())
	{
		ASC->RemoveActiveGameplayEffect(ModCooldownHandle);
	}
	ModCooldownHandle.Invalidate();
}
