#pragma once

#include "CoreMinimal.h"
#include "../SectorEvent.h"
#include "RequestEncounterEvent.generated.h"
class ACommonEnemyCharacter;
class ABaseCharacter;

/** 레벨 배치 몬스터의 공용 조우 이벤트입니다. 원본과 분열 자식을 전투 종료까지 추적합니다. */
UCLASS(Blueprintable)
class TEAMPROJECT_FIN_API ARequestEncounterEvent : public AScenePlacedEvent
{
	GENERATED_BODY()

public:
	ARequestEncounterEvent();

	UPROPERTY(EditInstanceOnly, Category = "Encounter")
	TObjectPtr<AActor> EncounterTarget;

	// 기존 최종 보스의 통계 종료 동작을 유지하며, 중간 엘리트 방에서는 끕니다.
	UPROPERTY(EditInstanceOnly, Category = "Encounter")
	bool bFinalizeRunOnDefeat = true;

	virtual void Execute(const UObject* WorldContext = nullptr) override;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	bool TracksEnemy(ACommonEnemyCharacter* Enemy) const
	{
		return TrackedEnemies.Contains(Enemy);
	}

	FText GetEncounterName() const
	{
		return EncounterName;
	}

protected:
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
	void Track(ACommonEnemyCharacter* Enemy);

	void OnSplit(ACommonEnemyCharacter* Parent, ACommonEnemyCharacter* Left, ACommonEnemyCharacter* Right);

	void OnDeathStarted(ABaseCharacter* Enemy);

	UFUNCTION()
	void OnDeathFinished(class AMonsterCharacter* Enemy);

	void ReturnOwnedEnemy(ACommonEnemyCharacter* Enemy);

	bool HasSpawnOwner(ACommonEnemyCharacter* Enemy) const;

	UPROPERTY(Replicated)
	TArray<TObjectPtr<ACommonEnemyCharacter>> TrackedEnemies;

	UPROPERTY(Replicated)
	FText EncounterName;

	TArray<TWeakObjectPtr<ACommonEnemyCharacter>> RetiredEnemies;

	bool bExecutionDeferred = false;

	bool bRequested = false;

	bool bCompleted = false;
};
