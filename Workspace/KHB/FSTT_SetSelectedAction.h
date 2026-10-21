#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "StateTreeTaskBase.h"
#include "FSTT_SetSelectedAction.generated.h"

USTRUCT()
struct FSTT_SetSelectedActionInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Action")
	FGameplayTag Action;
};

/** StateTree 진입 시 컨트롤러의 다음 실행 Action을 지정한다. */
USTRUCT(meta = (DisplayName = "Set Selected Action", Category = "Monster"))
struct TEAMPROJECT_FIN_API FSTT_SetSelectedAction : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FSTT_SetSelectedActionInstanceData;

	virtual const UStruct* GetInstanceDataType() const override
	{
		return FInstanceDataType::StaticStruct();
	}

	virtual EStateTreeRunStatus EnterState(
		FStateTreeExecutionContext& Context,
		const FStateTreeTransitionResult& Transition) const override;
};
