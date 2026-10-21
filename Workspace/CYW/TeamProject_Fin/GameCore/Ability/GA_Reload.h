#pragma once

#include "CoreMinimal.h"
#include "GA_WeaponAbilityBase.h"
#include "GA_Reload.generated.h"

class AWeaponBase;
class UAnimMontage;
class UAbilityTask_ReloadSequence;
class UWeaponComponent;
class UAbilitySystemComponent;

UCLASS()
class TEAMPROJECT_FIN_API UGA_Reload : public UGA_WeaponAbilityBase
{
	GENERATED_BODY()

public:
	UGA_Reload();

protected:
	virtual bool CanActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayTagContainer* SourceTags = nullptr,
		const FGameplayTagContainer* TargetTags = nullptr,
		FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData
	) override;

	virtual void EndAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility,
		bool bWasCancelled
	) override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Reload|Animation")
	TObjectPtr<UAnimMontage> ReloadMontage;

private:
	UFUNCTION()
	void OnReloadReady();

	UFUNCTION()
	void OnReloadCancelled();

	bool TryApplyReload();
	void BroadcastReloadedEvent(UAbilitySystemComponent* ASC, AActor* AvatarActor) const;

	UPROPERTY()
	TObjectPtr<UAbilityTask_ReloadSequence> ReloadSequence;

	TWeakObjectPtr<AWeaponBase> ReloadWeapon;
	bool bReloadApplied = false;
};
