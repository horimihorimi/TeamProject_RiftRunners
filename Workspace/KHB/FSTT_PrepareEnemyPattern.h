#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "StateTreeTaskBase.h"
#include "FSTT_PrepareEnemyPattern.generated.h"

class UMonsterCombatPositioningComponent;

USTRUCT()
struct FSTT_PrepareEnemyPatternInstanceData
{
	GENERATED_BODY()

	UPROPERTY(Transient)
	TObjectPtr<UActorComponent> MovementExecutor = nullptr;

	/** Reposition For Action을 맡은 위치 보정 컴포넌트. 설정돼 있으면 이동 대신 재배치를 기다립니다. */
	UPROPERTY(Transient)
	TObjectPtr<UMonsterCombatPositioningComponent> RepositionExecutor = nullptr;

	/** 이 준비가 GA 몽타주와 병행하는 이동인지. ExitState의 취소 여부를 가릅니다. */
	bool bMoveDuringAbility = false;
};

/**
 * Current Pattern의 시전 전 준비를 맡을 구현체에 위임합니다.
 *
 * 걷거나 나는 이동은 몬스터별 IEnemyPatternMovement 구현체(이동 컴포넌트)가,
 * 순간이동 재배치(Reposition For Action)는 이동 방식과 무관하므로 위치 보정 컴포넌트가 맡습니다.
 */
USTRUCT(meta = (DisplayName = "Prepare Enemy Pattern", Category = "Enemy Framework"))
struct TEAMPROJECT_FIN_API FSTT_PrepareEnemyPattern : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FSTT_PrepareEnemyPatternInstanceData;

	FSTT_PrepareEnemyPattern();

	virtual const UStruct* GetInstanceDataType() const override
	{
		return FInstanceDataType::StaticStruct();
	}

	virtual EStateTreeRunStatus EnterState(
		FStateTreeExecutionContext& Context,
		const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, float DeltaTime) const override;
	virtual void ExitState(
		FStateTreeExecutionContext& Context,
		const FStateTreeTransitionResult& Transition) const override;
};

