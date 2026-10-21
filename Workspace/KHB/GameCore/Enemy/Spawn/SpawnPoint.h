#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameCore/Enemy/Spawn/ActivationViewer.h"
#include "GameCore/Enemy/Spawn/EnemyType.h"
#include "PCGMap/MapBuild/SectorBuild/Event/SectorEvent.h"

#include "SpawnPoint.generated.h"

class AMonsterCharacter;
class USpawnAppearance;

/*
 * 어디에, 무엇을 스폰할지. 섹터 조각 BP에 ChildActor로 붙이거나 레벨에 직접 놓는다.
 * 몹 하나를 풀에서 빌려 오고, 죽거나 섹터가 사라지거나 플레이어가 멀어지면 이 포인트가 돌려준다. 빌린 쪽이 반납한다.
 * 어떻게 등장시킬지는 Appearance에 맡긴다.
 */
UCLASS()
class TEAMPROJECT_FIN_API ASpawnPoint : public AActor, public ISectorEvent
{
	GENERATED_BODY()


public:
	ASpawnPoint();

	// ISectorEvent. 서버에서 몹이 없을 때만 꺼낸다. WorldContext가 플레이어 폰이면 등장 대상의 힌트로 쓴다
	virtual void Execute(const UObject* WorldContext = nullptr) override;

	EEnemyType GetEnemyType() const
	{
		return EnemyType;
	}

	FName GetSpawnGroupId() const
	{
		return GroupId;
	}

	// 몹을 꺼내 쥐고 있는가(죽었어도 반납 전이면 참). 풀이 준비 수량을 셀 때 뺀다
	bool HasSpawned() const
	{
		return bSpawned || !MultiSpawns.IsEmpty();
	}

	// 지금 이 포인트에서 새 몹을 꺼낼 수 있는가. 여러 마리 모드는 쥐고 있어도 더 꺼낸다
	bool CanSpawn() const
	{
		return bAllowMultipleSpawns || !bSpawned;
	}

	// 꺼낸 몹 중 하나라도 살아 있는가
	bool HasAliveMonster() const;

	// 여러 마리 모드에서 아직 살아 있는 몹 수. 주기 소환 이벤트가 전체 상한을 셀 때 쓴다
	int32 GetAliveMultiSpawnCount() const;

	// 쥐고 있는 살아 있는 몹. 여러 마리 모드면 그중 하나를 돌려준다
	AMonsterCharacter* GetSpawnedMonster() const;

	/*
	 * 쥐고 있는 살아 있는 몹을 전부 보상 없이 죽인다. 서버 전용.
	 * 죽은 몹은 평소처럼 시체 시간 뒤 풀로 돌아온다. 보스 사망 때 남은 애드를 정리하는 데 쓴다
	 */
	void KillAllMonsters();

	bool TracksMonster(AMonsterCharacter* Monster) const;

	void ReplaceTrackedMonster(AMonsterCharacter* Parent, AMonsterCharacter* Left, AMonsterCharacter* Right);

	/*
	 * 켜면 살아 있는 몹이 있어도 Execute 마다 풀에서 한 마리씩 더 꺼낸다. 죽은 몹은 하나씩 풀로 돌려준다.
	 * 주기 소환 이벤트(보스 애드 등)가 켠다. 레벨에 놓을 때 직접 켤 수도 있다
	 */
	void SetAllowMultipleSpawns(bool bEnable)
	{
		bAllowMultipleSpawns = bEnable;
	}

	// 숨었다 튀어나오는 방식인가. 전투 섹터가 동시 전투 시작 대상을 고를 때 쓴다
	bool UsesEmerge() const;

	/*
	 * 이 포인트에서 꺼낸 몹이 시야와 무관하게 전 플레이어를 쫓게 한다. 보스 애드 이벤트가 켠다.
	 * 꺼낼 때마다 컨트롤러에 넘기므로, 같은 몹이 풀을 거쳐 일반 포인트에서 나오면 다시 시야 기반이 된다.
	 */
	void SetTrackAllPlayers(bool bEnable, float ScoringRange = 0.0f);

	/*
	 * 거리 기반 활성화. UEnemyActivationSubsystem이 주기마다 플레이어 위치와 인보커 반경에서 뽑은 거리를 넘긴다.
	 *   순찰(자동): 가까우면 꺼내고, 전투 중이 아닐 때 멀어지면 풀로 돌려보낸다. 돌려보낸 뒤 다시 가까워지면 새로 꺼낸다
	 *   그 외(Emerge 등): 멀어지면 재우고, 가까워지면 깨운다. 전투 섹터의 전멸 판정이 걸려 있어 돌려보내지 않는다
	 */
	void UpdateActivation(const TArray<FActivationViewer>& Viewers, float Interval);

protected:
	virtual void BeginPlay() override;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(EditAnywhere, Category = "Spawn")
	EEnemyType EnemyType = EEnemyType::Melee;

