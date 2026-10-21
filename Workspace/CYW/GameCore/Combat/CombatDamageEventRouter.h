#pragma once

#include "CoreMinimal.h"
#include "GameplayEffectExtension.h"

class UAbilitySystemComponent;

namespace CombatDamageEventRouter
{
	TEAMPROJECT_FIN_API void RouteDamageExecuted(
		UAbilitySystemComponent* TargetASC,
		AActor* TargetActor,
		AActor* InstigatorActor,
		const FGameplayEffectModCallbackData& Data,
		float LocalDamageDone,
		float AppliedDamage,
		float PreviousHealth,
		float NewHealth,
		float MaxHealth);
}
