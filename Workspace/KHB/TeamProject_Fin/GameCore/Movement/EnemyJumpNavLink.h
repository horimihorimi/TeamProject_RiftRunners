#pragma once

#include "CoreMinimal.h"
#include "Navigation/NavLinkProxy.h"
#include "EnemyJumpNavLink.generated.h"

// 맵의 도약 연결점을 제공하며 실제 이동은 몬스터 이동 컴포넌트에 맡깁니다.
UCLASS()
class TEAMPROJECT_FIN_API AEnemyJumpNavLink : public ANavLinkProxy
{
	GENERATED_BODY()

  public:
	AEnemyJumpNavLink();

  private:
	UFUNCTION()
	void HandleLinkReached(AActor* Agent, const FVector& Destination);
};