	/*
	 * 켜면 EnemyType 대신 RandomEnemyTypes 중 하나를 골라 낸다.
	 * 보스 애드처럼 같은 위치에서 여러 종류를 내보낼 때만 켠다.
	 */
	UPROPERTY(EditAnywhere, Category = "Spawn|Random")
	bool bUseRandomEnemyType = false;

	UPROPERTY(EditAnywhere, Category = "Spawn|Random",
	          meta = (EditCondition = "bUseRandomEnemyType", EditConditionHides))
	TArray<EEnemyType> RandomEnemyTypes;

	/*
	 * 죽은 몹을 풀에 돌려준 뒤 포인트도 비운다.
	 * 일반 전투 섹터는 한 번 처치하면 끝나야 하므로 기본값은 끈다.
	 */
	UPROPERTY(EditAnywhere, Category = "Spawn")
	bool bReusableAfterDeath = false;

	/*
	 * 한 포인트가 여러 마리를 쥔다. 살아 있는 몹이 있어도 Execute 마다 새로 꺼낸다.
	 * 한 번 처치로 끝나는 일반 전투 섹터는 끈 채로 둔다. 주기 소환 이벤트가 런타임에 켜기도 한다
	 */
	UPROPERTY(EditAnywhere, Category = "Spawn")
	bool bAllowMultipleSpawns = false;

	// 전투 섹터가 이 ID로 찾는다. 같은 섹터 조각(레벨 배치면 같은 레벨) 안에서만 찾는다
	UPROPERTY(EditAnywhere, Category = "Spawn")
	FName GroupId = NAME_None;

	// 등장 방식. 드롭다운에서 Emerge / Patrol을 고르면 그 방식의 설정만 보인다
	UPROPERTY(EditAnywhere, Instanced, Category = "Spawn")
	TObjectPtr<USpawnAppearance> Appearance;

private:
	UFUNCTION()
	void HandleMonsterDeath(AMonsterCharacter* DeadMonster);

	// 풀 준비가 끝나야 자동 활성화를 시작한다. 그 전에 꺼내면 준비 단계가 수량을 잘못 센다
	void HandlePrewarmCompleted();

	// 이번 Execute에 사용할 타입. 랜덤 후보가 비었으면 기존 EnemyType으로 안전하게 돌아간다
	EEnemyType ResolveSpawnEnemyType() const;

	// 쥐고 있는 몹을 풀로 돌려준다
	void ReleaseMonster();

	// 플레이어가 멀어져서 돌려준다. 죽은 게 아니라서 다시 꺼낼 수 있다
	void ReleaseForDistance();

	// 풀에서 한 마리 꺼내 추적 설정까지 마친다. 등장은 호출한 쪽이 Appearance로 시킨다
	AMonsterCharacter* AcquireMonster();

	// 여러 마리 모드의 거리 활성화. 몹마다 기존 규칙(순찰은 반납, 그 외는 재우기)을 따로 적용한다
	void UpdateMultiActivation(const TArray<FActivationViewer>& Viewers, float Interval);

	// 여러 마리 모드에서 몹 하나를 풀로 돌려준다
	void ReleaseMultiMonster(AMonsterCharacter* Monster);

	// 여러 마리 모드가 쥔 몹 하나. 거리 유예는 몹마다 따로 센다
	struct FMultiSpawnEntry
	{
		TWeakObjectPtr<AMonsterCharacter> Monster;
		float FarTime = 0.0f;

		// 죽어서 시체 시간 뒤 반납을 기다리는 중
		bool bDead = false;
	};

	// 여러 마리 모드가 쥔 몹. 반납될 때 빠진다. 섹터가 사라지면 남은 몹을 전부 돌려준다
	TArray<FMultiSpawnEntry> MultiSpawns;

	TWeakObjectPtr<AMonsterCharacter> SpawnedMonster;

	bool bSpawned = false;

	bool bMonsterDied = false;

	// SetTrackAllPlayers로 정한다. 꺼낼 때 컨트롤러에 넘긴다
	bool bTrackAllPlayers = false;

	float TrackScoringRange = 0.0f;

	// 자동 활성화 포인트가 풀 준비를 마쳤는가
	bool bActivationReady = false;

	// 모든 플레이어가 멀리 있던 시간. 유예 시간을 넘기면 돌려보내거나 재운다
	float FarTime = 0.0f;

	FTimerHandle ReleaseTimer;

	FDelegateHandle PrewarmHandle;
};
