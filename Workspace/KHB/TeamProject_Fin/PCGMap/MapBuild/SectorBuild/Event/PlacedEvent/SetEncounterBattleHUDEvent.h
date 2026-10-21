#pragma once

#include "CoreMinimal.h"
#include "../SectorEvent.h"
#include "SetEncounterBattleHUDEvent.generated.h"

class ACommonEnemyCharacter;

/** 서버의 씬 이벤트에서 각 클라이언트 GamePlayBase에 있는 보스 HUD를 제어합니다. */
UCLASS(Blueprintable)
class TEAMPROJECT_FIN_API ASetEncounterBattleHUDEvent : public AScenePlacedEvent
{
	GENERATED_BODY()


public:
	/** 켜기면 TargetEnemy의 이름과 체력을 바인딩하고, 끄기면 현재 HUD를 접습니다. */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Encounter HUD")
	bool bShow = true;

	/** bShow일 때 표시할 레벨 배치 보스입니다. */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Encounter HUD", meta = (EditCondition = "bShow"))
	TObjectPtr<ACommonEnemyCharacter> TargetEnemy = nullptr;

	virtual void Execute(const UObject* WorldContext = nullptr) override;
};
