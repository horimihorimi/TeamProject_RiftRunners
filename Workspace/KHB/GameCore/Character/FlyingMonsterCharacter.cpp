#include "FlyingMonsterCharacter.h"

#include "GameCore/Movement/FlyingMonsterMovementComponent.h"

AFlyingMonsterCharacter::AFlyingMonsterCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UFlyingMonsterMovementComponent>(ACharacter::CharacterMovementComponentName))
{
}

void AFlyingMonsterCharacter::BeginPlay()
{
	Super::BeginPlay();
	GetCharacterMovement()->SetMovementMode(MOVE_Flying);
}
