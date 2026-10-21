// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GA_Sprint.generated.h"

/**
 * 달리기(Sprint/Jog) 상태를 제어하는 Gameplay Ability
 * 활성화 시 스태미나 코스트 지불 및 달리기용 Gameplay Effect(GE)를 적용합니다.
 */
UCLASS()
class TEAMPROJECT_FIN_API UGA_Sprint : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_Sprint();

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

	void OnStaminaChanged(const struct FOnAttributeChangeData& Data);

protected:
	/** 달리기 상태일 때 적용할 Gameplay Effect 클래스 (태그 부여 및 이동속도 변경) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sprint")
	TSubclassOf<class UGameplayEffect> SprintEffectClass;

	/** 달리기 상태일 때 주기적으로 스태미나를 깎을 Gameplay Effect 클래스 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sprint")
	TSubclassOf<class UGameplayEffect> SprintDrainEffectClass;

	/** 달리기가 끝난 직후 스태미나 회복을 지연시킬 Gameplay Effect 클래스 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sprint")
	TSubclassOf<class UGameplayEffect> SprintRegenPauseEffectClass;

private:
	/** 현재 적용된 달리기 GE의 핸들 (어빌리티 종료 시 제거용) */
	FActiveGameplayEffectHandle ActiveSprintEffectHandle;

	/** 현재 적용된 달리기 스태미나 소모 GE의 핸들 */
	FActiveGameplayEffectHandle ActiveSprintDrainEffectHandle;

	FDelegateHandle StaminaChangeDelegateHandle;
};
