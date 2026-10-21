#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GA_Revive.generated.h"

class UNiagaraSystem;
class UNiagaraComponent;

UCLASS()
class TEAMPROJECT_FIN_API UGA_Revive : public UGameplayAbility
{
	GENERATED_BODY()
	
public:
	UGA_Revive();

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	UFUNCTION()
	void OnMontageCompleted();

	UFUNCTION()
	void OnSetupMontageCompleted();

	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Revive Settings")
	TObjectPtr<class UAnimMontage> ReviveMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Revive Settings")
	float ReviveHealthPercentage = 0.3f; 

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Revive Settings")
	TSubclassOf<class UGameplayEffect> ReviveEffectClass;
};
