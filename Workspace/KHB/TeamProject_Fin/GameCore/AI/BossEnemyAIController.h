#pragma once

#include "CoreMinimal.h"
#include "GameCore/AI/CommonEnemyAIController.h"
#include "BossEnemyAIController.generated.h"

/**
 * 새 보스 Framework용 Controller입니다.
 *
 * StateTreeAIComponent에는 BP 하위 클래스에서 ST_Enemy_Boss를 지정합니다.
 * 보스의 이동은 전부 StateTree가 이동 컴포넌트에 요청하므로 컨트롤러 추격을 끕니다.
 * StateTree는 조우 연출이 끝나 StartBattle이 불릴 때 시작합니다.
 * 이 클래스는 보스 전용 이동이나 패턴을 소유하지 않습니다.
 */
UCLASS(Blueprintable)
class TEAMPROJECT_FIN_API ABossEnemyAIController : public ACommonEnemyAIController
{
	GENERATED_BODY()

public:
	ABossEnemyAIController();

	/** 조우 연출이 끝나면 보스 캐릭터가 부른다. 그 전까지 StateTree는 돌지 않는다. */
	void StartBattle();

	bool HasBattleStarted() const { return bBattleStarted; }

private:
	bool bBattleStarted = false;
};
