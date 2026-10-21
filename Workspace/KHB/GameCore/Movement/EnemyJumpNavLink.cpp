#include "EnemyJumpNavLink.h"

#include "GroundMonsterMovementComponent.h"
#include "GameFramework/Character.h"
#include "AIController.h"

AEnemyJumpNavLink::AEnemyJumpNavLink()
{
	bSmartLinkIsRelevant = true;
	PointLinks.Empty();
	OnSmartLinkReached.AddDynamic(this, &ThisClass::HandleLinkReached);
}

void AEnemyJumpNavLink::HandleLinkReached(AActor* Agent, const FVector& Destination)
{
	ACharacter* Character = Cast<ACharacter>(Agent);
	UGroundMonsterMovementComponent* Movement =
		Character ? Cast<UGroundMonsterMovementComponent>(Character->GetCharacterMovement()) : nullptr;
	if (Movement && Movement->TryStartNavLinkJump(this, Destination))
	{
		return;
	}
	// 지원하지 않는 이동이나 잘못된 설정으로 링크에서 영구 대기하지 않도록 종료합니다.
	ResumePathFollowing(Agent);
	if (Character)
	{
		if (AAIController* Controller = Cast<AAIController>(Character->GetController()))
		{
			Controller->StopMovement();
		}
	}
}
