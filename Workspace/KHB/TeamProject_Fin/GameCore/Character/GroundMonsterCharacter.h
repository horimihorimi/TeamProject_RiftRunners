#pragma once

#include "CoreMinimal.h"
#include "MonsterCharacter.h"
#include "GroundMonsterCharacter.generated.h"

UCLASS()
class TEAMPROJECT_FIN_API AGroundMonsterCharacter : public AMonsterCharacter
{
	GENERATED_BODY()

public:
	AGroundMonsterCharacter(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
	virtual void BeginPlay() override;
};
