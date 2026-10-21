#include "FSTE_MonsterCombatState.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "GameCore/AI/MonsterAIController.h"
#include "GameCore/Tags/GameplayTagSystem.h"
#include "StateTreeExecutionContext.h"

void FSTE_MonsterCombatState::Tick(
	FStateTreeExecutionContext& Context,
	const float DeltaTime) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	AMonsterAIController* Controller =
		Cast<AMonsterAIController>(Context.GetOwner());

	if (!IsValid(Controller))
	{
		return;
	}

	const IAbilitySystemInterface* AbilitySystemInterface =
		Cast<IAbilitySystemInterface>(Controller->GetPawn());

	const UAbilitySystemComponent* AbilitySystemComponent =
		AbilitySystemInterface != nullptr
			? AbilitySystemInterface->GetAbilitySystemComponent()
			: nullptr;

	InstanceData.bIsDormant =
		!IsValid(AbilitySystemComponent) ||
		AbilitySystemComponent->HasMatchingGameplayTag(
			GameplayTagSystem::State_AI_Dormant);

	InstanceData.bHasEncountered =
		IsValid(AbilitySystemComponent) &&
		AbilitySystemComponent->HasMatchingGameplayTag(
			GameplayTagSystem::State_AI_Encountered);

	InstanceData.bIsGroggy =
		IsValid(AbilitySystemComponent) &&
		AbilitySystemComponent->HasMatchingGameplayTag(
			GameplayTagSystem::State_Groggy);

	InstanceData.SelectedAction = Controller->GetSelectedAction();

}
