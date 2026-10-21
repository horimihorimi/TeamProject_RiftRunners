// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffectExecutionCalculation.h"
#include "ExecCalc_Damage.generated.h"

/**
 * 데미지 연산을 전담하는 Execution Calculation 클래스
 * 방어력, 무적, 공격력 증감, 약점 배수, 오폭 배수 등을 수식으로 종합 계산합니다.
 */
UCLASS()
class TEAMPROJECT_FIN_API UExecCalc_Damage : public UGameplayEffectExecutionCalculation
{
	GENERATED_BODY()

public:
	UExecCalc_Damage();

	virtual void Execute_Implementation(const FGameplayEffectCustomExecutionParameters& ExecutionParams, FGameplayEffectCustomExecutionOutput& OutExecutionOutput) const override;
};
