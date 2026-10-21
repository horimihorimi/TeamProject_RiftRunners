#pragma once

#include "CoreMinimal.h"
#include "StateTreeConditionBase.h"
#include "FSTC_BossPhaseTransitionInProgress.generated.h"

USTRUCT()
struct FSTC_BossPhaseTransitionInProgressInstanceData
{
	GENERATED_BODY()
};

/** 보스가 체력 임계값을 넘어 다음 페이즈 전환을 요청한 상태인지 판정합니다. */
USTRUCT(meta = (DisplayName = "Boss Phase Transition In Progress", Category = "Boss"))
struct TEAMPROJECT_FIN_API FSTC_BossPhaseTransitionInProgress : public FStateTreeConditionCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FSTC_BossPhaseTransitionInProgressInstanceData;

	virtual const UStruct* GetInstanceDataType() const override
	{
		return FInstanceDataType::StaticStruct();
	}

	virtual bool TestCondition(FStateTreeExecutionContext& Context) const override;
};
