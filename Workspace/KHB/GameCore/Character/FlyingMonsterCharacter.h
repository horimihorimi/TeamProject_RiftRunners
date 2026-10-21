#pragma once

#include "CoreMinimal.h"
#include "MonsterCharacter.h"
#include "FlyingMonsterCharacter.generated.h"

UCLASS()
class TEAMPROJECT_FIN_API AFlyingMonsterCharacter : public AMonsterCharacter
{
	GENERATED_BODY()

public:
	AFlyingMonsterCharacter(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
	virtual void BeginPlay() override;
};
