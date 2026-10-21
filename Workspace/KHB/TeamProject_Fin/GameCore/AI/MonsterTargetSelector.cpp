#include "GameCore/AI/MonsterTargetSelector.h"

#include "GameCore/AI/MonsterAIController.h"
#include "GameCore/AI/MonsterCombatDirectorSubsystem.h"

AActor* UNearestTargetSelector::SelectTarget(const FTargetSelectionContext& Context) const
{
	AActor* BestCandidate = nullptr;
	float BestDistSq = TNumericLimits<float>::Max();

	for (AActor* Candidate : Context.Candidates)
	{
		if (!IsValid(Candidate))
		{
			continue;
		}

		const float DistSq =
			FVector::DistSquared(Context.SelfLocation, Candidate->GetActorLocation());

		if (DistSq < BestDistSq)
		{
			BestDistSq = DistSq;
			BestCandidate = Candidate;
		}
	}

	if (!IsValid(BestCandidate))
	{
		return nullptr;
	}

	// 현재 타깃이 없으면 비교할 것이 없다. 바로 최근접을 잡는다.
	if (!IsValid(Context.CurrentTarget) || BestCandidate == Context.CurrentTarget)
	{
		return BestCandidate;
	}

	const float CurrentDistSq =
		FVector::DistSquared(Context.SelfLocation, Context.CurrentTarget->GetActorLocation());
	const float Ratio = FMath::Clamp(SwitchDistanceRatio, 0.1f, 1.0f);

	// 확실히 더 가까울 때만 갈아탄다. 거리가 비슷하면 지금 타깃을 유지한다.
	return (BestDistSq < CurrentDistSq * Ratio * Ratio)
		? BestCandidate
		: Context.CurrentTarget.Get();
}

float UWeightedTargetSelector::ComputeScore(
	const FTargetSelectionContext& Context,
	const AActor* Candidate,
	float& OutDistanceTerm,
	float& OutRecentTerm,
	float& OutDamageThreatTerm,
	float& OutCrowdTerm) const
{
	OutDistanceTerm = 0.0f;
	OutRecentTerm = 0.0f;
	OutDamageThreatTerm = 0.0f;
	OutCrowdTerm = 0.0f;

	if (!IsValid(Candidate))
	{
		return -TNumericLimits<float>::Max();
	}

	// 거리: 가까울수록 1에 가깝다. MaxScoringRange 밖은 전부 0이다.
	const float Distance = FVector::Dist(Context.SelfLocation, Candidate->GetActorLocation());
	const float ScoringRange = Context.ScoringRangeOverride > 0.0f ? Context.ScoringRangeOverride : MaxScoringRange;
	OutDistanceTerm = FMath::Clamp(1.0f - Distance / FMath::Max(ScoringRange, 1.0f), 0.0f, 1.0f);

	// 최근 피격: 방금 나를 때린 상대에게 반응한다. 누적이 아니라 최근 여부만 본다.
	if (IsValid(Context.Controller)
		&& Context.Controller->WasRecentlyDamagedBy(Candidate, RecentDamageWindow))
	{
		OutRecentTerm = 1.0f;
	}

	// 피해 Threat: 실제 적용 피해량을 최대 체력 비율로 누적한 뒤 시간 감쇠한 값이다.
	if (IsValid(Context.Controller))
	{
		OutDamageThreatTerm = Context.Controller->GetDamageThreatNormalized(Candidate);
	}

	/*
	 * 혼잡도: 이미 몰린 대상은 감점한다. 한 플레이어에게 전부 달라붙는 것을
	 * 막되, 하드 배정이 아니라 감점이라 어그로가 강하면 여전히 몰릴 수 있다.
	 */
	if (const UWorld* World = IsValid(Context.Controller) ? Context.Controller->GetWorld() : nullptr)
	{
		if (const UMonsterCombatDirectorSubsystem* Director =
			World->GetSubsystem<UMonsterCombatDirectorSubsystem>())
		{
			const int32 TargetingCount = Director->CountMonstersTargeting(Candidate);
			OutCrowdTerm = FMath::Clamp(
				static_cast<float>(TargetingCount)
					/ static_cast<float>(FMath::Max(CrowdSaturationCount, 1)),
				0.0f,
				1.0f);
		}
	}

	return DistanceWeight * OutDistanceTerm
		+ RecentDamageWeight * OutRecentTerm
		+ DamageThreatWeight * OutDamageThreatTerm
		- CrowdWeight * OutCrowdTerm;
}

