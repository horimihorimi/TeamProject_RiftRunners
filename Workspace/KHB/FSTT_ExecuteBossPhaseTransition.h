#pragma once

#include "Abilities/GameplayAbilityTypes.h"
#include "CoreMinimal.h"
#include "StateTreeTaskBase.h"
#include "FSTT_ExecuteBossPhaseTransition.generated.h"

class UAbilitySystemComponent;

USTRUCT()
struct FSTT_ExecuteBossPhaseTransitionInstanceData
{
	GENERATED_BODY()

	UPROPERTY(Transient)
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent = nullptr;

	FGameplayAbilitySpecHandle ActiveSpecHandle;
};

/** Pending Phase의 TransitionAbilityTag로 GA를 실행하고 종료까지 대기합니다. */
USTRUCT(meta = (DisplayName = "Execute Boss Phase Transition", Category = "Boss"))
struct TEAMPROJECT_FIN_API FSTT_ExecuteBossPhaseTransition : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FSTT_ExecuteBossPhaseTransitionInstanceData;
	FSTT_ExecuteBossPhaseTransition();
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, float DeltaTime) const override;
};
