#include "GroundMonsterCharacter.h"

#include "GameCore/Movement/GroundMonsterMovementComponent.h"

AGroundMonsterCharacter::AGroundMonsterCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UGroundMonsterMovementComponent>(ACharacter::CharacterMovementComponentName))
{
}

void AGroundMonsterCharacter::BeginPlay()
{
	Super::BeginPlay();
	GetCharacterMovement()->SetMovementMode(MOVE_Walking);
}
