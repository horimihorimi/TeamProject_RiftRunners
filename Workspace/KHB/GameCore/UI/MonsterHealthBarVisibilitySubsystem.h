#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "MonsterHealthBarVisibilitySubsystem.generated.h"

class AMonsterCharacter;

/** 로컬 화면의 몬스터 체력바를 통합 관리한다. 서버에 표시 상태를 복제하지 않는다. */
UCLASS()
class TEAMPROJECT_FIN_API UMonsterHealthBarVisibilitySubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()
public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	void RegisterMonster(AMonsterCharacter* Monster);
private:
	struct FEntry
	{
		TWeakObjectPtr<AMonsterCharacter> Monster;
		double NextCheckTime = 0.0;
		double OccludedSince = -1.0;
		bool bWithinDistance = false;
	};
	TArray<FEntry> Entries;
	int32 NextEntry = 0;
};
