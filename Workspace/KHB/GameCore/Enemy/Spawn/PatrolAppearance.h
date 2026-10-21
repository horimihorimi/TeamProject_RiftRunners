#pragma once

#include "CoreMinimal.h"
#include "GameCore/Enemy/Patrol/PatrolArea.h"
#include "GameCore/Enemy/Spawn/SpawnAppearance.h"

#include "PatrolAppearance.generated.h"

/*
 * 처음부터 보이는 채로 서 있다가 스폰 포인트 주변을 순찰하고, 눈에 띄는 플레이어와 싸운다.
 * 순찰 영역도 이 객체가 제공한다. 반경·최소 이동 거리·대기 시간은 순찰 방식에만 있는 값이라서다
 */
UCLASS(meta = (DisplayName = "Patrol"))
class TEAMPROJECT_FIN_API UPatrolAppearance : public USpawnAppearance, public IPatrolArea
{
	GENERATED_BODY()

public:
	virtual void Apply(AMonsterCharacter* Monster, AActor* InstigatorHint) override;
	virtual bool ShouldAutoExecute() const override { return bAutoExecute; }

	// IPatrolArea
	virtual FVector GetPatrolCenter() const override;
	virtual float GetPatrolRadius() const override { return Radius; }
	virtual float GetMinMoveDistance() const override { return FMath::Min(MinDistance, Radius); }
	virtual float GetRandomWaitTime() const override { return FMath::FRandRange(WaitMin, FMath::Max(WaitMin, WaitMax)); }

protected:
	/*
	 * 켜면 플레이어의 네비 인보커 생성 반경 안쪽(반경에서 Enemy Spawn의 Activation Margin을 뺀 거리)에 들어올 때 스스로 나오고,
	 * 멀어지면 풀로 돌아갔다가 다시 나온다.
	 * 끄면 전투 섹터나 이벤트가 Execute 할 때만 나온다. 문이 열리면 나오는 순찰병 같은 경우
	 */
	UPROPERTY(EditAnywhere, Category = "Patrol")
	bool bAutoExecute = true;

	UPROPERTY(EditAnywhere, Category = "Patrol", meta = (ClampMin = "0.0", Units = "cm"))
	float Radius = 800.0f;

	// 반경보다 크면 조건을 만족하는 지점이 없어서 반경으로 잘라 쓴다
	UPROPERTY(EditAnywhere, Category = "Patrol", meta = (ClampMin = "0.0", Units = "cm"))
	float MinDistance = 300.0f;

	UPROPERTY(EditAnywhere, Category = "Patrol", meta = (ClampMin = "0.0", Units = "s"))
	float WaitMin = 2.0f;

	UPROPERTY(EditAnywhere, Category = "Patrol", meta = (ClampMin = "0.0", Units = "s"))
	float WaitMax = 4.0f;
};
