#include "AnimNotifyState_NavLinkFlight.h"

#include "GroundMonsterMovementComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"

namespace
{
UGroundMonsterMovementComponent* FindGroundMovement(USkeletalMeshComponent* Mesh)
{
	ACharacter* Character = Mesh ? Cast<ACharacter>(Mesh->GetOwner()) : nullptr;
	return Character ? Cast<UGroundMonsterMovementComponent>(Character->GetCharacterMovement()) : nullptr;
}
} // namespace

void UAnimNotifyState_NavLinkFlight::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
												 float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	if (UGroundMonsterMovementComponent* Movement = FindGroundMovement(MeshComp))
	{
		Movement->BeginNavLinkFlight();
	}
}

void UAnimNotifyState_NavLinkFlight::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
											   const FAnimNotifyEventReference& EventReference)
{
	if (UGroundMonsterMovementComponent* Movement = FindGroundMovement(MeshComp))
	{
		Movement->EndNavLinkFlight();
	}
}
