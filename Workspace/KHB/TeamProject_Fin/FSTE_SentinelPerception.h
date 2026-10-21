#pragma once

#include "CoreMinimal.h"
#include "StateTreeEvaluatorBase.h"
#include "FSTE_SentinelPerception.generated.h"

class AActor;

USTRUCT()
struct FSTE_SentinelPerceptionInstanceData
{
	GENERATED_BODY()

	/*
	 * 컨트롤러가 이미 처리해둔 퍼셉션 결과를 그대로 옮긴다.
	 *
	 * 실제 감지(시야/청각)는 AMonsterAIController가 담당한다.
	 */
	UPROPERTY(EditAnywhere, Category = "Output")
	TObjectPtr<AActor> TargetActor = nullptr;

	UPROPERTY(EditAnywhere, Category = "Output")
	bool bHasTarget = false;
};

/*
 * AMonsterAIController의 퍼셉션 결과를 StateTree 바인딩용 프로퍼티로 옮긴다.
 */
USTRUCT(meta = (DisplayName = "Monster Perception", Category = "Monster"))
struct TEAMPROJECT_FIN_API FSTE_SentinelPerception
	: public FStateTreeEvaluatorCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FSTE_SentinelPerceptionInstanceData;

	virtual const UStruct* GetInstanceDataType() const override
	{
		return FInstanceDataType::StaticStruct();
	}

	virtual void Tick(FStateTreeExecutionContext& Context, float DeltaTime) const override;
};
