#include "GameCore/Character/BossCharacter.h"

#include "GameCore/Tags/GameplayTagSystem.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"

ABossCharacter::ABossCharacter()
{
	PrimaryActorTick.bCanEverTick = false;
	CharacterRoleTag = GameplayTagSystem::Character_Boss;
}

AActor* ABossCharacter::GetArenaCenter() const
{
	if (!IsValid(ArenaCenter))
	{
		const_cast<ABossCharacter*>(this)->ResolveArenaCenter();
	}

	return ArenaCenter;
}

void ABossCharacter::ResolveArenaCenter()
{
	if (IsValid(ArenaCenter) || ArenaCenterTag.IsNone())
	{
		return;
	}

	TArray<AActor*> FoundActors;
	UGameplayStatics::GetAllActorsWithTag(this, ArenaCenterTag, FoundActors);

	if (FoundActors.Num() > 0)
	{
		ArenaCenter = FoundActors[0];
	}
	else
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[Boss] 아레나 중심을 찾지 못했습니다. 태그 %s 를 확인하세요."),
			*ArenaCenterTag.ToString());
	}
}

void ABossCharacter::BeginPlay()
{
	ResolveArenaCenter();
	Super::BeginPlay();

	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->SetMovementMode(MOVE_Flying);
		Movement->GravityScale = 0.0f;
		Movement->BrakingDecelerationFlying = 4096.0f;
	}
}
