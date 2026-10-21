#pragma once

#include "CoreMinimal.h"
#include "StateTreeTaskBase.h"
#include "FSTT_WaitPatternRecovery.generated.h"

USTRUCT()
struct FSTT_WaitPatternRecoveryInstanceData
{
	GENERATED_BODY()

	double EndTimeSeconds = 0.0;
};

/** Current Pattern의 대기 시간(Min~Max에서 뽑음)이 0보다 클 때만 대기합니다. */
USTRUCT(meta = (DisplayName = "Wait Pattern Recovery", Category = "Enemy Framework"))
struct TEAMPROJECT_FIN_API FSTT_WaitPatternRecovery : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FSTT_WaitPatternRecoveryInstanceData;

	FSTT_WaitPatternRecovery();

	virtual const UStruct* GetInstanceDataType() const override
	{
		return FInstanceDataType::StaticStruct();
	}

	virtual EStateTreeRunStatus EnterState(
		FStateTreeExecutionContext& Context,
		const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, float DeltaTime) const override;
};

