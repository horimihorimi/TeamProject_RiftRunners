#include "FSTT_CompleteBossPhaseTransition.h"

#include "GameCore/AI/MonsterAIController.h"
#include "GameCore/Character/BossMonsterCharacter.h"
#include "GameCore/Combat/BossPhaseComponent.h"
#include "GameCore/Combat/EnemyPatternComponent.h"
#include "StateTreeExecutionContext.h"

EStateTreeRunStatus FSTT_CompleteBossPhaseTransition::EnterState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	AMonsterAIController* Controller = Cast<AMonsterAIController>(Context.GetOwner());
	ABossMonsterCharacter* Boss = IsValid(Controller)
		? Cast<ABossMonsterCharacter>(Controller->GetPawn())
		: nullptr;
	UBossPhaseComponent* PhaseComponent = IsValid(Boss) ? Boss->GetBossPhaseComponent() : nullptr;
	UEnemyPatternComponent* PatternComponent = IsValid(Boss) ? Boss->GetEnemyPatternComponent() : nullptr;
	if (!IsValid(PhaseComponent) || !IsValid(PatternComponent) || !PhaseComponent->CompletePhaseTransition())
	{
		return EStateTreeRunStatus::Failed;
	}

	PatternComponent->SetActivePatternSet(PhaseComponent->GetCurrentPatternSet());
	return EStateTreeRunStatus::Succeeded;
}
