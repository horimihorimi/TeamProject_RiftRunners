#include "FSTT_ExecuteBossPhaseTransition.h"

#include "AbilitySystemComponent.h"
#include "GameCore/AI/MonsterAIController.h"
#include "GameCore/Character/BossMonsterCharacter.h"
#include "GameCore/Combat/BossPhaseComponent.h"
#include "StateTreeExecutionContext.h"

FSTT_ExecuteBossPhaseTransition::FSTT_ExecuteBossPhaseTransition()
{
	bShouldCallTick = true;
	bShouldCopyBoundPropertiesOnTick = false;
	bShouldCopyBoundPropertiesOnExitState = false;
}

EStateTreeRunStatus FSTT_ExecuteBossPhaseTransition::EnterState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	AMonsterAIController* Controller = Cast<AMonsterAIController>(Context.GetOwner());
	ABossMonsterCharacter* Boss = IsValid(Controller)
		? Cast<ABossMonsterCharacter>(Controller->GetPawn())
		: nullptr;
	UBossPhaseComponent* PhaseComponent = IsValid(Boss) ? Boss->GetBossPhaseComponent() : nullptr;
	const FGameplayTag TransitionAbilityTag = IsValid(PhaseComponent)
		? PhaseComponent->GetPendingTransitionAbilityTag()
		: FGameplayTag();
	InstanceData.AbilitySystemComponent = IsValid(Boss) ? Boss->GetAbilitySystemComponent() : nullptr;
	InstanceData.ActiveSpecHandle = FGameplayAbilitySpecHandle();

	if (!IsValid(InstanceData.AbilitySystemComponent.Get()) || !TransitionAbilityTag.IsValid()
		|| !InstanceData.AbilitySystemComponent->TryActivateAbilitiesByTag(FGameplayTagContainer(TransitionAbilityTag)))
	{
		return EStateTreeRunStatus::Failed;
	}

	for (const FGameplayAbilitySpec& Spec : InstanceData.AbilitySystemComponent->GetActivatableAbilities())
	{
		if (Spec.IsActive() && IsValid(Spec.Ability) && Spec.Ability->GetAssetTags().HasTag(TransitionAbilityTag))
		{
			InstanceData.ActiveSpecHandle = Spec.Handle;
			break;
		}
	}

	return InstanceData.ActiveSpecHandle.IsValid() ? EStateTreeRunStatus::Running : EStateTreeRunStatus::Failed;
}

EStateTreeRunStatus FSTT_ExecuteBossPhaseTransition::Tick(
	FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	if (!IsValid(InstanceData.AbilitySystemComponent.Get()))
	{
		return EStateTreeRunStatus::Failed;
	}

	const FGameplayAbilitySpec* Spec = InstanceData.AbilitySystemComponent->FindAbilitySpecFromHandle(InstanceData.ActiveSpecHandle);
	return Spec != nullptr && Spec->IsActive()
		? EStateTreeRunStatus::Running
		: EStateTreeRunStatus::Succeeded;
}
