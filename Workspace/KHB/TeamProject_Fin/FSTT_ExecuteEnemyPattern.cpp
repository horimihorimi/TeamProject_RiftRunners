#include "FSTT_ExecuteEnemyPattern.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "Engine/World.h"
#include "GameCore/AI/MonsterAIController.h"
#include "GameCore/AI/MonsterCombatDirectorSubsystem.h"
#include "GameCore/Attribute/CharacterAttributeSet.h"
#include "GameCore/Character/CommonEnemyCharacter.h"
#include "GameCore/Combat/EnemyPatternComponent.h"
#include "GameCore/Combat/EnemyPatternMovement.h"
#include "StateTreeExecutionContext.h"

namespace EnemyPatternExecuteStateTree
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
			? FMath::Clamp(ASC->GetNumericAttribute(UCharacterAttributeSet::GetHealthAttribute()) / MaxHealth, 0.0f, 1.0f)
			: 1.0f;
	}

	static FGameplayAbilitySpecHandle FindActiveSpecByTag(
		const UAbilitySystemComponent* ASC,
		const FGameplayTag& AbilityTag)
	{
		if (!IsValid(ASC))
		{
			return FGameplayAbilitySpecHandle();
		}

		for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
		{
			if (Spec.IsActive() && IsValid(Spec.Ability) && Spec.Ability->GetAssetTags().HasTag(AbilityTag))
			{
				return Spec.Handle;
			}
		}

		return FGameplayAbilitySpecHandle();
	}

	static UMonsterCombatDirectorSubsystem* GetCombatDirector(const AController* Controller)
	{
		UWorld* World = IsValid(Controller) ? Controller->GetWorld() : nullptr;
		return IsValid(World) ? World->GetSubsystem<UMonsterCombatDirectorSubsystem>() : nullptr;
	}
}

FSTT_ExecuteEnemyPattern::FSTT_ExecuteEnemyPattern()
{
	bShouldCallTick = true;
	bShouldCopyBoundPropertiesOnTick = false;
	bShouldCopyBoundPropertiesOnExitState = false;
}

