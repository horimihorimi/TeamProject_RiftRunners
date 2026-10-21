#pragma once

#include "CoreMinimal.h"
#include "GameCore/Character/CommonEnemyCharacter.h"
#include "GroundEnemyCharacter.generated.h"

/**
 * Enemy Framework의 지상형 일반 적입니다.
 *
 * 이동 컴포넌트를 지상형으로 지정하는 일만 한다. 전투 구성은 부모가 Definition DA로 조립한다.
 */
UCLASS(Blueprintable)
class TEAMPROJECT_FIN_API AGroundEnemyCharacter : public ACommonEnemyCharacter
{
	GENERATED_BODY()

public:
	AGroundEnemyCharacter(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
	virtual void BeginPlay() override;
};
