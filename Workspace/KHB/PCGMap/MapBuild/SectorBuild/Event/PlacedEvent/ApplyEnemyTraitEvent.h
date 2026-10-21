#pragma once

#include "CoreMinimal.h"
#include "../SectorEvent.h"
#include "GameCore/Enemy/Data/EnemyDataAssets.h"
#include "ApplyEnemyTraitEvent.generated.h"
class ACommonEnemyCharacter;
UCLASS(Blueprintable)
class TEAMPROJECT_FIN_API AApplyEnemyTraitEvent : public AScenePlacedEvent
{
	GENERATED_BODY()

public:
	UPROPERTY(EditInstanceOnly, Category = "Enemy Trait")
	TObjectPtr<ACommonEnemyCharacter> TargetEnemy;

	UPROPERTY(EditAnywhere, Category = "Enemy Trait")
	FEnemyTraitEffectDefinition Trait;

	UPROPERTY(EditInstanceOnly, Category = "Enemy Trait")
	bool bShowBeforeBattle = true;

	virtual void Execute(const UObject* WorldContext = nullptr) override;

private:
	bool bExecutionDeferred = false;
};
