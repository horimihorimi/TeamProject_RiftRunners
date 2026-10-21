#pragma once

#include "CoreMinimal.h"
#include "GameCore/Ability/GA_WeaponAbilityBase.h"
#include "GA_RocketBarrage.generated.h"

class ARocketActor;
class UProjectilePoolComponent;

UCLASS()
class TEAMPROJECT_FIN_API UGA_RocketBarrage : public UGA_WeaponAbilityBase
{
	GENERATED_BODY()

public:
	UGA_RocketBarrage();
	virtual void OnAvatarSet(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec) override;
	virtual void OnRemoveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec) override;

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void OnTriggered();

	UPROPERTY(EditDefaultsOnly, Category = "Rocket Barrage")
	TSubclassOf<ARocketActor> RocketClass;

	UPROPERTY(EditDefaultsOnly, Category = "Rocket Barrage", meta = (ClampMin = "1.0"))
	float DamageThreshold = 100.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Rocket Barrage", meta = (ClampMin = "1"))
	int32 MaxRockets = 5;

	UPROPERTY(EditDefaultsOnly, Category = "Rocket Barrage|Targeting", meta = (ClampMin = "1.0"))
	float SearchRadius = 2000.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Rocket Barrage|Targeting", meta = (ClampMin = "0.05"))
	float ScanInterval = 0.2f;

	UPROPERTY(EditDefaultsOnly, Category = "Rocket Barrage|Targeting", meta = (ClampMin = "1.0"))
	float AimDistance = 10000.0f;

private:
	void OnDamageDealt(const FGameplayEventData* Event);
	void ScanTargets();
	void ResetRockets();
	void Unbind();
	void OnDeathChanged(FGameplayTag Tag, int32 Count);

	UFUNCTION()
	void OnWeaponChanged(AWeaponBase* Weapon);
	TArray<TWeakObjectPtr<ARocketActor>> Rockets;
	TWeakObjectPtr<UAbilitySystemComponent> BoundASC;
	TWeakObjectPtr<UWeaponComponent> BoundWeapons;
	TWeakObjectPtr<AWeaponBase> BoundWeapon;

	UPROPERTY()
	TObjectPtr<UProjectilePoolComponent> RocketPool;
	FDelegateHandle DamageHandle;
	FDelegateHandle DeathHandle;
	FDelegateHandle ModifiersHandle;
	FTimerHandle ScanTimer;
	float AccumulatedDamage = 0.0f;
};
