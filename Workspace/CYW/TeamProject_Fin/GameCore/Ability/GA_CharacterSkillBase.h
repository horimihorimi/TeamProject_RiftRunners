#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GA_CharacterSkillBase.generated.h"

UCLASS(Abstract, Blueprintable)
class TEAMPROJECT_FIN_API UGA_CharacterSkillBase : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_CharacterSkillBase();

protected:
	virtual void ApplyCooldown(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo) const override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Character Skill|Cooldown", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float CooldownDuration = 60.0f;
};
