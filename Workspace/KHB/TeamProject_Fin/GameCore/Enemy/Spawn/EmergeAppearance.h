#pragma once

#include "CoreMinimal.h"
#include "GameCore/Character/MonsterCharacter.h"
#include "GameCore/Enemy/Spawn/SpawnAppearance.h"

#include "EmergeAppearance.generated.h"

class APawn;

/*
 * 숨어 있다가 바닥·벽·천장에서 튀어나와 곧바로 전투한다. 전투 섹터나 이벤트가 Execute 해야만 나온다.
 * 전투하다 플레이어를 놓쳐도 스폰 포인트로 돌아가지 않고 놓친 자리에서 기다린다.
 * 벽·천장에서 나온 몹이 그 자리로 되돌아가는 것은 어색하고, 그 자리에는 네비메시도 없을 수 있어서 순찰 영역을 주지 않는다.
 * StateTree의 Idle이 순찰 영역이 없는 몹을 제자리 대기로 보낸다
 */
UCLASS(meta = (DisplayName = "Emerge"))
class TEAMPROJECT_FIN_API UEmergeAppearance : public USpawnAppearance
{
	GENERATED_BODY()

public:
	virtual void Apply(AMonsterCharacter* Monster, AActor* InstigatorHint) override;

protected:
	UPROPERTY(EditAnywhere, Category = "Emerge")
	EEmergeSpawnType SpawnType = EEmergeSpawnType::Floor;

	UPROPERTY(EditAnywhere, Category = "Emerge")
	bool bUseTurnSection = false;

	UPROPERTY(EditAnywhere, Category = "Emerge")
	bool bUseTurnRotationWarp = true;

private:
	// 체인으로 불리면 힌트가 이벤트 액터라 쓸 수 없다. 그때는 가장 가까운 플레이어를 노린다
	APawn* FindNearestPlayerPawn(const FVector& From) const;
};
