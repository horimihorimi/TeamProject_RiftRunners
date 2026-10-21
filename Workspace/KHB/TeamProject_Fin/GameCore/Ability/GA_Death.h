#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GA_Death.generated.h"

class ACharacter;
class UAbilityTask_PlayMontageAndWait;
class ABaseCharacter;
class UAbilitySystemComponent;

/**
 * 
 */
UCLASS()
class TEAMPROJECT_FIN_API UGA_Death : public UGameplayAbility
{
	GENERATED_BODY()
	
public:
	UGA_Death();

protected:
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

	FName ComputeDeathDirectionSection(const FGameplayEventData* TriggerEventData) const;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Death|Sections")
	FName SectionFront = FName("Front");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Death|Sections")
	FName SectionBack = FName("Back");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Death|Sections")
	FName SectionLeft = FName("Left");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Death|Sections")
	FName SectionRight = FName("Right");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Death|Knockback")
	float DeathKnockbackForce = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Death|Knockback")
	float DeathKnockbackDuration = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Death|Revive")
	TSubclassOf<class AReviveZoneActor> ReviveZoneClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Death|Effect")
	TSubclassOf<class UGameplayEffect> DeathEffectClass;

private:
	void BeginDeath(ABaseCharacter* Character, UAbilitySystemComponent* ASC);
	
	UFUNCTION()
	void OnDeathMontageFinished();

	void FinishDeathAbility();

	UPROPERTY()
	TObjectPtr<UAbilityTask_PlayMontageAndWait> PlayMontageTask;
};
