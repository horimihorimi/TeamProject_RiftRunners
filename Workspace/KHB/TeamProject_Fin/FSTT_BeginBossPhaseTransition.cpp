#include "FSTT_BeginBossPhaseTransition.h"

#include "AbilitySystemComponent.h"
#include "GameCore/AI/MonsterAIController.h"
#include "GameCore/Character/BossMonsterCharacter.h"
#include "GameCore/Combat/BossPhaseComponent.h"
#include "GameCore/Combat/EnemyPatternComponent.h"
#include "GameCore/Movement/BossMovementComponent.h"
#include "StateTreeExecutionContext.h"

EStateTreeRunStatus FSTT_BeginBossPhaseTransition::EnterState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	AMonsterAIController* Controller = Cast<AMonsterAIController>(Context.GetOwner());
	ABossMonsterCharacter* Boss = IsValid(Controller)
		? Cast<ABossMonsterCharacter>(Controller->GetPawn())
		: nullptr;
	UBossPhaseComponent* PhaseComponent = IsValid(Boss) ? Boss->GetBossPhaseComponent() : nullptr;
	if (!IsValid(Boss) || !IsValid(PhaseComponent) || !PhaseComponent->IsPhaseTransitionInProgress())
	{
		return EStateTreeRunStatus::Failed;
	}

	UEnemyPatternComponent* PatternComponent = Boss->GetEnemyPatternComponent();
	FEnemyPatternDefinition CurrentPattern;
	if (IsValid(PatternComponent) && PatternComponent->GetCurrentPattern(CurrentPattern))
	{
		if (UAbilitySystemComponent* ASC = Boss->GetAbilitySystemComponent())
		{
			const FGameplayTagContainer CurrentPatternTag(CurrentPattern.AbilityTag);
			ASC->CancelAbilities(&CurrentPatternTag, nullptr, nullptr);
		}
	}

	if (IsValid(PatternComponent))
	{
		PatternComponent->ClearRuntimeState();
	}
	if (UBossMovementComponent* Movement = Cast<UBossMovementComponent>(Boss->GetCharacterMovement()))
	{
		Movement->CancelPatternPreparation();
	}
	Controller->SetInterruptible(true);
	return EStateTreeRunStatus::Succeeded;
}
