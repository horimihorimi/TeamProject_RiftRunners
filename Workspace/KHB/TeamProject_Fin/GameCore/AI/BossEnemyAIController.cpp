#include "BossEnemyAIController.h"

#include "Components/StateTreeAIComponent.h"

ABossEnemyAIController::ABossEnemyAIController()
{
	// 보스는 패턴 준비 이동과 공전이 이동을 전부 맡는다. 컨트롤러 추격이 겹치면 이동이 서로 덮어쓴다.
	bEnablePursuitMovement = false;

	/*
	 * ST_Enemy_Boss에는 휴면 조건이 없어 시작하자마자 CombatLoop에 들어간다.
	 * 휴면(투명) 중에 패턴이 나가지 않도록 조우가 끝날 때까지 StateTree를 시작하지 않는다.
	 */
	StateTreeAIComponent->SetStartLogicAutomatically(false);

	// 보스전은 한 방 안의 싸움이라 "못 봐서 타깃이 없는" 상태가 없어야 한다. 누구를 칠지는 셀렉터가 고른다
	SetTrackAllPlayers(true);
}

void ABossEnemyAIController::StartBattle()
{
	if (bBattleStarted || !IsValid(StateTreeAIComponent))
	{
		return;
	}

	bBattleStarted = true;
	StateTreeAIComponent->StartLogic();
}
