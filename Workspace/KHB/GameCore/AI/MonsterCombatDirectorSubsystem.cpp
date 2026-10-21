#include "GameCore/AI/MonsterCombatDirectorSubsystem.h"

void UMonsterCombatDirectorSubsystem::ReportTarget(AActor* Monster, AActor* Target)
{
	if (!IsValid(Monster))
	{
		return;
	}

	if (!IsValid(Target))
	{
		MonsterToTarget.Remove(Monster);
		// 타깃이 없으면 공격할 일도 없다. 토큰을 쥔 채로 놓지 않는 것을 막는다.
		ReleaseAttackToken(Monster);
		return;
	}

	const TWeakObjectPtr<AActor>* PreviousTarget = MonsterToTarget.Find(Monster);
	const bool bTargetChanged = PreviousTarget == nullptr || PreviousTarget->Get() != Target;

	MonsterToTarget.Add(Monster, Target);

	/*
	 * 토큰은 "누구를 때릴 권한"이다. 대상이 바뀌면 그 권한은 의미를 잃으므로
	 * 반납시킨다. 그대로 두면 새 대상의 토큰 상한을 조용히 넘게 된다.
	 */
	if (bTargetChanged)
	{
		ReleaseAttackToken(Monster);
	}
}

void UMonsterCombatDirectorSubsystem::UnregisterMonster(AActor* Monster)
{
	if (!IsValid(Monster))
	{
		return;
	}

	MonsterToTarget.Remove(Monster);
	AttackTokenHolders.Remove(Monster);
}

int32 UMonsterCombatDirectorSubsystem::CountMonstersTargeting(const AActor* Target) const
{
	if (!IsValid(Target))
	{
		return 0;
	}

	PruneStaleEntries();

	int32 Count = 0;
	for (const TPair<TWeakObjectPtr<AActor>, TWeakObjectPtr<AActor>>& Pair : MonsterToTarget)
	{
		if (Pair.Value.Get() == Target)
		{
			++Count;
		}
	}

	return Count;
}

int32 UMonsterCombatDirectorSubsystem::GetTrackedMonsterCount() const
{
	PruneStaleEntries();
	return MonsterToTarget.Num();
}

bool UMonsterCombatDirectorSubsystem::TryAcquireAttackToken(AActor* Monster)
{
	if (!IsValid(Monster))
	{
		return false;
	}

	PruneStaleEntries();

	// 재진입은 성공으로 본다. 같은 공격 중 두 번 요청해도 상한을 갉아먹지 않는다.
	if (AttackTokenHolders.Contains(Monster))
	{
		return true;
	}

	const TWeakObjectPtr<AActor>* TargetPtr = MonsterToTarget.Find(Monster);
	AActor* Target = TargetPtr ? TargetPtr->Get() : nullptr;

	if (!IsValid(Target))
	{
		// 타깃이 없으면 조율할 대상도 없다. 막지 않는다.
		return true;
	}

	int32 HeldForTarget = 0;
	for (const TPair<TWeakObjectPtr<AActor>, TWeakObjectPtr<AActor>>& Pair : AttackTokenHolders)
	{
		if (Pair.Value.Get() == Target)
		{
			++HeldForTarget;
		}
	}

	if (HeldForTarget >= FMath::Max(MaxAttackTokensPerTarget, 1))
	{
		return false;
	}

	AttackTokenHolders.Add(Monster, Target);
	return true;
}

void UMonsterCombatDirectorSubsystem::ReleaseAttackToken(AActor* Monster)
{
	if (IsValid(Monster))
	{
		AttackTokenHolders.Remove(Monster);
	}
}

bool UMonsterCombatDirectorSubsystem::HasAttackToken(const AActor* Monster) const
{
	return IsValid(Monster) && AttackTokenHolders.Contains(Monster);
}

void UMonsterCombatDirectorSubsystem::SetMaxAttackTokensPerTarget(const int32 NewMax)
{
	MaxAttackTokensPerTarget = FMath::Max(NewMax, 1);
}

void UMonsterCombatDirectorSubsystem::PruneStaleEntries() const
{
	/*
	 * 몬스터가 파괴될 때 UnregisterMonster가 불리는 것이 정상 경로지만,
	 * 레벨 전환이나 강제 파괴처럼 그 경로를 타지 않는 경우가 있다.
	 * 조회 전에 한 번 걷어내 유령 항목이 혼잡도와 토큰 수를 부풀리지 않게 한다.
	 */
	for (auto It = MonsterToTarget.CreateIterator(); It; ++It)
	{
		if (!It.Key().IsValid() || !It.Value().IsValid())
		{
			It.RemoveCurrent();
		}
	}

	for (auto It = AttackTokenHolders.CreateIterator(); It; ++It)
	{
		if (!It.Key().IsValid() || !It.Value().IsValid())
		{
			It.RemoveCurrent();
		}
	}
}
