// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BaseCharacter.h"
#include "Test_EnemyCharacter.generated.h"

/**
 * 적(몬스터/보스) 테스트용 캐릭터 클래스
 */
UCLASS()
class TEAMPROJECT_FIN_API ATest_EnemyCharacter : public ABaseCharacter
{
	GENERATED_BODY()

public:
	ATest_EnemyCharacter();

protected:
	virtual void BeginPlay() override;
};
