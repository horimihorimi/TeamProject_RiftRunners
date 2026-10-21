#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "StateTreeEvaluatorBase.h"
#include "FSTE_MonsterCombatState.generated.h"

USTRUCT()
struct FSTE_MonsterCombatStateInstanceData
{
	GENERATED_BODY()

	// 몬스터 Pawn의 ASC가 State.AI.Encountered 태그를 보유하는지 전달한다.
	UPROPERTY(EditAnywhere, Category = "Output")
	bool bIsDormant = true;

	UPROPERTY(EditAnywhere, Category = "Output")
	bool bHasEncountered = false;

	/** ASC가 State.Groggy 태그를 보유하고 있는지 여부 */
	UPROPERTY(EditAnywhere, Category = "Output")
	bool bIsGroggy = false;

	// BehaviorDecision에서 뽑힌 행동 태그. 형제 State끼리 못 보는 값을 여기서 중계한다.
	UPROPERTY(EditAnywhere, Category = "Output")
	FGameplayTag SelectedAction;

};

/*
 * 퍼셉션이 아니라 "전투 진행 상태"를 다루는 Evaluator.
 *
 * AMonsterAIController가 실제 값을 들고 있고, 여기서는 매 틱 옮겨서
 * ST 바인딩용으로 노출만 한다.
 */
USTRUCT(meta = (DisplayName = "Monster Combat State", Category = "Monster"))
struct TEAMPROJECT_FIN_API FSTE_MonsterCombatState
	: public FStateTreeEvaluatorCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FSTE_MonsterCombatStateInstanceData;

	virtual const UStruct* GetInstanceDataType() const override
	{
		return FInstanceDataType::StaticStruct();
	}

	virtual void Tick(FStateTreeExecutionContext& Context, float DeltaTime) const override;
};
