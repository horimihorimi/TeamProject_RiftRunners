// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GA_Aim.generated.h"

/**
 * 조준(Aim) 상태를 제어하는 Gameplay Ability
 * 활성화 시 조준용 Gameplay Effect(GE)를 적용하여 이속 감소 및 조준 태그를 부여합니다.
 * 카메라 줌 인/아웃은 블루프린트 자식 클래스에서 타임라인(Timeline)으로 처리하는 것을 권장합니다.
 */
UCLASS()
class TEAMPROJECT_FIN_API UGA_Aim : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_Aim();

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
	/** 조준 상태일 때 적용할 Gameplay Effect 클래스 (이동속도 감소, 조준 태그 부여 등) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Aim")
	TSubclassOf<class UGameplayEffect> AimEffectClass;

private:
	/** 현재 적용된 조준 GE의 핸들 (어빌리티 종료 시 제거용) */
	FActiveGameplayEffectHandle ActiveAimEffectHandle;
};
