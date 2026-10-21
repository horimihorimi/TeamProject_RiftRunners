#include "FSTT_SelectEnemyPattern.h"

#include "AbilitySystemComponent.h"
#include "GameCore/AI/MonsterAIController.h"
#include "GameCore/Attribute/CharacterAttributeSet.h"
#include "GameCore/Character/CommonEnemyCharacter.h"
#include "GameCore/Combat/EnemyPatternComponent.h"
#include "GameCore/Tags/GameplayTagSystem.h"
#include "StateTreeExecutionContext.h"

namespace EnemyPatternSelectStateTree
{
static float GetHealthRatio(const ACommonEnemyCharacter* Character)
{
	const UAbilitySystemComponent* ASC = IsValid(Character) ? Character->GetAbilitySystemComponent() : nullptr;
	if (!IsValid(ASC))
	{
		return 1.0f;
	}

	const float MaxHealth = ASC->GetNumericAttribute(UCharacterAttributeSet::GetMaxHealthAttribute());
	return MaxHealth > 0.0f
	           ? FMath::Clamp(ASC->GetNumericAttribute(UCharacterAttributeSet::GetHealthAttribute()) / MaxHealth, 0.0f,
	                          1.0f)
	           : 1.0f;
}

static bool IsActionCommitted(const ACommonEnemyCharacter* Character)
{
	const UAbilitySystemComponent* ASC = IsValid(Character) ? Character->GetAbilitySystemComponent() : nullptr;
	return IsValid(ASC) && ASC->HasMatchingGameplayTag(GameplayTagSystem::State_AI_ActionCommitted);
}
}

FSTT_SelectEnemyPattern::FSTT_SelectEnemyPattern()
{
	// 커밋된 행동을 기다리는 동안만 Tick이 일한다.
	bShouldCallTick = true;
}

EStateTreeRunStatus FSTT_SelectEnemyPattern::EnterState(FStateTreeExecutionContext& Context,
                                                        const FStateTreeTransitionResult& Transition) const
{
	return TrySelect(Context);
}

EStateTreeRunStatus FSTT_SelectEnemyPattern::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	return TrySelect(Context);
}

EStateTreeRunStatus FSTT_SelectEnemyPattern::TrySelect(FStateTreeExecutionContext& Context) const
{
	AMonsterAIController* Controller = Cast<AMonsterAIController>(Context.GetOwner());
	ACommonEnemyCharacter* Character =
	    IsValid(Controller) ? Cast<ACommonEnemyCharacter>(Controller->GetPawn()) : nullptr;
	UEnemyPatternComponent* PatternComponent = IsValid(Character) ? Character->GetEnemyPatternComponent() : nullptr;

	/*
	 * 타깃을 잃어도 계속하는 GA(Continue Without Target, Finish Committed Action)는 실행 상태를 빠져나간 뒤에도 돈다.
	 * 그동안 다음 패턴을 실행하면 새 몽타주가 그 GA의 몽타주를 끊으므로, 태그가 빠질 때까지 여기서 기다린다.
	 * 태그는 GA의 ActivationOwnedTags 로만 붙어 GA가 끝나면 GAS가 떼므로 영원히 기다리는 일은 없다.
	 */
	if (EnemyPatternSelectStateTree::IsActionCommitted(Character))
	{
		return EStateTreeRunStatus::Running;
	}

	/*
	 * 사거리 밖이면 고를 패턴이 없어 매 프레임 실패하는 것이 정상이다. 진입 로그는 Verbose로 두고,
	 * 실패 이유는 EnemyPatternComponent가 1초에 한 번 Warning으로 남긴다.
	 */
	UE_LOG(
	    LogTemp, Verbose,
	    TEXT(
	        "[SelectEnemyPatternTask] Enter | Controller=%s | Pawn=%s | Authority=%s | PatternComponent=%s | ActiveSet=%s"),
	    *GetNameSafe(Controller), *GetNameSafe(Character),
	    IsValid(Character) && Character->HasAuthority() ? TEXT("True") : TEXT("False"), *GetNameSafe(PatternComponent),
	    IsValid(PatternComponent) ? *GetNameSafe(PatternComponent->GetActivePatternSet()) : TEXT("None"));

	if (!IsValid(Controller) || !IsValid(PatternComponent))
	{
		UE_LOG(LogTemp, Warning,
		       TEXT("[SelectEnemyPatternTask] Exit=Failed | Reason=ControllerOrPatternComponentInvalid"));
		return EStateTreeRunStatus::Failed;
	}

	const float HealthRatio = EnemyPatternSelectStateTree::GetHealthRatio(Character);
	AActor* TargetActor = Controller->GetPerceivedTarget();
	if (PatternComponent->SelectPrioritySplitPattern(TargetActor, HealthRatio))
	{
		return EStateTreeRunStatus::Succeeded;
	}
	if (PatternComponent->HasPendingPattern() && PatternComponent->ValidatePendingPattern(TargetActor, HealthRatio) &&
	    PatternComponent->PromotePendingPattern())
	{
		UE_LOG(LogTemp, Log, TEXT("[SelectEnemyPatternTask] Exit=Succeeded | Source=Pending | Target=%s | Health=%.3f"),
		       *GetNameSafe(TargetActor), HealthRatio);
		return EStateTreeRunStatus::Succeeded;
	}

	const bool bSelected = PatternComponent->SelectPattern(TargetActor, HealthRatio, false);
	if (bSelected)
	{
		UE_LOG(LogTemp, Log,
		       TEXT("[SelectEnemyPatternTask] Exit=Succeeded | Source=NewSelection | Target=%s | Health=%.3f"),
		       *GetNameSafe(TargetActor), HealthRatio);
		return EStateTreeRunStatus::Succeeded;
	}

	UE_LOG(LogTemp, Verbose,
	       TEXT("[SelectEnemyPatternTask] Exit=Failed | Source=NewSelection | Target=%s | Health=%.3f"),
	       *GetNameSafe(TargetActor), HealthRatio);
	return EStateTreeRunStatus::Failed;
}
