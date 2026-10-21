#pragma once

#include "CoreMinimal.h"

/*
 * 거리 판정에 쓰는 플레이어 한 명. 네비메시는 플레이어마다 붙은 인보커가 만든 타일의 합집합이라,
 * 거리도 그 플레이어의 인보커 반경으로 각자 잰다. UEnemyActivationSubsystem이 주기마다 채워 스폰 포인트에 넘긴다
 */
struct FActivationViewer
{
	FVector Location = FVector::ZeroVector;

	// 이 플레이어가 이 거리 안에 들어오면 순찰 몹을 꺼낸다. 인보커 생성 반경 안쪽
	float ActivationDistance = 0.0f;

	// 몹이 이 플레이어에게서 이 거리 안이면 유지한다. 모든 플레이어에게서 벗어나야 처리한다. 인보커 제거 반경 안쪽
	float DeactivationDistance = 0.0f;
};
