#include "FSTT_ChooseWeightedAction.h"

#include "AIController.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "Abilities/GameplayAbility.h"
#include "GameCore/AI/MonsterAIController.h"
#include "GameCore/Attribute/CharacterAttributeSet.h"
#include "StateTreeExecutionContext.h"

namespace
{
	const UAbilitySystemComponent* FindAbilitySystemComponent(const APawn* OwnerPawn)
	{
		const IAbilitySystemInterface* AbilitySystemInterface =
			Cast<IAbilitySystemInterface>(OwnerPawn);

		return AbilitySystemInterface != nullptr
			? AbilitySystemInterface->GetAbilitySystemComponent()
			: nullptr;
	}

	/*
	 * 자신의 현재 체력 비율(0~1)을 구한다.
	 *
	 * ASC나 MaxHealth를 못 읽으면 1.0을 돌려준다. 체력을 모르는 상태에서
	 * 후보를 걸러내면 행동이 통째로 사라져 몹이 멈춰버리므로, 모를 때는
	 * 체력 조건을 걸지 않은 것과 같게 둔다.
	 */
	float GetSelfHealthRatio(const UAbilitySystemComponent* AbilitySystemComponent)
	{
		if (!IsValid(AbilitySystemComponent))
		{
			return 1.0f;
		}

		const float MaxHealth = AbilitySystemComponent->GetNumericAttribute(
			UCharacterAttributeSet::GetMaxHealthAttribute());

		if (MaxHealth <= 0.0f)
		{
			return 1.0f;
		}

		const float Health = AbilitySystemComponent->GetNumericAttribute(
			UCharacterAttributeSet::GetHealthAttribute());

		return FMath::Clamp(Health / MaxHealth, 0.0f, 1.0f);
	}

	/*
	 * 이 액션 태그의 어빌리티를 지금 발동할 수 있는지 어빌리티 본인에게 묻는다.
	 *
	 * 쿨다운만 보는 것이 아니라 코스트, ActivationBlockedTags, 그리고
	 * GA_Possession처럼 자체 CanActivateAbility에서 대상을 찾는 어빌리티까지
	 * 한 번에 반영된다. 같은 조건을 StateTree 데이터에 옮겨 적으면 어빌리티를
	 * 고칠 때 한쪽만 바뀌어 조용히 어긋난다.
	 *
	 * 부여되지 않은 어빌리티는 후보에서 뺀다. 뽑아봐야 발동에 실패하고
	 * 그 자리에서 다시 뽑게 되므로, 애초에 후보가 아닌 것이 맞다.
	 *
	 * ASC를 못 찾으면 막지 않는다. 조건을 확인할 수 없다는 이유로 행동을
	 * 지우면 몹이 아무것도 못 하고 멈춘다.
	 */
	bool CanActivateAction(
		const UAbilitySystemComponent* AbilitySystemComponent,
		const FGameplayTag& ActionTag)
	{
		if (!IsValid(AbilitySystemComponent))
		{
			return true;
		}

		for (const FGameplayAbilitySpec& Spec : AbilitySystemComponent->GetActivatableAbilities())
		{
			if (!IsValid(Spec.Ability) || !Spec.Ability->GetAssetTags().HasTag(ActionTag))
			{
				continue;
			}

			/*
			 * InstancedPerActor 어빌리티는 인스턴스가 상태를 들고 있으므로
			 * CDO가 아니라 그쪽에 물어야 한다.
			 */
			const UGameplayAbility* AbilityToAsk = Spec.GetPrimaryInstance()
				? Spec.GetPrimaryInstance()
				: Spec.Ability.Get();

			return IsValid(AbilityToAsk)
				&& AbilityToAsk->CanActivateAbility(
					Spec.Handle,
					AbilitySystemComponent->AbilityActorInfo.Get());
		}

		return false;
	}
}

