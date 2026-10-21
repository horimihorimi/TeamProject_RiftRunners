#include "FSTT_PrepareEnemyPattern.h"

#include "GameCore/AI/MonsterAIController.h"
#include "GameCore/Character/CommonEnemyCharacter.h"
#include "GameCore/Combat/EnemyPatternComponent.h"
#include "GameCore/Combat/EnemyPatternMovement.h"
#include "GameCore/Combat/MonsterCombatPositioningComponent.h"
#include "StateTreeExecutionContext.h"

namespace EnemyPatternStateTree
{
	static IEnemyPatternMovement* FindMovementExecutor(AActor* Owner, UActorComponent*& OutComponent)
	{
		OutComponent = nullptr;
		if (!IsValid(Owner))
		{
			return nullptr;
		}

		TInlineComponentArray<UActorComponent*> Components(Owner);
		for (UActorComponent* Component : Components)
		{
			if (IEnemyPatternMovement* Executor = Cast<IEnemyPatternMovement>(Component))
			{
				OutComponent = Component;
				return Executor;
			}
		}

		return nullptr;
	}

	static EStateTreeRunStatus ToRunStatus(const EEnemyPatternPreparationStatus Status)
	{
		switch (Status)
		{
		case EEnemyPatternPreparationStatus::Ready:
			return EStateTreeRunStatus::Succeeded;
		case EEnemyPatternPreparationStatus::Preparing:
			return EStateTreeRunStatus::Running;
		default:
			return EStateTreeRunStatus::Failed;
		}
	}
}

FSTT_PrepareEnemyPattern::FSTT_PrepareEnemyPattern()
{
	bShouldCallTick = true;
	bShouldCopyBoundPropertiesOnTick = false;
	bShouldCopyBoundPropertiesOnExitState = false;
}

EStateTreeRunStatus FSTT_PrepareEnemyPattern::EnterState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	InstanceData.MovementExecutor = nullptr;
	InstanceData.RepositionExecutor = nullptr;
	InstanceData.bMoveDuringAbility = false;

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

	if (Pattern.bRequiresVisibleTarget
		&& (!IsValid(Controller) || !Controller->CanAttackTarget(Controller->GetPerceivedTarget())))
	{
		return EStateTreeRunStatus::Failed;
	}

	FEnemyPatternMovementOption MovementOption;
	if (!PatternComponent->GetCurrentMovementOption(MovementOption)
		|| MovementOption.Intent == EEnemyMovementIntent::None)
	{
		// 이동 후보가 없는 패턴은 제자리에서 바로 시전한다.
		// 직전 기본 움직임의 속도가 남아 있으면 시전 중에 미끄러지므로 여기서 정리한다.
		UActorComponent* IdleMovementComponent = nullptr;
		if (IEnemyPatternMovement* Executor = EnemyPatternStateTree::FindMovementExecutor(Character, IdleMovementComponent))
		{
			Executor->CancelPatternPreparation();
		}
		return EStateTreeRunStatus::Succeeded;
	}

	/*
	 * 재배치는 걷거나 나는 이동이 아니라 순간이동 절차라 위치 보정 컴포넌트에 맡긴다.
	 * 지상형과 비행형이 같은 경로를 탄다.
	 */
	if (MovementOption.Intent == EEnemyMovementIntent::RepositionForAction)
	{
		UMonsterCombatPositioningComponent* PositioningComponent =
			Character->FindComponentByClass<UMonsterCombatPositioningComponent>();
		if (!IsValid(PositioningComponent))
		{
			UE_LOG(LogTemp, Warning, TEXT("[PrepareEnemyPattern] Missing positioning component | Pawn=%s | Pattern=%s"),
				*GetNameSafe(Character), *Pattern.PatternId.ToString());
			return EStateTreeRunStatus::Failed;
		}

		InstanceData.RepositionExecutor = PositioningComponent;
		return EnemyPatternStateTree::ToRunStatus(
			PositioningComponent->BeginReposition(Pattern.AbilityTag, Controller->GetPerceivedTarget()));
	}

	UActorComponent* MovementComponent = nullptr;
	IEnemyPatternMovement* Executor = EnemyPatternStateTree::FindMovementExecutor(Character, MovementComponent);
	if (Executor == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("[PrepareEnemyPattern] Missing movement executor | Pawn=%s | Pattern=%s"),
			*GetNameSafe(Character), *Pattern.PatternId.ToString());
		return EStateTreeRunStatus::Failed;
	}

	InstanceData.MovementExecutor = MovementComponent;
	InstanceData.bMoveDuringAbility = MovementOption.bMoveDuringAbility;

	const EEnemyPatternPreparationStatus Status =
		Executor->BeginPatternPreparation(Pattern, MovementOption, Controller->GetPerceivedTarget());

	/*
	 * 몽타주와 병행하는 이동은 여기서 기다리지 않는다.
	 *
	 * 준비가 끝나기를 기다리면 몽타주가 이동 후에야 시작되어, 이동을 애니메이션에
	 * 겹쳐 넣으려던 의도가 사라진다. 시작만 걸어 두고 즉시 통과시킨 뒤
	 * 몽타주의 노티파이가 이동을 끊는다.
	 */
	if (InstanceData.bMoveDuringAbility && Status != EEnemyPatternPreparationStatus::Failed)
	{
		return EStateTreeRunStatus::Succeeded;
	}

	return EnemyPatternStateTree::ToRunStatus(Status);
}

EStateTreeRunStatus FSTT_PrepareEnemyPattern::Tick(
	FStateTreeExecutionContext& Context, const float DeltaTime) const
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
	FEnemyPatternMovementOption MovementOption;
	IEnemyPatternMovement* Executor = Cast<IEnemyPatternMovement>(InstanceData.MovementExecutor);
	if (!IsValid(Controller) || !IsValid(PatternComponent)
		|| !PatternComponent->GetCurrentPattern(Pattern)
		|| !PatternComponent->GetCurrentMovementOption(MovementOption))
	{
		return EStateTreeRunStatus::Failed;
	}

	if (Pattern.bRequiresVisibleTarget && !Controller->CanAttackTarget(Controller->GetPerceivedTarget()))
	{
		return EStateTreeRunStatus::Failed;
	}
	if (UMonsterCombatPositioningComponent* PositioningComponent = InstanceData.RepositionExecutor.Get())
	{
		return EnemyPatternStateTree::ToRunStatus(PositioningComponent->UpdateReposition());
	}
	if (Executor == nullptr) { return EStateTreeRunStatus::Failed; }

	return EnemyPatternStateTree::ToRunStatus(
		Executor->UpdatePatternPreparation(Pattern, MovementOption, Controller->GetPerceivedTarget(), DeltaTime));
}

void FSTT_PrepareEnemyPattern::ExitState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	// 재배치가 끝나기 전에 빠져나가면 예약만 취소한다. 이미 시작된 순간이동은 GA가 마무리한다.
	if (UMonsterCombatPositioningComponent* PositioningComponent = InstanceData.RepositionExecutor.Get())
	{
		PositioningComponent->CancelReposition();
		InstanceData.RepositionExecutor = nullptr;
		return;
	}

	/*
	 * 몽타주와 병행하는 이동은 이 상태를 빠져나간 뒤에도 계속되어야 한다.
	 * 여기서 취소하면 GA가 시작되기도 전에 이동이 끊긴다.
	 */
	if (InstanceData.bMoveDuringAbility)
	{
		return;
	}

	if (IEnemyPatternMovement* Executor = Cast<IEnemyPatternMovement>(InstanceData.MovementExecutor))
	{
		Executor->CancelPatternPreparation();
	}
}

