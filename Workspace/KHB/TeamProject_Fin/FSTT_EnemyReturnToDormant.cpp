#include "FSTT_EnemyReturnToDormant.h"

#include "AbilitySystemComponent.h"
#include "GameCore/AI/MonsterAIController.h"
#include "GameCore/Character/CommonEnemyCharacter.h"
#include "GameCore/Movement/MonsterMovementComponent.h"
#include "GameCore/Tags/GameplayTagSystem.h"
#include "StateTreeExecutionContext.h"

EStateTreeRunStatus FSTT_EnemyReturnToDormant::EnterState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	AMonsterAIController* Controller = Cast<AMonsterAIController>(Context.GetOwner());
	ACommonEnemyCharacter* Character = IsValid(Controller)
		? Cast<ACommonEnemyCharacter>(Controller->GetPawn())
		: nullptr;
	if (!IsValid(Character) || !Character->HasAuthority())
	{
		return EStateTreeRunStatus::Failed;
	}

	if (UAbilitySystemComponent* ASC = Character->GetAbilitySystemComponent())
	{
		ASC->RemoveLooseGameplayTag(GameplayTagSystem::State_AI_Encountered);
		ASC->AddLooseGameplayTag(GameplayTagSystem::State_AI_Dormant);
	}

	if (UMonsterMovementComponent* MovementComponent =
		Cast<UMonsterMovementComponent>(Character->GetCharacterMovement()))
	{
		MovementComponent->ResetMovementState();
	}

	UE_LOG(LogTemp, Warning, TEXT("[EnemyTargetLost] ReturnToDormant | Monster=%s"), *GetNameSafe(Character));
	return EStateTreeRunStatus::Succeeded;
}
