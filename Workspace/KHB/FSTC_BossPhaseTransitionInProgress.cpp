#include "FSTC_BossPhaseTransitionInProgress.h"

#include "GameCore/AI/MonsterAIController.h"
#include "GameCore/Character/BossMonsterCharacter.h"
#include "GameCore/Combat/BossPhaseComponent.h"
#include "StateTreeExecutionContext.h"

bool FSTC_BossPhaseTransitionInProgress::TestCondition(FStateTreeExecutionContext& Context) const
{
	const AMonsterAIController* Controller = Cast<AMonsterAIController>(Context.GetOwner());
	const ABossMonsterCharacter* Boss = IsValid(Controller)
		? Cast<ABossMonsterCharacter>(Controller->GetPawn())
		: nullptr;
	const UBossPhaseComponent* PhaseComponent = IsValid(Boss)
		? Boss->GetBossPhaseComponent()
		: nullptr;

	return IsValid(PhaseComponent) && PhaseComponent->IsPhaseTransitionInProgress();
}
