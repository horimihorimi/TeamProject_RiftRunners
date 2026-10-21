#pragma once

#include "CoreMinimal.h"
#include "GameCore/Character/CommonEnemyCharacter.h"
#include "FlyingEnemyCharacter.generated.h"

/**
 * Enemy Framework의 비행형 일반 적입니다.
 *
 * 이동 컴포넌트를 비행형으로 지정하는 일만 한다. 전투 구성은 부모가 Definition DA로 조립한다.
 */
UCLASS(Blueprintable)
class TEAMPROJECT_FIN_API AFlyingEnemyCharacter : public ACommonEnemyCharacter
{
	GENERATED_BODY()

public:
	AFlyingEnemyCharacter(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** 순찰 지점은 바닥에 찍히므로 비행 적은 이동 프로필의 비행 높이만큼 위를 난다. */
	virtual float GetPatrolHeightOffset() const override;

protected:
	virtual void BeginPlay() override;
};
