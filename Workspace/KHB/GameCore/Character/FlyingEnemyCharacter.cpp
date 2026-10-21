#include "FlyingEnemyCharacter.h"

#include "GameCore/Enemy/Data/EnemyDataAssets.h"
#include "GameCore/Movement/FlyingMonsterMovementComponent.h"

AFlyingEnemyCharacter::AFlyingEnemyCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UFlyingMonsterMovementComponent>(ACharacter::CharacterMovementComponentName))
{
}

float AFlyingEnemyCharacter::GetPatrolHeightOffset() const
{
	const UEnemyMovementProfileDataAsset* Profile = GetActiveMovementProfile();
	return IsValid(Profile) ? Profile->Flying.DefaultHeightFromFloor : 0.0f;
}

void AFlyingEnemyCharacter::BeginPlay()
{
	Super::BeginPlay();
	GetCharacterMovement()->SetMovementMode(MOVE_Flying);
}
