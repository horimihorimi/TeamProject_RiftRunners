#pragma once

#include "CoreMinimal.h"
#include "Abilities/Tasks/AbilityTask.h"
#include "GameplayEffectTypes.h"
#include "AbilityTask_ReloadSequence.generated.h"

class AWeaponBase;
class UAnimMontage;
class UAbilityTask_PlayMontageAndWait;
class UAbilityTask_WaitGameplayEvent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FReloadSequenceEvent);

// Coordinates animation and notifies; the ability owns authority and ammo changes.
UCLASS()
class TEAMPROJECT_FIN_API UAbilityTask_ReloadSequence : public UAbilityTask
{
	GENERATED_BODY()

public:
	static UAbilityTask_ReloadSequence* CreateReloadSequence(
		UGameplayAbility* OwningAbility, UAnimMontage* Montage, AWeaponBase* Weapon);
	virtual void Activate() override;
	virtual void OnDestroy(bool AbilityEnded) override;

	UPROPERTY()
	FReloadSequenceEvent OnCommitRequested;
	UPROPERTY()
	FReloadSequenceEvent OnCancelled;

private:
	UFUNCTION()
	void OnReloadCommit(FGameplayEventData Payload);
	UFUNCTION()
	void OnMontageCompleted();
	UFUNCTION()
	void OnMontageCancelled();
	UFUNCTION()
	void OnLeftHandIKRecover(FGameplayEventData Payload);
	void RequestCommit();
	bool CreateReloadEventTask();
	void CreateLeftHandIKRecoverTask();
	bool CreateReloadMontageTask();
	void CleanupAbilityTasks();

	UPROPERTY()
	TObjectPtr<UAnimMontage> ReloadMontage;
	UPROPERTY()
	TObjectPtr<UAbilityTask_PlayMontageAndWait> MontageTask;
	UPROPERTY()
	TObjectPtr<UAbilityTask_WaitGameplayEvent> ReloadEventTask;
	UPROPERTY()
	TObjectPtr<UAbilityTask_WaitGameplayEvent> LeftHandIKRecoverTask;
	TWeakObjectPtr<AWeaponBase> ReloadWeapon;
	bool bStopping = false;
	bool bCommitRequested = false;
};