EStateTreeRunStatus FSTT_ExecuteEnemyPattern::EnterState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	InstanceData.ActiveSpecHandle = FGameplayAbilitySpecHandle();
	InstanceData.bHoldingAttackToken = false;

	AMonsterAIController* Controller = Cast<AMonsterAIController>(Context.GetOwner());
	ACommonEnemyCharacter* Character = IsValid(Controller)
		? Cast<ACommonEnemyCharacter>(Controller->GetPawn())
		: nullptr;
	UEnemyPatternComponent* PatternComponent = IsValid(Character)
		? Character->GetEnemyPatternComponent()
		: nullptr;
	FEnemyPatternDefinition Pattern;
	if (!IsValid(Controller) || !IsValid(Character) || !IsValid(PatternComponent)
		|| !PatternComponent->GetCurrentPattern(Pattern))
	{
		return EStateTreeRunStatus::Failed;
	}

	// 준비 중 시야가 끊긴 경우에도 실제 GA 활성화 직전에 다시 확인한다.
	if (Pattern.bRequiresVisibleTarget
		&& !Controller->CanAttackTarget(Controller->GetPerceivedTarget()))
	{
		return EStateTreeRunStatus::Failed;
	}

	InstanceData.InterruptPolicy = Pattern.InterruptPolicy;
	InstanceData.bRequiresVisibleTarget = Pattern.bRequiresVisibleTarget;

	/*
	 * 공격 토큰을 못 받으면 시전하지 않고 실패로 빠진다. 그 사이 다른 적이 공격하고,
	 * 이 적은 대기 뒤 다시 고른다. 토큰은 ExitState에서 반드시 돌려준다.
	 */
	if (Pattern.bRequiresAttackToken)
	{
		if (UMonsterCombatDirectorSubsystem* Director = EnemyPatternExecuteStateTree::GetCombatDirector(Controller))
		{
			if (!Director->TryAcquireAttackToken(Character))
			{
				UE_LOG(LogTemp, Verbose, TEXT("[ExecuteEnemyPattern] Blocked | Pawn=%s | Pattern=%s | Reason=NoAttackToken"),
					*GetNameSafe(Character), *Pattern.PatternId.ToString());
				return EStateTreeRunStatus::Failed;
			}

			InstanceData.bHoldingAttackToken = true;
		}
	}

	const IAbilitySystemInterface* AbilitySystemInterface = Cast<IAbilitySystemInterface>(Character);
	InstanceData.AbilitySystemComponent = AbilitySystemInterface
		? AbilitySystemInterface->GetAbilitySystemComponent()
		: nullptr;
	if (!IsValid(InstanceData.AbilitySystemComponent.Get())
		|| !InstanceData.AbilitySystemComponent->TryActivateAbilitiesByTag(FGameplayTagContainer(Pattern.AbilityTag)))
	{
		UE_LOG(LogTemp, Warning, TEXT("[ExecuteEnemyPattern] Ability activation failed | Pawn=%s | Pattern=%s | Ability=%s"),
			*GetNameSafe(Character), *Pattern.PatternId.ToString(), *Pattern.AbilityTag.ToString());
		return EStateTreeRunStatus::Failed;
	}

	if (!PatternComponent->CommitCurrentPattern())
	{
		return EStateTreeRunStatus::Failed;
	}

	// 현재 GA의 연출 중 다음 후보를 미리 골라, 종료 후 선택 지연을 없앤다.
	PatternComponent->SelectPattern(
		Controller->GetPerceivedTarget(),
		EnemyPatternExecuteStateTree::GetHealthRatio(Character),
		true);

	InstanceData.ActiveSpecHandle = EnemyPatternExecuteStateTree::FindActiveSpecByTag(
		InstanceData.AbilitySystemComponent, Pattern.AbilityTag);
	if (!InstanceData.ActiveSpecHandle.IsValid())
	{
		return EStateTreeRunStatus::Succeeded;
	}

	/*
	 * 시전 중 회피를 받을지는 패턴이 정한다.
	 *
	 * 끊어도 되는 행동(Cancel Immediately)은 회피가 끼어들 수 있어야 저격에 대응할 수 있고,
	 * 끝까지 재생해야 하는 행동은 회피로 끊기면 연출이 깨진다.
	 * 회피가 막히면 그 피격의 데미지도 그대로 들어간다(NotifyIncomingAttack).
	 */
	if (Pattern.InterruptPolicy != EEnemyActionInterruptPolicy::CancelImmediately)
	{
		Controller->SetInterruptible(false);
	}
	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FSTT_ExecuteEnemyPattern::Tick(
	FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	if (!IsValid(InstanceData.AbilitySystemComponent.Get()))
	{
		return EStateTreeRunStatus::Failed;
	}

	AMonsterAIController* Controller = Cast<AMonsterAIController>(Context.GetOwner());
	if (InstanceData.bRequiresVisibleTarget
		&& (!IsValid(Controller) || !Controller->CanAttackTarget(Controller->GetPerceivedTarget()))
		&& InstanceData.InterruptPolicy == EEnemyActionInterruptPolicy::CancelImmediately)
	{
		// 발사 함수를 막는 대신 공격 GA 자체를 취소하고 실패 전환을 요청한다.
		InstanceData.AbilitySystemComponent->CancelAbilityHandle(InstanceData.ActiveSpecHandle);
		if (IsValid(Controller) && IsValid(Controller->GetPawn()))
		{
			TInlineComponentArray<UActorComponent*> Components(Controller->GetPawn());
			for (UActorComponent* Component : Components)
			{
				if (IEnemyPatternMovement* Executor = Cast<IEnemyPatternMovement>(Component))
				{
					Executor->CancelPatternPreparation();
				}
			}
		}
		return EStateTreeRunStatus::Failed;
	}

	const FGameplayAbilitySpec* Spec =
		InstanceData.AbilitySystemComponent->FindAbilitySpecFromHandle(InstanceData.ActiveSpecHandle);
	return Spec != nullptr && Spec->IsActive()
		? EStateTreeRunStatus::Running
		: EStateTreeRunStatus::Succeeded;
}

void FSTT_ExecuteEnemyPattern::ExitState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	AMonsterAIController* Controller = Cast<AMonsterAIController>(Context.GetOwner());

	// 성공이든 실패든 이 상태를 벗어나면 토큰은 반드시 돌려준다.
	if (InstanceData.bHoldingAttackToken)
	{
		if (UMonsterCombatDirectorSubsystem* Director = EnemyPatternExecuteStateTree::GetCombatDirector(Controller))
		{
			Director->ReleaseAttackToken(Controller->GetPawn());
		}
		InstanceData.bHoldingAttackToken = false;
	}

	if (IsValid(Controller))
	{
		Controller->SetInterruptible(true);
	}

	/*
	 * GA가 아직 돌고 있다면 시전 도중 다른 상태(피격, 페이즈 전환 등)로 넘어간 것이다.
	 * Cancel Immediately 면 여기서 끊고, 그 외에는 GA가 스스로 끝나게 둔다.
	 */
	UAbilitySystemComponent* ASC = InstanceData.AbilitySystemComponent.Get();
	if (InstanceData.InterruptPolicy == EEnemyActionInterruptPolicy::CancelImmediately
		&& IsValid(ASC) && InstanceData.ActiveSpecHandle.IsValid())
	{
		const FGameplayAbilitySpec* Spec = ASC->FindAbilitySpecFromHandle(InstanceData.ActiveSpecHandle);
		if (Spec != nullptr && Spec->IsActive())
		{
			ASC->CancelAbilityHandle(InstanceData.ActiveSpecHandle);
		}
	}

	InstanceData.ActiveSpecHandle = FGameplayAbilitySpecHandle();
}
