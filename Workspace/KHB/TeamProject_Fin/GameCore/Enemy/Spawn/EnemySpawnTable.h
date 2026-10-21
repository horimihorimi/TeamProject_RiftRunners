#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameCore/Enemy/Spawn/EnemyType.h"

#include "EnemySpawnTable.generated.h"

class AMonsterCharacter;

// 몹 종류 → 실제 BP 클래스 대응표. 어느 에셋을 쓸지는 UEnemySpawnSettings가 정한다
UCLASS(BlueprintType)
class TEAMPROJECT_FIN_API UEnemySpawnTable : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	// 소프트 참조라 테이블을 읽는 것만으로 BP가 전부 올라오지 않는다. 로드는 풀이 필요할 때 건다
	UPROPERTY(EditAnywhere, Category = "Spawn")
	TMap<EEnemyType, TSoftClassPtr<AMonsterCharacter>> Enemies;

	// 등록되지 않은 종류면 비어 있는 포인터
	TSoftClassPtr<AMonsterCharacter> FindEnemyClass(EEnemyType Type) const
	{
		const TSoftClassPtr<AMonsterCharacter>* Found = Enemies.Find(Type);
		return Found ? *Found : TSoftClassPtr<AMonsterCharacter>();
	}
};
