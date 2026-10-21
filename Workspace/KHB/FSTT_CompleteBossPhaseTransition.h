#pragma once

#include "CoreMinimal.h"
#include "StateTreeTaskBase.h"
#include "FSTT_CompleteBossPhaseTransition.generated.h"

USTRUCT()
struct FSTT_CompleteBossPhaseTransitionInstanceData
{
	GENERATED_BODY()
};

/** 전환 완료를 확정하고, 새 페이즈 Pattern Set을 활성화합니다. */
USTRUCT(meta = (DisplayName = "Complete Boss Phase Transition", Category = "Boss"))
struct TEAMPROJECT_FIN_API FSTT_CompleteBossPhaseTransition : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FSTT_CompleteBossPhaseTransitionInstanceData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
};
