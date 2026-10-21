#include "FSTT_ActivateAbilityByTag.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "GameCore/AI/MonsterAIController.h"
#include "GameCore/AI/MonsterCombatDirectorSubsystem.h"
#include "StateTreeExecutionContext.h"

FSTT_ActivateAbilityByTag::FSTT_ActivateAbilityByTag()
{
	bShouldCallTick = true;
	bShouldCopyBoundPropertiesOnTick = false;
	bShouldCopyBoundPropertiesOnExitState = false;
}

EStateTreeRunStatus FSTT_ActivateAbilityByTag::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	AMonsterAIController* MonsterController = Cast<AMonsterAIController>(Context.GetOwner());
	if (!IsValid(MonsterController))
	{
		UE_LOG(LogTemp, Warning, TEXT("[ActivateAbilityByTag] Failed | Reason=ControllerInvalid"));
		return EStateTreeRunStatus::Failed;
	}

	InstanceData.ActiveSpecHandle = FGameplayAbilitySpecHandle();
	InstanceData.bLoggedFirstTick = false;
	InstanceData.bHoldingAttackToken = false;

	/*
	 * 어택 토큰을 못 얻으면 어빌리티를 시작하지 않고 실패로 빠진다.
	 * StateTree는 그 사이 접근이나 견제 같은 다른 행동을 고르게 된다.
	 */
	if (InstanceData.bRequiresAttackToken)
	{
		UWorld* World = MonsterController->GetWorld();
		UMonsterCombatDirectorSubsystem* Director =
			World ? World->GetSubsystem<UMonsterCombatDirectorSubsystem>() : nullptr;

		if (Director != nullptr)
		{
			if (!Director->TryAcquireAttackToken(MonsterController->GetPawn()))
			{
				UE_LOG(LogTemp, Verbose,
					TEXT("[ActivateAbilityByTag] Blocked | Controller=%s | Reason=NoAttackToken"),
					*GetNameSafe(MonsterController));
				return EStateTreeRunStatus::Failed;
			}

			InstanceData.bHoldingAttackToken = true;
		}
	}
	const FGameplayTag AbilityTag = MonsterController->GetSelectedAction();
	UE_LOG(LogTemp, Warning, TEXT("[ActivateAbilityByTag] Enter | Controller=%s | Pawn=%s | SelectedAction=%s"),
		*GetNameSafe(MonsterController), *GetNameSafe(MonsterController->GetPawn()), *AbilityTag.ToString());

	if (!AbilityTag.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("[ActivateAbilityByTag] Failed | Reason=SelectedActionInvalid"));
		return EStateTreeRunStatus::Failed;
	}

	const IAbilitySystemInterface* AbilitySystemInterface = Cast<IAbilitySystemInterface>(MonsterController->GetPawn());
	InstanceData.AbilitySystemComponent = AbilitySystemInterface ? AbilitySystemInterface->GetAbilitySystemComponent() : nullptr;
	if (!IsValid(InstanceData.AbilitySystemComponent.Get()))
	{
		UE_LOG(LogTemp, Warning, TEXT("[ActivateAbilityByTag] Failed | Controller=%s | Reason=ASCInvalid"), *GetNameSafe(MonsterController));
		return EStateTreeRunStatus::Failed;
	}

	if (!InstanceData.AbilitySystemComponent->TryActivateAbilitiesByTag(FGameplayTagContainer(AbilityTag)))
	{
		UE_LOG(LogTemp, Warning, TEXT("[ActivateAbilityByTag] Failed | Controller=%s | Action=%s | Reason=TryActivateFailed"),
			*GetNameSafe(MonsterController), *AbilityTag.ToString());
		return EStateTreeRunStatus::Failed;
	}

	UE_LOG(LogTemp, Warning, TEXT("[ActivateAbilityByTag] TryActivateAbilitiesByTag succeeded: %s"), *AbilityTag.ToString());
	for (const FGameplayAbilitySpec& Spec : InstanceData.AbilitySystemComponent->GetActivatableAbilities())
	{
		if (Spec.IsActive() && IsValid(Spec.Ability) && Spec.Ability->GetAssetTags().HasTag(AbilityTag))
		{
			InstanceData.ActiveSpecHandle = Spec.Handle;
			UE_LOG(LogTemp, Warning,
				TEXT("[ActivateAbilityByTag] BoundSpec | Controller=%s | Action=%s | Handle=%s | Ability=%s | Active=%s"),
				*GetNameSafe(MonsterController),
				*AbilityTag.ToString(),
				*Spec.Handle.ToString(),
				*GetNameSafe(Spec.Ability),
				Spec.IsActive() ? TEXT("true") : TEXT("false"));
			break;
		}
	}

	if (!InstanceData.ActiveSpecHandle.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("[ActivateAbilityByTag] SucceededImmediately | Controller=%s | Pawn=%s | Action=%s | Reason=NoActiveSpec"),
			*GetNameSafe(MonsterController), *GetNameSafe(MonsterController->GetPawn()), *AbilityTag.ToString());
		return EStateTreeRunStatus::Succeeded;
	}

	MonsterController->SetInterruptible(false);
	UE_LOG(LogTemp, Warning, TEXT("[ActivateAbilityByTag] Running | Controller=%s | Pawn=%s | Action=%s | SpecHandle=%s"),
		*GetNameSafe(MonsterController), *GetNameSafe(MonsterController->GetPawn()), *AbilityTag.ToString(), *InstanceData.ActiveSpecHandle.ToString());
	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FSTT_ActivateAbilityByTag::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	AMonsterAIController* MonsterController = Cast<AMonsterAIController>(Context.GetOwner());
	if (!IsValid(InstanceData.AbilitySystemComponent.Get()))
	{
		if (IsValid(MonsterController))
		{
			MonsterController->SetInterruptible(true);
			UE_LOG(LogTemp, Warning, TEXT("[ActivateAbilityByTag] Failed | Controller=%s | Pawn=%s | Reason=ASCInvalid"),
				*GetNameSafe(MonsterController), *GetNameSafe(MonsterController->GetPawn()));
		}
		return EStateTreeRunStatus::Failed;
	}

	const FGameplayAbilitySpec* Spec = InstanceData.AbilitySystemComponent->FindAbilitySpecFromHandle(InstanceData.ActiveSpecHandle);
	if (!InstanceData.bLoggedFirstTick)
	{
		InstanceData.bLoggedFirstTick = true;
		UE_LOG(LogTemp, Warning,
			TEXT("[ActivateAbilityByTag] FirstTick | Controller=%s | Pawn=%s | Handle=%s | SpecFound=%s | Active=%s | Ability=%s"),
			*GetNameSafe(MonsterController),
			*GetNameSafe(MonsterController ? MonsterController->GetPawn() : nullptr),
			*InstanceData.ActiveSpecHandle.ToString(),
			Spec ? TEXT("true") : TEXT("false"),
			Spec && Spec->IsActive() ? TEXT("true") : TEXT("false"),
			Spec ? *GetNameSafe(Spec->Ability) : TEXT("None"));
	}
	if (!Spec || !Spec->IsActive())
	{
		if (IsValid(MonsterController))
		{
			MonsterController->SetInterruptible(true);
			UE_LOG(LogTemp, Warning, TEXT("[ActivateAbilityByTag] Succeeded | Controller=%s | Pawn=%s | SpecHandle=%s | SpecFound=%s"),
				*GetNameSafe(MonsterController), *GetNameSafe(MonsterController->GetPawn()), *InstanceData.ActiveSpecHandle.ToString(), Spec ? TEXT("true") : TEXT("false"));
		}
		return EStateTreeRunStatus::Succeeded;
	}

	return EStateTreeRunStatus::Running;
}

