#include "AN_SendGameplayEvent.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Actor.h"

UAN_SendGameplayEvent::UAN_SendGameplayEvent()
{
	bIsNativeBranchingPoint = true;
}

void UAN_SendGameplayEvent::Notify(
	USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	if (!IsValid(MeshComp) || !EventTag.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("[DashSpline] Notify skipped: MeshComp or EventTag is invalid."));
		return;
	}

	AActor* OwnerActor = MeshComp->GetOwner();
	if (!IsValid(OwnerActor))
	{
		UE_LOG(LogTemp, Warning, TEXT("[DashSpline] Notify skipped: Mesh owner is invalid."));
		return;
	}

	FGameplayEventData EventData;
	EventData.EventTag = EventTag;
	EventData.Instigator = OwnerActor;
	EventData.Target = OwnerActor;

	UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(
		OwnerActor,
		EventTag,
		EventData);

	UE_LOG(LogTemp, Warning, TEXT("[DashSpline] Notify sent %s to %s."),
		*EventTag.ToString(), *GetNameSafe(OwnerActor));
}
