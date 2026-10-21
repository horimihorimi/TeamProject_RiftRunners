#include "FSTT_EnemyIdleMovement.h"

#include "AbilitySystemComponent.h"
#include "GameCore/AI/MonsterAIController.h"
#include "GameCore/Attribute/CharacterAttributeSet.h"
#include "GameCore/Character/CommonEnemyCharacter.h"
#include "GameCore/Combat/EnemyPatternComponent.h"
#include "GameCore/Combat/EnemyPatternMovement.h"
#include "StateTreeExecutionContext.h"

namespace EnemyIdleMovementStateTree
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

	static float GetHealthRatio(const ACommonEnemyCharacter* Character)
	{
		const UAbilitySystemComponent* ASC = IsValid(Character) ? Character->GetAbilitySystemComponent() : nullptr;
		if (!IsValid(ASC))
		{
			return 1.0f;
		}

		const float MaxHealth = ASC->GetNumericAttribute(UCharacterAttributeSet::GetMaxHealthAttribute());
		return MaxHealth > 0.0f
			? FMath::Clamp(ASC->GetNumericAttribute(UCharacterAttributeSet::GetHealthAttribute()) / MaxHealth, 0.0f, 1.0f)
			: 1.0f;
	}
}

FSTT_EnemyIdleMovement::FSTT_EnemyIdleMovement()
{
	bShouldCallTick = true;
	bShouldCopyBoundPropertiesOnTick = false;
	bShouldCopyBoundPropertiesOnExitState = false;
}

EStateTreeRunStatus FSTT_EnemyIdleMovement::EnterState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	InstanceData.MovementExecutor = nullptr;
	InstanceData.FinishableSinceTime = -1.0;

	AMonsterAIController* Controller = Cast<AMonsterAIController>(Context.GetOwner());
	ACommonEnemyCharacter* Character = IsValid(Controller)
		? Cast<ACommonEnemyCharacter>(Controller->GetPawn())
		: nullptr;

	if (!IsValid(Controller) || !IsValid(Character))
	{
		return EStateTreeRunStatus::Failed;
	}

	/*
	 * 기본 움직임이 없는 이동 방식이면 기다리지 않고 넘어간다(IEnemyPatternMovement 계약).
	 * 일반 적은 평소 이동을 컨트롤러 추격이 맡으므로 여기서 할 일이 없다.
	 */
	UActorComponent* MovementComponent = nullptr;
	IEnemyPatternMovement* Executor = EnemyIdleMovementStateTree::FindMovementExecutor(Character, MovementComponent);
	if (Executor == nullptr || !Executor->BeginIdleMovement(Controller->GetPerceivedTarget()))
	{
		UE_LOG(LogTemp, Verbose,
			TEXT("[EnemyIdleMovement] Skipped | Pawn=%s | Executor=%s"),
			*GetNameSafe(Character), *GetNameSafe(MovementComponent));
		return EStateTreeRunStatus::Succeeded;
	}

	InstanceData.MovementExecutor = MovementComponent;
	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FSTT_EnemyIdleMovement::Tick(
	FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	const IEnemyPatternMovement* Executor = Cast<IEnemyPatternMovement>(InstanceData.MovementExecutor);
	if (Executor == nullptr)
	{
		return EStateTreeRunStatus::Failed;
	}

	if (!Executor->CanFinishIdleMovement())
	{
		return EStateTreeRunStatus::Running;
	}

	AMonsterAIController* Controller = Cast<AMonsterAIController>(Context.GetOwner());
	ACommonEnemyCharacter* Character = IsValid(Controller)
		? Cast<ACommonEnemyCharacter>(Controller->GetPawn())
		: nullptr;
	const UEnemyPatternComponent* PatternComponent = IsValid(Character)
		? Character->GetEnemyPatternComponent()
		: nullptr;

	// 기다릴 다음 패턴이 없으면 끝내도 되는 즉시 넘어가 새로 고르게 한다.
	if (!InstanceData.bWaitForPendingPattern || !IsValid(PatternComponent) || !PatternComponent->HasPendingPattern())
	{
		return EStateTreeRunStatus::Succeeded;
	}

	if (PatternComponent->ValidatePendingPattern(
		Controller->GetPerceivedTarget(),
		EnemyIdleMovementStateTree::GetHealthRatio(Character)))
	{
		return EStateTreeRunStatus::Succeeded;
	}

	// 다음 패턴이 아직 조건에 맞지 않으면 움직임을 유지한 채 정해진 시간까지만 더 기다린다.
	const double Now = Controller->GetWorld()->GetTimeSeconds();
	if (InstanceData.FinishableSinceTime < 0.0)
	{
		InstanceData.FinishableSinceTime = Now;
	}

	return Now - InstanceData.FinishableSinceTime >= InstanceData.MaxExtraWaitDuration
		? EStateTreeRunStatus::Succeeded
		: EStateTreeRunStatus::Running;
}

void FSTT_EnemyIdleMovement::ExitState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	if (IEnemyPatternMovement* Executor = Cast<IEnemyPatternMovement>(InstanceData.MovementExecutor))
	{
		Executor->EndIdleMovement();
	}
	InstanceData.MovementExecutor = nullptr;
}
