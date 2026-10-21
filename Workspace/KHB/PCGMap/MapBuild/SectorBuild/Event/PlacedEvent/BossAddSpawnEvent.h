#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "../SectorEvent.h"
#include "BossAddSpawnEvent.generated.h"

class ABaseCharacter;
class ABossMonsterCharacter;
class ASpawnPoint;
class UBossPhaseComponent;

/* 페이즈 하나에서 사용할 보스 애드 스폰 규칙. */
USTRUCT(BlueprintType)
struct FBossAddPhaseSpawnRule
{
	GENERATED_BODY()

	// 0 이하면 이 페이즈에서는 애드를 소환하지 않는다.
	UPROPERTY(EditAnywhere, Category = "Boss Add", meta = (ClampMin = "0.0", Units = "s"))
	float SpawnInterval = 12.0f;

	/*
	 * [0] = 1인, [1] = 2인, [2] = 3인 순서로, 한 번 소환할 때 쓰는 스폰 포인트 수.
	 * 살아 있는 애드와 상관없이 주기마다 이만큼 소환한다. 배열보다 많은 인원이면 마지막 값을 사용한다.
	 * (이름은 기존 에셋 값 보존을 위해 유지한다)
	 */
	UPROPERTY(EditAnywhere, Category = "Boss Add", meta = (ClampMin = "0"))
	TArray<int32> MaxActiveSpawnPointsByPlayerCount;
};

/*
 * 보스 조우 중 기존 SpawnPoint 그룹을 주기적으로 보충한다.
 * BP 자식으로 만들어 보스, 그룹 ID, 페이즈별 규칙만 맵마다 설정한다.
 */
UCLASS(Blueprintable)
class TEAMPROJECT_FIN_API ABossAddSpawnEvent : public AScenePlacedEvent
{
	GENERATED_BODY()

public:
	virtual void Execute(const UObject* WorldContext = nullptr) override;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// 애드가 붙을 레벨 배치 보스. ABossMonsterCharacter 여야 한다.
	UPROPERTY(EditInstanceOnly, Category = "Boss Add")
	TObjectPtr<ABossMonsterCharacter> Boss = nullptr;

	// 기존 SpawnPoint의 GroupId. 예: BossAdds
	UPROPERTY(EditAnywhere, Category = "Boss Add")
	FName SpawnPointGroupId = NAME_None;

	// [0]은 보스 1페이즈, [1]은 2페이즈에 대응한다.
	UPROPERTY(EditAnywhere, Category = "Boss Add")
	TArray<FBossAddPhaseSpawnRule> PhaseRules;

	/*
	 * 애드는 시야와 무관하게 전 플레이어를 후보로 두고 끝까지 쫓는다.
	 * 이 거리까지 가까울수록 높은 점수를 줘서 넓은 보스맵에서도 가까운 플레이어를 우선한다.
	 */
	UPROPERTY(EditAnywhere, Category = "Boss Add|Targeting", meta = (ClampMin = "1.0", Units = "cm"))
	float TrackScoringRange = 20000.0f;

	/*
	 * 필드에 동시에 살아 있을 수 있는 애드 전체 상한. 소환이 쌓이기만 하므로 이 수를 넘기면 넘는 만큼 덜 소환한다.
	 * 0이면 제한하지 않는다.
	 */
	UPROPERTY(EditAnywhere, Category = "Boss Add", meta = (ClampMin = "0"))
	int32 MaxAliveAdds = 12;

private:
	UFUNCTION()
	void HandlePhaseTransitionRequested(int32 NextPhaseIndex, FGameplayTag NextPhaseId);

	UFUNCTION()
	void HandlePhaseChanged(int32 CurrentPhaseIndex, FGameplayTag CurrentPhaseId);

	void HandleBossDeathStarted(ABaseCharacter* DeadCharacter);
	void CacheSpawnPoints();
	void StartTimerForCurrentPhase();
	void SpawnToCurrentTarget();
	void StopSpawning();

	const FBossAddPhaseSpawnRule* GetCurrentPhaseRule() const;
	int32 GetParticipatingPlayerCount() const;
	int32 GetSpawnCountPerWave(const FBossAddPhaseSpawnRule& Rule, int32 PlayerCount) const;

	TArray<TWeakObjectPtr<ASpawnPoint>> BossAddSpawnPoints;
	TWeakObjectPtr<UBossPhaseComponent> PhaseComponent;
	FTimerHandle SpawnTimer;
	FDelegateHandle BossDeathStartedHandle;
	bool bSpawning = false;
};
