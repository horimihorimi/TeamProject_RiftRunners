// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GA_Crouch.generated.h"

/**
 * 앉기(Crouch) 상태를 제어하는 Gameplay Ability
 * 활성화 시 캡슐 크기를 줄이고(Unreal 기본 Crouch 연동) 앉기용 Gameplay Effect(GE)를 적용합니다.
 */
UCLASS()
class TEAMPROJECT_FIN_API UGA_Crouch : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_Crouch();

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
	/** 앉기 상태일 때 적용할 Gameplay Effect 클래스 (이동속도 감소 등) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Crouch")
	TSubclassOf<class UGameplayEffect> CrouchEffectClass;

private:
	/** 현재 적용된 앉기 GE의 핸들 (어빌리티 종료 시 제거용) */
	FActiveGameplayEffectHandle ActiveCrouchEffectHandle;
};
