#pragma once

#include "CoreMinimal.h"
#include "GameCore/AI/MonsterAIController.h"
#include "CommonEnemyAIController.generated.h"

/**
 * 새 Enemy Framework의 공용 Controller입니다.
 *
 * 기존 AMonsterAIController를 부모로 두어 BattleSector, Encounter, Perception,
 * TargetActor API와 호환됩니다. 일반 적은 이 클래스를 그대로 쓰고,
 * 보스는 ABossEnemyAIController를 씁니다.
 *
 * 일반 적의 평소 이동은 기존 매 틱 추격이 맡습니다(빙의 돌진, 자폭 접근 포함).
 * 패턴 준비 이동을 쓰는 적은 파생 컨트롤러에서 추격을 끕니다.
 */
UCLASS(Abstract, Blueprintable)
class TEAMPROJECT_FIN_API ACommonEnemyAIController : public AMonsterAIController
{
	GENERATED_BODY()

public:
	ACommonEnemyAIController();

	virtual void Tick(float DeltaSeconds) override;

	/** 시작부터 표시된 채 피격·사망만 검증할 적인지 여부입니다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy Framework|Test")
	bool bStationaryDamageTest = false;

	bool IsStationaryDamageTestEnabled() const { return bStationaryDamageTest; }
	void StopAIForStationaryDamageTest();

	/*
	 * 휴면 중이라 타깃 평가를 멈춘 상태인지 반환합니다.
	 *
	 * 새 Framework 적의 최초 타깃은 조우(보스는 등장 연출 종료) 시점에 정해져야 한다.
	 * 부모는 휴면 여부와 상관없이 주기·시야 평가를 돌리므로, 휴면 동안은 이 계층에서 막는다.
	 */
	UFUNCTION(BlueprintPure, Category = "AI|Perception")
	bool IsTargetEvaluationSuspended() const;

	/** 전투 대상이 될 수 있는 플레이어인지 반환합니다. 부모 컨트롤러의 후보 조건과 같습니다. */
	bool IsSelectableCombatPlayer(const AActor* Candidate) const;

	/*
	 * 월드의 선택 가능한 플레이어 중 가장 가까운 대상을 반환합니다. 없으면 nullptr.
	 *
	 * 시야 갱신 전이거나 조우를 일으킨 대상이 없을 때 최초 타깃의 폴백으로 쓴다.
	 */
	AActor* FindNearestCombatPlayer() const;

protected:
	virtual void BeginPlay() override;

private:
	bool bStationaryDamageTestActive = false;

	/** 휴면이면 부모의 시야 콜백을 떼고, 휴면이 풀리면 다시 붙인다. */
	void UpdatePerceptionCallbackBinding();
};