EStateTreeRunStatus FSTT_ChooseWeightedAction::EnterState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	AAIController* AIController = Cast<AAIController>(Context.GetOwner());
	AMonsterAIController* MonsterController = Cast<AMonsterAIController>(AIController);

	// Never let a failed selection execute the action selected in a previous cycle.
	InstanceData.SelectedAction = FGameplayTag();
	if (IsValid(MonsterController))
	{
		MonsterController->SetSelectedAction(FGameplayTag());
	}

	const APawn* OwnerPawn = IsValid(AIController) ? AIController->GetPawn() : nullptr;

	if (!IsValid(OwnerPawn) || !IsValid(InstanceData.TargetActor))
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[ChooseWeightedAction] OwnerPawn 또는 TargetActor가 유효하지 않습니다."));

		return EStateTreeRunStatus::Failed;
	}

	const float Distance = FVector::Dist(
		OwnerPawn->GetActorLocation(),
		InstanceData.TargetActor->GetActorLocation());

	// 체력과 발동 가능 여부를 모두 여기서 보므로 ASC는 한 번만 찾는다.
	const UAbilitySystemComponent* AbilitySystemComponent =
		FindAbilitySystemComponent(OwnerPawn);

	const float HealthRatio = GetSelfHealthRatio(AbilitySystemComponent);

	// 거리·체력·발동 가능 여부에 모두 맞는 후보만 걸러낸다.
	TArray<const FWeightedActionChoice*> ValidChoices;
	float TotalWeight = 0.0f;

	for (const FWeightedActionChoice& Choice : InstanceData.Choices)
	{
		// Zero-weight entries are disabled choices, not fallback candidates.
		if (!Choice.Action.IsValid() || Choice.Weight <= 0.0f)
		{
			continue;
		}

		const bool bAboveMin = Choice.MinDistance <= 0.0f || Distance >= Choice.MinDistance;
		const bool bBelowMax = Choice.MaxDistance <= 0.0f || Distance <= Choice.MaxDistance;

		const bool bHealthInRange =
			HealthRatio >= Choice.MinHealthRatio && HealthRatio <= Choice.MaxHealthRatio;

		if (!bAboveMin || !bBelowMax || !bHealthInRange)
		{
			continue;
		}

		/*
		 * 발동 가능 여부는 마지막에 본다. 어빌리티를 순회하고 CanActivateAbility를
		 * 부르는 비용이 거리·체력 비교보다 크므로, 값싼 조건으로 먼저 줄인다.
		 */
		if (!CanActivateAction(AbilitySystemComponent, Choice.Action))
		{
			continue;
		}

		ValidChoices.Add(&Choice);
		TotalWeight += Choice.Weight;
	}

	if (ValidChoices.IsEmpty())
	{
		/*
		 * 쿨다운 때문에 비었을 수도 있어서 거리·체력만으로는 원인을 못 가린다.
		 * 조건 없는 후보를 하나 넣어두면 이 경로 자체가 생기지 않는다.
		 */
		UE_LOG(LogTemp, Warning,
			TEXT("[ChooseWeightedAction] 후보가 없습니다. 거리 %.1f / 체력비율 %.2f ")
			TEXT("(쿨다운이나 어빌리티 자체 조건으로 전부 걸러졌을 수 있음)"),
			Distance,
			HealthRatio);

		return EStateTreeRunStatus::Failed;
	}

	FGameplayTag ChosenAction = ValidChoices.Last()->Action;
	float Roll = FMath::FRandRange(0.0f, TotalWeight);

	for (const FWeightedActionChoice* Choice : ValidChoices)
	{
		Roll -= Choice->Weight;

		if (Roll <= 0.0f)
		{
			ChosenAction = Choice->Action;
			break;
		}
	}

	InstanceData.SelectedAction = ChosenAction;

	/*
	 * 형제 State끼리는 이 Task의 출력을 직접 못 보므로, 최상위
	 * FSTE_MonsterCombatState가 매 틱 중계할 수 있게 컨트롤러에도 넣어둔다.
	 */
	if (IsValid(MonsterController))
	{
		MonsterController->SetSelectedAction(ChosenAction);
	}

	return EStateTreeRunStatus::Succeeded;
}
