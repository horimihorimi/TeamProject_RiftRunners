#include "GroundEnemyCharacter.h"

#include "GameCore/Movement/GroundMonsterMovementComponent.h"

AGroundEnemyCharacter::AGroundEnemyCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UGroundMonsterMovementComponent>(ACharacter::CharacterMovementComponentName))
{
}

void AGroundEnemyCharacter::BeginPlay()
{
	Super::BeginPlay();
	GetCharacterMovement()->SetMovementMode(MOVE_Walking);
}
