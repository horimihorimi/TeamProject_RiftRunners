#include "GA_CharacterSkillBase.h"

#include "../Tags/GameplayTagSystem.h"

UGA_CharacterSkillBase::UGA_CharacterSkillBase()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	bReplicateInputDirectly = true;

	FGameplayTagContainer DefaultAbilityTags;
	DefaultAbilityTags.AddTag(GameplayTagSystem::Ability_Skill_Main);
	SetAssetTags(DefaultAbilityTags);

	ActivationOwnedTags.AddTag(GameplayTagSystem::State_Skill_Active);
	ActivationBlockedTags.AddTag(GameplayTagSystem::State_Dead);
}

void UGA_CharacterSkillBase::ApplyCooldown(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo) const
{
	if (!CooldownGameplayEffectClass || CooldownDuration <= 0.0f)
	{
		return;
	}

	FGameplayEffectSpecHandle SpecHandle = MakeOutgoingGameplayEffectSpec(
		Handle,
		ActorInfo,
		ActivationInfo,
		CooldownGameplayEffectClass,
		GetAbilityLevel(Handle, ActorInfo));
	if (!SpecHandle.IsValid())
	{
		return;
	}

	SpecHandle.Data->SetSetByCallerMagnitude(GameplayTagSystem::Data_Cooldown, CooldownDuration);
	ApplyGameplayEffectSpecToOwner(Handle, ActorInfo, ActivationInfo, SpecHandle);
}
