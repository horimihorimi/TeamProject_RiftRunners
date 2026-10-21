#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "GE_WeaponTimingModifier.generated.h"

class UAbilitySystemComponent;

UCLASS()
class TEAMPROJECT_FIN_API UGE_WeaponTimingModifier : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UGE_WeaponTimingModifier();

	static FActiveGameplayEffectHandle ApplyTo(
		UAbilitySystemComponent* ASC,
		float FireIntervalMultiplier,
		float ReloadMontageRateMultiplier,
		float Level = 1.0f);
};
