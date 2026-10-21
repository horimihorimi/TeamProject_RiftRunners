#pragma once

#include "CoreMinimal.h"
#include "GameCore/Ability/GA_WeaponAbilityBase.h"
#include "GA_DeployTurret.generated.h"

class ATurretActor;

UCLASS()
class TEAMPROJECT_FIN_API UGA_DeployTurret : public UGA_WeaponAbilityBase
{
	GENERATED_BODY()

public:
	UGA_DeployTurret();

protected:
	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	virtual void OnTriggered();

	UPROPERTY(EditDefaultsOnly, Category = "Deploy Turret")
	TSubclassOf<ATurretActor> TurretClass;
	UPROPERTY(EditDefaultsOnly, Category = "Deploy Turret", meta = (ClampMin = "0.0"))
	float DamageMultiplier = 0.5f;
	UPROPERTY(EditDefaultsOnly, Category = "Deploy Turret", meta = (ClampMin = "0.0"))
	float DamageAmpMultiplier = 1.0f;
	UPROPERTY(EditDefaultsOnly, Category = "Deploy Turret", meta = (ClampMin = "0.0"))
	float AmmoCapacityAmpMultiplier = 1.0f;
};
