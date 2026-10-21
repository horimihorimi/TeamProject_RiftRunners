#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "GameCore/Enemy/Spawn/ActivationViewer.h"

#include "EnemyActivationSubsystem.generated.h"

class ASpawnPoint;

/*
 * 거리 기반 활성화의 시계. 일정 주기(기본 0.5초)마다 플레이어 위치와 그 플레이어의 NavigationInvokerComponent 반경을
 * 모아 등록된 스폰 포인트에 넘긴다. 거리의 원본은 인보커 컴포넌트다.
 * 무엇을 할지(꺼내기·돌려보내기·재우기·깨우기)는 각 스폰 포인트가 정한다.
 * 매 프레임이 아니라 주기로만 돌고, 비교는 제곱 거리라 비용이 거의 없다. 서버 전용
 */
UCLASS()
class TEAMPROJECT_FIN_API UEnemyActivationSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;

	void Register(ASpawnPoint* SpawnPoint);
	void Unregister(ASpawnPoint* SpawnPoint);

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	void CheckActivation();

	TArray<TWeakObjectPtr<ASpawnPoint>> SpawnPoints;

	// 판정 중에 등록·해제가 일어나도 순회가 깨지지 않게 복사본을 돈다
	TArray<TWeakObjectPtr<ASpawnPoint>> CheckBuffer;
	TArray<FActivationViewer> Viewers;

	// 인보커가 없는 폰은 거리 판정에서 빠진다. 로그가 주기마다 쏟아지지 않게 한 번만 알린다
	bool bWarnedMissingInvoker = false;

	FTimerHandle CheckTimer;
	float CheckInterval = 0.5f;
};
