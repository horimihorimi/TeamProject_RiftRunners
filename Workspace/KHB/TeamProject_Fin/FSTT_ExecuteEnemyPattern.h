#pragma once

#include "Abilities/GameplayAbilityTypes.h"
#include "CoreMinimal.h"
#include "GameCore/Enemy/Data/EnemyDataAssets.h"
#include "StateTreeTaskBase.h"
#include "FSTT_ExecuteEnemyPattern.generated.h"

class UAbilitySystemComponent;

USTRUCT()
struct FSTT_ExecuteEnemyPatternInstanceData
{
	GENERATED_BODY()

	UPROPERTY(Transient)
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent = nullptr;

	FGameplayAbilitySpecHandle ActiveSpecHandle;

	/** 시전 중 상태가 바뀌면 GA를 어떻게 할지. 시작할 때 패턴에서 복사한다. */
	EEnemyActionInterruptPolicy InterruptPolicy = EEnemyActionInterruptPolicy::CancelImmediately;

	/** 이 시전을 위해 전투 디렉터에게 받은 공격 토큰을 들고 있는지. */
	bool bHoldingAttackToken = false;

	bool bRequiresVisibleTarget = true;
};

/*
 * Current Pattern의 GA를 실행하고, GA가 끝날 때까지 StateTree를 Running 상태로 유지합니다.
 *
 * 시전 중에는 회피 요청을 막고(Interruptible 해제), 필요하면 공격 토큰을 들고 있는다.
 * 상태를 벗어나면 둘 다 되돌리고, InterruptPolicy에 따라 남은 GA를 정리한다.
 */
USTRUCT(meta = (DisplayName = "Execute Enemy Pattern", Category = "Enemy Framework"))
struct TEAMPROJECT_FIN_API FSTT_ExecuteEnemyPattern : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FSTT_ExecuteEnemyPatternInstanceData;

	FSTT_ExecuteEnemyPattern();

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

