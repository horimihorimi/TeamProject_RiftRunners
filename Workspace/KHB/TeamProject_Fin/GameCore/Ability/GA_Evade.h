#pragma once

#include "Abilities/GameplayAbility.h"
#include "CoreMinimal.h"
#include "GA_Evade.generated.h"

class UAbilityTask_PlayMontageAndWait;
class UAnimMontage;

/**
 * AI가 감지한 총성의 반대 방향으로 회피 몽타주를 재생한다.
 * 총성 감지와 회피 가능 여부 판단은 AIController와 State Tree가 담당한다.
 */
UCLASS()
class TEAMPROJECT_FIN_API UGA_Evade : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_Evade();

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
	/** Left, Right 섹션을 포함하는 회피 몽타주. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Evade")
	TObjectPtr<UAnimMontage> EvadeMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Evade")
	FName LeftSectionName = TEXT("Left");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Evade")
	FName RightSectionName = TEXT("Right");

private:
	UFUNCTION()
	void OnMontageCompleted();

	UFUNCTION()
	void OnMontageInterrupted();

	UPROPERTY()
	TObjectPtr<UAbilityTask_PlayMontageAndWait> PlayMontageTask;
};
