#include "AN_StopPatternMovement.h"

#include "Components/SkeletalMeshComponent.h"
#include "GameCore/Movement/MonsterMovementComponent.h"
#include "GameFramework/Actor.h"

FString UAN_StopPatternMovement::GetNotifyName_Implementation() const
{
	return TEXT("Stop Pattern Movement");
}

void UAN_StopPatternMovement::Notify(
	USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	AActor* Owner = IsValid(MeshComp) ? MeshComp->GetOwner() : nullptr;
	if (!IsValid(Owner) || !Owner->HasAuthority()) return;

	if (UMonsterMovementComponent* Movement = Owner->FindComponentByClass<UMonsterMovementComponent>())
	{
		Movement->StopPatternMovement();
	}
}
