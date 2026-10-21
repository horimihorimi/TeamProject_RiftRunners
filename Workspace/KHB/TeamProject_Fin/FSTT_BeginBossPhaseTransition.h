#pragma once

#include "CoreMinimal.h"
#include "StateTreeTaskBase.h"
#include "FSTT_BeginBossPhaseTransition.generated.h"

USTRUCT()
struct FSTT_BeginBossPhaseTransitionInstanceData
{
	GENERATED_BODY()
};

/** 전환 GA를 시작하기 전 현재 패턴 GA와 패턴 이동만 정리합니다. */
USTRUCT(meta = (DisplayName = "Begin Boss Phase Transition", Category = "Boss"))
struct TEAMPROJECT_FIN_API FSTT_BeginBossPhaseTransition : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FSTT_BeginBossPhaseTransitionInstanceData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
};
