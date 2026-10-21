#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "MonsterCombatDirectorSubsystem.generated.h"

/**
 * 한 레벨의 몬스터 전투를 위에서 조율한다.
 *
 * 두 가지를 관리한다.
 *
 * 1. 배정 레지스트리: 어느 몬스터가 누구를 노리는지. 타깃 선정의 혼잡도
 *    감점(한 플레이어에게 몰리면 점수가 깎임)을 계산하는 근거다.
 *
 * 2. 어택 토큰: 한 플레이어를 동시에 때릴 수 있는 몬스터 수 상한. 토큰을
 *    쥔 몬스터만 공격 행동을 시작할 수 있고, 나머지는 접근/견제만 한다.
 *    적 수가 늘어도 실제 위협이 선형으로 커지지 않게 막는 장치이며,
 *    상한값이 곧 난이도 손잡이가 된다.
 *
 * 레벨 단위로 수명이 끝나야 하는 데이터라 GameInstance가 아니라 World에 둔다.
 * AIController는 서버에만 존재하므로 이 클래스도 서버에서만 의미가 있다.
 */
UCLASS()
class TEAMPROJECT_FIN_API UMonsterCombatDirectorSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/** 타깃이 바뀔 때마다 컨트롤러가 알린다. Target이 null이면 배정을 지운다. */
	void ReportTarget(AActor* Monster, AActor* Target);

	/** 몬스터가 죽거나 사라질 때 호출한다. 토큰도 함께 반납된다. */
	void UnregisterMonster(AActor* Monster);

	/** 이 대상을 지금 몇 마리가 노리고 있는가. */
	int32 CountMonstersTargeting(const AActor* Target) const;

	/** 배정이 기록된 전체 몬스터 수. 혼잡도 정규화의 분모다. */
	int32 GetTrackedMonsterCount() const;

	/*
	 * 공격 토큰을 요청한다. 같은 대상을 노리는 몬스터 중 이미 토큰을 쥔 수가
	 * 상한에 도달했으면 실패한다. 이미 쥐고 있으면 성공으로 처리한다.
	 */
	bool TryAcquireAttackToken(AActor* Monster);

	/** 공격이 끝나면 반드시 반납한다. 안 하면 그 대상은 더 이상 공격받지 않는다. */
	void ReleaseAttackToken(AActor* Monster);

	/** 이 몬스터가 지금 토큰을 쥐고 있는가. */
	bool HasAttackToken(const AActor* Monster) const;

	/** 대상 한 명을 동시에 공격할 수 있는 최대 몬스터 수. */
	void SetMaxAttackTokensPerTarget(int32 NewMax);
	int32 GetMaxAttackTokensPerTarget() const { return MaxAttackTokensPerTarget; }

private:
	/** 소멸한 액터가 남긴 항목을 걷어낸다. 조회 직전에 부른다. */
	void PruneStaleEntries() const;

	mutable TMap<TWeakObjectPtr<AActor>, TWeakObjectPtr<AActor>> MonsterToTarget;

	/** 토큰을 쥔 몬스터와, 쥘 당시의 대상. 대상이 바뀌면 토큰은 반납된다. */
	mutable TMap<TWeakObjectPtr<AActor>, TWeakObjectPtr<AActor>> AttackTokenHolders;

	/*
	 * 기본 2. 난이도를 올리려면 키운다. 값이 클수록 동시에 얻어맞는 양이
	 * 늘어 체감 난이도가 그대로 올라간다.
	 */
	int32 MaxAttackTokensPerTarget = 2;
};
