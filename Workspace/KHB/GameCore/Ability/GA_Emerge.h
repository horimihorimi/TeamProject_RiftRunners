#pragma once

#include "Abilities/GameplayAbility.h"
#include "CoreMinimal.h"
#include "GA_Emerge.generated.h"

class UAbilityTask_PlayMontageAndWait;
class UAbilityTask_WaitGameplayEvent;
class UAbilityTask;
class UAnimMontage;

/**
 * Plays the monster emergence montage while controller movement is locked.
 * Movement is unlocked only after the montage completes normally.
 */
UCLASS()
class TEAMPROJECT_FIN_API UGA_Emerge : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_Emerge();

	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility,
		bool bWasCancelled) override;

protected:
	/** Montage played while the monster emerges from its dormant position. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Emerge")
	TObjectPtr<UAnimMontage> EmergeMontage;

	/** EmergeSpawnType이 Wall일 때 재생할 몽타주. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Emerge")
	TObjectPtr<UAnimMontage> WallEmergeMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Emerge", meta = (ClampMin = "0.01"))
	float MontagePlayRate = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Emerge|Wall Sections")
	FName WallEmergeSectionName = FName(TEXT("Emerge"));

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Emerge|Wall Sections")
	FName TurnLeftSectionName = FName(TEXT("Turn_Left"));

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Emerge|Wall Sections")
	FName TurnRightSectionName = FName(TEXT("Turn_Right"));

	/** 벽 Turn 몽타주의 Motion Warping Notify와 일치해야 한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Emerge|Motion Warping")
	FName TurnWarpTargetName = FName(TEXT("EmergeTurn"));

private:
	UFUNCTION()
	void OnMontageCompleted();

	UFUNCTION()
	void OnMontageInterrupted();

	UFUNCTION()
	void OnBattleCombatReleased(FGameplayEventData Payload);

	UFUNCTION()
	void OnTurnSelectionEvent(FGameplayEventData Payload);

	UPROPERTY()
	TObjectPtr<UAbilityTask_PlayMontageAndWait> PlayMontageTask;

	UPROPERTY()
	TObjectPtr<UAbilityTask_WaitGameplayEvent> TurnSelectionEventTask;

	UPROPERTY()
	TObjectPtr<UAbilityTask_WaitGameplayEvent> BattleCombatReleaseEventTask;

	UPROPERTY()
	TObjectPtr<UAnimMontage> ActiveMontage;

	UPROPERTY()
	TObjectPtr<UAbilityTask> CenterPivotRotationTask;
};
