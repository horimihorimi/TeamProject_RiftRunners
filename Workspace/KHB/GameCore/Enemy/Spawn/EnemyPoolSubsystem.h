#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "GameCore/Enemy/Spawn/EnemyType.h"

#include "EnemyPoolSubsystem.generated.h"

class ASpawnPoint;
class ACommonEnemyCharacter;

DECLARE_MULTICAST_DELEGATE(FOnEnemyPoolPrewarmCompleted);

// 한 종류의 쉬고 있는 몹들. TMap 값으로 TArray를 UPROPERTY에 바로 못 넣어서 감쌌다
USTRUCT()
struct FEnemyPoolBucket
{
	GENERATED_BODY()


	UPROPERTY()
	TArray<TObjectPtr<AActor>> Idle;
};

/*
 * 몹 오브젝트 풀. 생성·보관·대여·반환만 맡는다. 어디에 어떻게 등장시킬지는 스폰 포인트가 정한다.
 *
 * 월드 서브시스템이라 월드가 바뀌면 풀도 같이 사라진다. 게임 인스턴스에 두면 죽은 월드의 액터를 쥔 채
 * 다음 월드로 넘어간다. 스폰·반환은 서버에서만 하고 클라는 복제로 받는다.
 */
UCLASS()
class TEAMPROJECT_FIN_API UEnemyPoolSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()


public:
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

	virtual void Deinitialize() override;

	// 스폰 포인트가 BeginPlay에서 자기 몫을 알린다. 다음 준비 단계에서 이만큼을 미리 만들어 둔다
	void RegisterDemand(ASpawnPoint* SpawnPoint);

	void UnregisterDemand(ASpawnPoint* SpawnPoint);

	// 쉬는 몹을 꺼내 자리로 옮긴다. 모자라면 그 자리에서 만들고 경고를 남긴다. 클라에서는 널
	AActor* Acquire(EEnemyType Type, const FTransform& SpawnTransform);

	// 파괴하지 않고 숨겨서 되돌린다. 이 풀이 내준 액터가 아니면 무시한다
	void Release(AActor* Actor);

	bool IsActive(AActor* Actor) const
	{
		return ActiveTypes.Contains(Actor);
	}

	// 부모와 동일한 클래스를 획득하고 무작위 특성은 적용하지 않습니다. 배치 좌표는 캡슐 중심입니다.
	ACommonEnemyCharacter* AcquireForSplit(ACommonEnemyCharacter* Parent, const FTransform& SpawnTransform);

	// 준비 단계가 끝났다. 자동 실행 스폰 포인트가 이걸 받고 등장한다
	FOnEnemyPoolPrewarmCompleted OnPrewarmCompleted;

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	// 섹터 조립이 끝났을 때 USectorSubsystem이 부른다
	void HandleSectorReady();

	// 아직 몹을 꺼내지 않은 포인트 수만큼, 쉬는 몹이 모자란 종류만 새로 만든다
	void Prewarm();

	// 새로 만들어 쉬는 상태로 둔다. 영구 레벨 소속이라 스트리밍 레벨이 내려가도 같이 지워지지 않는다
	AActor* CreatePooled(EEnemyType Type, UClass* ExactClass = nullptr, bool bSkipRandomTrait = false);

	bool IsServer() const;

	UPROPERTY()
	TMap<EEnemyType, FEnemyPoolBucket> IdlePool;

	// 빌려 준 몹과 그 종류. 돌려받을 때 어느 칸으로 넣을지 여기서 찾는다
	UPROPERTY()
	TMap<TObjectPtr<AActor>, EEnemyType> ActiveTypes;

	TArray<TWeakObjectPtr<ASpawnPoint>> Demands;

	FDelegateHandle SectorReadyHandle;
};
