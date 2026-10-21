#include "GE_WeaponTimingModifier.h"

#include "../Attribute/CharacterAttributeSet.h"
#include "../Tags/GameplayTagSystem.h"
#include "AbilitySystemComponent.h"

namespace
{
	FGameplayModifierInfo MakeMultiplier(
		const FGameplayAttribute& Attribute,
		const FGameplayTag& SetByCallerTag)
	{
		FSetByCallerFloat SetByCaller;
		SetByCaller.DataTag = SetByCallerTag;

		FGameplayModifierInfo Modifier;
		Modifier.Attribute = Attribute;
		Modifier.ModifierOp = EGameplayModOp::Multiplicitive;
		Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(SetByCaller);
		return Modifier;
	}
}

UGE_WeaponTimingModifier::UGE_WeaponTimingModifier()
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;
	Modifiers.Add(MakeMultiplier(
		UCharacterAttributeSet::GetFireIntervalMultiplierAttribute(),
		GameplayTagSystem::Data_Multiplier_FireInterval));
	Modifiers.Add(MakeMultiplier(
		UCharacterAttributeSet::GetReloadMontageRateMultiplierAttribute(),
		GameplayTagSystem::Data_Multiplier_ReloadMontageRate));
}

FActiveGameplayEffectHandle UGE_WeaponTimingModifier::ApplyTo(
	UAbilitySystemComponent* ASC,
	float FireIntervalMultiplier,
	float ReloadMontageRateMultiplier,
	float Level)
{
	if (!ASC)
	{
		return FActiveGameplayEffectHandle();
	}

	FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
	Context.AddSourceObject(ASC->GetAvatarActor());

	FGameplayEffectSpecHandle SpecHandle = ASC->MakeOutgoingSpec(StaticClass(), Level, Context);
	if (!SpecHandle.IsValid())
	{
		return FActiveGameplayEffectHandle();
	}

	SpecHandle.Data->SetSetByCallerMagnitude(
		GameplayTagSystem::Data_Multiplier_FireInterval,
		FMath::Max(FireIntervalMultiplier, 0.01f));
	SpecHandle.Data->SetSetByCallerMagnitude(
		GameplayTagSystem::Data_Multiplier_ReloadMontageRate,
		FMath::Max(ReloadMontageRateMultiplier, 0.01f));

	return ASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
}