float UWeightedTargetSelector::ComputeSwitchThreshold(const float HoldTime) const
{
	const float StartRatio = FMath::Max(SwitchScoreRatio, 1.0f);

	if (HoldTime <= SwitchGraceTime)
	{
		return StartRatio;
	}

	/*
	 * 유예가 끝나면 문턱이 선형으로 1.0까지 내려간다. 오래 물고 있을수록
	 * 다른 후보로 넘어가기 쉬워지므로, 한 대상에 눌러앉지 않는다.
	 */
	const float Elapsed = HoldTime - SwitchGraceTime;
	const float Alpha = FMath::Clamp(Elapsed / FMath::Max(SwitchDecayTime, 0.1f), 0.0f, 1.0f);

	return FMath::Max(StartRatio - Alpha * (StartRatio - 1.0f), 1.0f);
}

AActor* UWeightedTargetSelector::SelectTarget(const FTargetSelectionContext& Context) const
{
	AActor* BestCandidate = nullptr;
	float BestScore = -TNumericLimits<float>::Max();
	float BestDistance = 0.0f;
	float BestRecent = 0.0f;
	float BestDamageThreat = 0.0f;
	float BestCrowd = 0.0f;

	for (AActor* Candidate : Context.Candidates)
	{
		float DistanceTerm = 0.0f;
		float RecentTerm = 0.0f;
		float DamageThreatTerm = 0.0f;
		float CrowdTerm = 0.0f;
		const float Score = ComputeScore(Context, Candidate, DistanceTerm, RecentTerm, DamageThreatTerm, CrowdTerm);

		if (Score > BestScore)
		{
			BestScore = Score;
			BestCandidate = Candidate;
			BestDistance = DistanceTerm;
			BestRecent = RecentTerm;
			BestDamageThreat = DamageThreatTerm;
			BestCrowd = CrowdTerm;
		}
	}

	if (!IsValid(BestCandidate))
	{
		return nullptr;
	}

	if (!IsValid(Context.CurrentTarget) || BestCandidate == Context.CurrentTarget)
	{
		return BestCandidate;
	}

	float CurrentDistance = 0.0f;
	float CurrentRecent = 0.0f;
	float CurrentDamageThreat = 0.0f;
	float CurrentCrowd = 0.0f;
	const float CurrentScore =
		ComputeScore(Context, Context.CurrentTarget, CurrentDistance, CurrentRecent, CurrentDamageThreat, CurrentCrowd);

	const float Threshold = ComputeSwitchThreshold(Context.HoldTime);

	/*
	 * 현재 점수가 0 이하로 떨어졌다면(혼잡도 감점이 커진 경우) 비율 비교가
	 * 의미를 잃는다. 그때는 더 높은 점수면 무조건 갈아탄다.
	 */
	const bool bShouldSwitch = (CurrentScore <= 0.0f)
		? (BestScore > CurrentScore)
		: (BestScore > CurrentScore * Threshold);

	if (!bShouldSwitch)
	{
		return Context.CurrentTarget.Get();
	}

	UE_LOG(LogTemp, Verbose,
		TEXT("[MonsterTarget] Score | New=%s(%.3f D=%.2f R=%.2f T=%.2f C=%.2f) Cur=%s(%.3f D=%.2f R=%.2f T=%.2f C=%.2f) Threshold=%.2f Hold=%.1f"),
		*GetNameSafe(BestCandidate), BestScore, BestDistance, BestRecent, BestDamageThreat, BestCrowd,
		*GetNameSafe(Context.CurrentTarget), CurrentScore, CurrentDistance, CurrentRecent, CurrentDamageThreat, CurrentCrowd,
		Threshold, Context.HoldTime);

	return BestCandidate;
}