void FSTT_ActivateAbilityByTag::ExitState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	AMonsterAIController* MonsterController = Cast<AMonsterAIController>(Context.GetOwner());

	// 성공이든 실패든 이 상태를 벗어나면 토큰은 반드시 돌려준다.
	if (InstanceData.bHoldingAttackToken && IsValid(MonsterController))
	{
		if (UWorld* World = MonsterController->GetWorld())
		{
			if (UMonsterCombatDirectorSubsystem* Director =
				World->GetSubsystem<UMonsterCombatDirectorSubsystem>())
			{
				Director->ReleaseAttackToken(MonsterController->GetPawn());
			}
		}
	}
	InstanceData.bHoldingAttackToken = false;

	const FGameplayAbilitySpec* Spec =
		IsValid(InstanceData.AbilitySystemComponent.Get())
			? InstanceData.AbilitySystemComponent->FindAbilitySpecFromHandle(InstanceData.ActiveSpecHandle)
			: nullptr;

	UE_LOG(LogTemp, Warning,
		TEXT("[ActivateAbilityByTag] ExitState | Controller=%s | Pawn=%s | Handle=%s | SpecFound=%s | Active=%s | Ability=%s"),
		*GetNameSafe(MonsterController),
		*GetNameSafe(MonsterController ? MonsterController->GetPawn() : nullptr),
		*InstanceData.ActiveSpecHandle.ToString(),
		Spec ? TEXT("true") : TEXT("false"),
		Spec && Spec->IsActive() ? TEXT("true") : TEXT("false"),
		Spec ? *GetNameSafe(Spec->Ability) : TEXT("None"));
}
