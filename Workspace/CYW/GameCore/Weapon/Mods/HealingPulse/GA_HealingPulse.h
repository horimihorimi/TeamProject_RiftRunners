#pragma once

#include "CoreMinimal.h"
#include "GameCore/Ability/GA_WeaponAbilityBase.h"
#include "GA_HealingPulse.generated.h"

class AWeaponBase;
class UWeaponComponent;

UCLASS()
class TEAMPROJECT_FIN_API UGA_HealingPulse : public UGA_WeaponAbilityBase
{
	GENERATED_BODY()

public:
	UGA_HealingPulse();
	virtual void OnAvatarSet(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec) override;
	virtual void OnRemoveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec) override;

protected:
	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	UPROPERTY(EditDefaultsOnly, Category = "Healing Pulse", meta = (ClampMin = "1.0"))
	float DamageThreshold = 300.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Healing Pulse", meta = (ClampMin = "0.0", Units = "cm"))
	float HealRadius = 600.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Healing Pulse", meta = (ClampMin = "0.0"))
	float HealAmount = 35.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Healing Pulse")
	TSubclassOf<UGameplayEffect> HealEffectClass;

	// Use a Blueprint of GCN_AttachNiagaraAndSound with TargetRoot or TargetFeet.
	UPROPERTY(EditDefaultsOnly, Category = "Healing Pulse", meta = (Categories = "GameplayCue"))
	FGameplayTag PulseCueTag;

	UPROPERTY(EditDefaultsOnly, Category = "Healing Pulse|Debug")
	bool bShowDebug = false;

private:
	UFUNCTION()
	void OnWeaponChanged(AWeaponBase* NewWeapon);
	void ResetCharge();
	void UnbindWeapon();
	void TriggerPulse(UAbilitySystemComponent* SourceASC, AActor* AvatarActor);
	void DebugMessage(const FString& Message) const;

	TWeakObjectPtr<UWeaponComponent> BoundWeapons;
	TWeakObjectPtr<AWeaponBase> BoundWeapon;
	FDelegateHandle ModifiersChangedHandle;
	float AccumulatedDamage = 0.0f;
};
