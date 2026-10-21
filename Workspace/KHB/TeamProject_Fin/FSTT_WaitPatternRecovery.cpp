#include "FSTT_WaitPatternRecovery.h"

#include "GameCore/AI/MonsterAIController.h"
#include "GameCore/Character/CommonEnemyCharacter.h"
#include "GameCore/Combat/EnemyPatternComponent.h"
#include "StateTreeExecutionContext.h"

FSTT_WaitPatternRecovery::FSTT_WaitPatternRecovery()
{
	bShouldCallTick = true;
	bShouldCopyBoundPropertiesOnTick = false;
	bShouldCopyBoundPropertiesOnExitState = false;
}

EStateTreeRunStatus FSTT_WaitPatternRecovery::EnterState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	AMonsterAIController* Controller = Cast<AMonsterAIController>(Context.GetOwner());
	ACommonEnemyCharacter* Character = IsValid(Controller)
		? Cast<ACommonEnemyCharacter>(Controller->GetPawn())
		: nullptr;
	UEnemyPatternComponent* PatternComponent = IsValid(Character)
		? Character->GetEnemyPatternComponent()
		: nullptr;
	FEnemyPatternDefinition Pattern;
	if (!IsValid(PatternComponent) || !PatternComponent->GetCurrentPattern(Pattern))
	{
		return EStateTreeRunStatus::Failed;
	}

	// 상한이 하한보다 작으면(기본값 0 포함) 하한으로 고정된 대기다.
	const float RecoveryDuration = Pattern.RecoveryDurationMax > Pattern.RecoveryDuration
		? FMath::FRandRange(Pattern.RecoveryDuration, Pattern.RecoveryDurationMax)
		: Pattern.RecoveryDuration;
	if (RecoveryDuration <= 0.0f)
	{
		return EStateTreeRunStatus::Succeeded;
	}

	const UWorld* World = IsValid(Controller) ? Controller->GetWorld() : nullptr;
	if (!IsValid(World))
	{
		return EStateTreeRunStatus::Failed;
	}

	InstanceData.EndTimeSeconds = World->GetTimeSeconds() + RecoveryDuration;
	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FSTT_WaitPatternRecovery::Tick(
	FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	AMonsterAIController* Controller = Cast<AMonsterAIController>(Context.GetOwner());
	const UWorld* World = IsValid(Controller) ? Controller->GetWorld() : nullptr;
	return IsValid(World) && World->GetTimeSeconds() < InstanceData.EndTimeSeconds
		? EStateTreeRunStatus::Running
		: EStateTreeRunStatus::Succeeded;
}
