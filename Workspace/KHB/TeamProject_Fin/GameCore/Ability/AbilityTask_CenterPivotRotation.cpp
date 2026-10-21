#include "AbilityTask_CenterPivotRotation.h"

#include "GameFramework/Character.h"

UAbilityTask_CenterPivotRotation::UAbilityTask_CenterPivotRotation()
{
	bTickingTask = true;
}

UAbilityTask_CenterPivotRotation*
UAbilityTask_CenterPivotRotation::StartCenterPivotRotation(
	UGameplayAbility* OwningAbility,
	ACharacter* InTargetCharacter)
{
	UAbilityTask_CenterPivotRotation* Task =
		NewAbilityTask<UAbilityTask_CenterPivotRotation>(OwningAbility);
	Task->TargetCharacter = InTargetCharacter;

	if (IsValid(InTargetCharacter))
	{
		FVector PivotExtent;
		InTargetCharacter->GetActorBounds(true, Task->PivotLocation, PivotExtent);
		Task->LocalActorOffset =
			InTargetCharacter->GetActorQuat().UnrotateVector(
				InTargetCharacter->GetActorLocation() - Task->PivotLocation);
	}

	return Task;
}

void UAbilityTask_CenterPivotRotation::Activate()
{
	if (!IsValid(TargetCharacter))
	{
		EndTask();
		return;
	}

	Super::Activate();
}

void UAbilityTask_CenterPivotRotation::TickTask(float DeltaTime)
{
	Super::TickTask(DeltaTime);

	if (!IsValid(TargetCharacter))
	{
		EndTask();
		return;
	}

	const FVector CorrectedLocation =
		PivotLocation
		+ TargetCharacter->GetActorQuat().RotateVector(LocalActorOffset);

	TargetCharacter->SetActorLocation(
		CorrectedLocation,
		false,
		nullptr,
		ETeleportType::TeleportPhysics);
}

void UAbilityTask_CenterPivotRotation::OnDestroy(bool AbilityEnded)
{
	TargetCharacter = nullptr;
	Super::OnDestroy(AbilityEnded);
}
