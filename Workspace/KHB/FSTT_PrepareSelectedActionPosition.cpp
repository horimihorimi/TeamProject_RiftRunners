#include "FSTT_PrepareSelectedActionPosition.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "GameCore/AI/MonsterAIController.h"
#include "GameCore/Combat/MonsterCombatPositioningComponent.h"
#include "StateTreeExecutionContext.h"

FSTT_PrepareSelectedActionPosition::FSTT_PrepareSelectedActionPosition()
{
	bShouldCallTick = true;
	bShouldCopyBoundPropertiesOnTick = false;
	bShouldCopyBoundPropertiesOnExitState = false;
}

EStateTreeRunStatus FSTT_PrepareSelectedActionPosition::EnterState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	AMonsterAIController* Controller = Cast<AMonsterAIController>(Context.GetOwner());
	if (!IsValid(Controller))
	{
		return EStateTreeRunStatus::Failed;
	}

	APawn* Pawn = Controller->GetPawn();
	AActor* TargetActor = Controller->GetPerceivedTarget();
	UMonsterCombatPositioningComponent* PositioningComponent =
		IsValid(Pawn) ? Pawn->FindComponentByClass<UMonsterCombatPositioningComponent>() : nullptr;
	if (!IsValid(Pawn) || !IsValid(TargetActor) || !IsValid(PositioningComponent))
	{
		return EStateTreeRunStatus::Failed;
	}

	FGameplayTag RepositionAbilityTag;
	const EMonsterActionPositionPreparationResult Result =
		PositioningComponent->PrepareSelectedAction(
			TargetActor,
			Controller->GetSelectedAction(),
			RepositionAbilityTag);

	if (Result == EMonsterActionPositionPreparationResult::Ready)
	{
		return EStateTreeRunStatus::Succeeded;
	}

	if (Result == EMonsterActionPositionPreparationResult::Failed || !RepositionAbilityTag.IsValid())
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[EliteTeleport] PrepareFailed | Pawn=%s | Action=%s | Result=%d | RepositionAbility=%s"),
			*GetNameSafe(Pawn),
			*Controller->GetSelectedAction().ToString(),
			static_cast<int32>(Result),
			*RepositionAbilityTag.ToString());
		return EStateTreeRunStatus::Failed;
	}

	const IAbilitySystemInterface* AbilitySystemInterface = Cast<IAbilitySystemInterface>(Pawn);
	InstanceData.AbilitySystemComponent = AbilitySystemInterface
		? AbilitySystemInterface->GetAbilitySystemComponent()
		: nullptr;
	if (!IsValid(InstanceData.AbilitySystemComponent.Get())
		|| !InstanceData.AbilitySystemComponent->TryActivateAbilitiesByTag(FGameplayTagContainer(RepositionAbilityTag)))
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[EliteTeleport] AbilityActivationFailed | Pawn=%s | Action=%s | RepositionAbility=%s"),
			*GetNameSafe(Pawn),
			*Controller->GetSelectedAction().ToString(),
			*RepositionAbilityTag.ToString());
		PositioningComponent->CancelPendingReposition();
		return EStateTreeRunStatus::Failed;
	}

	for (const FGameplayAbilitySpec& Spec : InstanceData.AbilitySystemComponent->GetActivatableAbilities())
	{
		if (Spec.IsActive() && IsValid(Spec.Ability) && Spec.Ability->GetAssetTags().HasTag(RepositionAbilityTag))
		{
			InstanceData.ActiveSpecHandle = Spec.Handle;
			InstanceData.bWaitingForRepositionAbility = true;
			UE_LOG(LogTemp, Log,
				TEXT("[EliteTeleport] AbilityActivated | Pawn=%s | Action=%s | RepositionAbility=%s"),
				*GetNameSafe(Pawn),
				*Controller->GetSelectedAction().ToString(),
				*RepositionAbilityTag.ToString());
			return EStateTreeRunStatus::Running;
		}
	}

	// 즉시 종료된 Ability는 성공 여부를 컴포넌트에서 확인한다.
	const bool bSucceeded = PositioningComponent->WasLastRepositionSuccessful();
	if (bSucceeded)
	{
		UE_LOG(LogTemp, Log,
			TEXT("[EliteTeleport] AbilityCompleted | Pawn=%s | Success=True"),
			*GetNameSafe(Pawn));
	}
	else
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[EliteTeleport] AbilityCompleted | Pawn=%s | Success=False"),
			*GetNameSafe(Pawn));
	}
	return bSucceeded ? EStateTreeRunStatus::Succeeded : EStateTreeRunStatus::Failed;
}

EStateTreeRunStatus FSTT_PrepareSelectedActionPosition::Tick(
	FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	AMonsterAIController* Controller = Cast<AMonsterAIController>(Context.GetOwner());
	APawn* Pawn = IsValid(Controller) ? Controller->GetPawn() : nullptr;
	UMonsterCombatPositioningComponent* PositioningComponent =
		IsValid(Pawn) ? Pawn->FindComponentByClass<UMonsterCombatPositioningComponent>() : nullptr;
	if (!InstanceData.bWaitingForRepositionAbility
		|| !IsValid(InstanceData.AbilitySystemComponent.Get())
		|| !IsValid(PositioningComponent))
	{
		return EStateTreeRunStatus::Failed;
	}

	const FGameplayAbilitySpec* Spec =
		InstanceData.AbilitySystemComponent->FindAbilitySpecFromHandle(InstanceData.ActiveSpecHandle);
	if (Spec != nullptr && Spec->IsActive())
	{
		return EStateTreeRunStatus::Running;
	}

	return PositioningComponent->WasLastRepositionSuccessful()
		? EStateTreeRunStatus::Succeeded
		: EStateTreeRunStatus::Failed;
}

void FSTT_PrepareSelectedActionPosition::ExitState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	AMonsterAIController* Controller = Cast<AMonsterAIController>(Context.GetOwner());
	APawn* Pawn = IsValid(Controller) ? Controller->GetPawn() : nullptr;
	if (UMonsterCombatPositioningComponent* PositioningComponent =
		IsValid(Pawn) ? Pawn->FindComponentByClass<UMonsterCombatPositioningComponent>() : nullptr)
	{
		if (InstanceData.bWaitingForRepositionAbility && PositioningComponent->HasPendingReposition())
		{
			PositioningComponent->CancelPendingReposition();
		}
	}
}
