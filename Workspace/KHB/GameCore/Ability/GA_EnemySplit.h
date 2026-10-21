#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GA_EnemySplit.generated.h"
class UAnimMontage;
UCLASS()
class TEAMPROJECT_FIN_API UGA_EnemySplit : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_EnemySplit();

	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	                                const FGameplayTagContainer* SourceTags = nullptr,
	                                const FGameplayTagContainer* TargetTags = nullptr,
	                                FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	                             const FGameplayAbilityActivationInfo ActivationInfo,
	                             const FGameplayEventData* TriggerEventData) override;

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Split")
	TObjectPtr<UAnimMontage> SplitMontage;

private:
	UFUNCTION()
	void OnSplitCommit(FGameplayEventData Payload);

	bool bCommitReceived = false;

	UFUNCTION()
	void OnCompleted();

	UFUNCTION()
	void OnInterrupted();
};
