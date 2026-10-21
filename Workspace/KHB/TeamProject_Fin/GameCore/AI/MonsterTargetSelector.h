#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "MonsterTargetSelector.generated.h"

class AMonsterAIController;

/**
 * 타깃 선정에 필요한 입력을 한 번에 넘기기 위한 묶음.
 *
 * 셀렉터는 월드를 직접 뒤지지 않고 여기 담긴 값만 본다. 그래야 같은 입력에
 * 대해 항상 같은 결과가 나오고, 테스트할 때 상황을 그대로 재현할 수 있다.
 */
USTRUCT()
struct FTargetSelectionContext
{
	GENERATED_BODY()

	// 판단 주체. 최근 피격 이력 조회 등에 쓴다.
	UPROPERTY()
	TObjectPtr<const AMonsterAIController> Controller = nullptr;

	// 후보 필터를 통과한 대상들. 비어 있으면 셀렉터를 호출하지 않는다.
	UPROPERTY()
	TArray<TObjectPtr<AActor>> Candidates;

	/*
	 * 지금 물고 있는 타깃. 없거나, 유지 시간 상한으로 강제 전환하는 중이면
	 * nullptr이 들어온다. 셀렉터는 이 값이 있을 때만 전환 문턱을 적용한다.
	 */
	UPROPERTY()
	TObjectPtr<AActor> CurrentTarget = nullptr;

	// 몬스터 위치. 거리 계산 기준점이다.
	FVector SelfLocation = FVector::ZeroVector;

	// 현재 타깃을 유지한 연속 시간(초). 전환 문턱 감쇠에 쓴다.
	float HoldTime = 0.0f;

	// 0보다 크면 셀렉터의 거리 점수 범위 대신 쓴다. 넓은 보스맵처럼 컨트롤러가 상황에 따라 넘긴다.
	float ScoringRangeOverride = 0.0f;
};

/**
 * 후보 중 누구를 칠지 정하는 정책 클래스.
 *
 * 컨트롤러는 "언제 평가할지, 후보를 어떻게 모을지, 결과를 어디에 저장할지"만
 * 담당하고 선정 규칙은 모른다. 몹 종류마다 다른 셀렉터를 끼워 성격을 바꾼다.
 */
UCLASS(Abstract, EditInlineNew, DefaultToInstanced, Blueprintable)
class TEAMPROJECT_FIN_API UMonsterTargetSelector : public UObject
{
	GENERATED_BODY()

public:
	/*
	 * 후보 중 하나를 고른다.
	 *
	 * Context.CurrentTarget을 그대로 돌려주면 "유지"를 뜻한다.
	 * nullptr을 돌려주면 타깃을 비운다.
	 */
	virtual AActor* SelectTarget(const FTargetSelectionContext& Context) const
		PURE_VIRTUAL(UMonsterTargetSelector::SelectTarget, return nullptr;);
};

/**
 * 가장 가까운 후보를 고른다.
 *
 * 전환은 새 후보가 현재 타깃보다 SwitchDistanceRatio 배 이상 가까울 때만
 * 일어난다. 거리가 비슷한 두 플레이어 사이에서 타깃이 매 평가마다
 * 뒤집히는 것을 막는 여유값이다.
 */
UCLASS(meta = (DisplayName = "Nearest Target Selector"))
class TEAMPROJECT_FIN_API UNearestTargetSelector : public UMonsterTargetSelector
{
	GENERATED_BODY()

public:
	virtual AActor* SelectTarget(const FTargetSelectionContext& Context) const override;

protected:
	UPROPERTY(EditAnywhere, Category = "Targeting", meta = (ClampMin = "0.1", ClampMax = "1.0"))
	float SwitchDistanceRatio = 0.8f;
};

/**
 * 거리·최근 피격·피해 Threat·혼잡도를 정규화해 가중합한 점수로 고른다.
 *
 *   Score(P) = W_Distance·D(P) + W_RecentDamage·R(P) + W_DamageThreat·T(P) − W_Crowd·C(P)
 *
 * 각 항은 [0,1]로 정규화한다. 곱이 아니라 합을 쓰는 이유는, 고려 항목을
 * 곱하면 항이 늘어날수록 점수가 0으로 수렴해 비교가 무의미해지기 때문이다.
 *
 * 전환은 다음 조건에서만 일어난다.
 *
 *   Score(최고 후보) > Score(현재 타깃) × Threshold(HoldTime)
 *
 * Threshold는 SwitchScoreRatio에서 시작해 유지 시간이 길어질수록 1.0으로
 * 내려간다. 초반에는 잘 안 바뀌고, 오래 물고 있을수록 쉽게 바뀐다.
 */
UCLASS(meta = (DisplayName = "Weighted Target Selector"))
class TEAMPROJECT_FIN_API UWeightedTargetSelector : public UMonsterTargetSelector
{
	GENERATED_BODY()

public:
	virtual AActor* SelectTarget(const FTargetSelectionContext& Context) const override;

protected:
	/** 점수를 계산한다. 로그로 근거를 남길 수 있도록 항별 값도 돌려준다. */
	float ComputeScore(
		const FTargetSelectionContext& Context,
		const AActor* Candidate,
		float& OutDistanceTerm,
		float& OutRecentTerm,
		float& OutDamageThreatTerm,
		float& OutCrowdTerm) const;

	/** 유지 시간에 따라 낮아지는 전환 문턱. 최저값은 1.0이다. */
	float ComputeSwitchThreshold(float HoldTime) const;

	// 거리 정규화 기준. 이 거리 이상은 전부 0점으로 본다.
	UPROPERTY(EditAnywhere, Category = "Targeting", meta = (ClampMin = "1.0"))
	float MaxScoringRange = 3000.0f;

	UPROPERTY(EditAnywhere, Category = "Targeting|Weights", meta = (ClampMin = "0.0"))
	float DistanceWeight = 1.0f;

	UPROPERTY(EditAnywhere, Category = "Targeting|Weights", meta = (ClampMin = "0.0"))
	float RecentDamageWeight = 0.6f;

	/** 실제 적용 피해량으로 쌓인 Threat의 가중치입니다. */
	UPROPERTY(EditAnywhere, Category = "Targeting|Weights", meta = (ClampMin = "0.0"))
	float DamageThreatWeight = 1.0f;

	/*
	 * 혼잡도 감점의 크기.
	 *
	 * 분산을 강제하는 값이 아니라 동점을 깨는 값이다. 앞장선 플레이어에게
	 * 몬스터가 몰리는 것은 그 플레이어의 선택이므로 코드가 막지 않는다.
	 * 0.15는 MaxScoringRange 3000 기준 450uu 차이를 뒤집는 크기다.
	 */
	UPROPERTY(EditAnywhere, Category = "Targeting|Weights", meta = (ClampMin = "0.0"))
	float CrowdWeight = 0.15f;

	/*
	 * 한 대상에 이만큼 몰리면 혼잡도가 최대(1.0)가 된다.
	 *
	 * 전체 몬스터 수로 나누면 방 크기와 등록 순서에 따라 같은 상황의 값이
	 * 달라져 튜닝이 불가능하다. 고정 기준을 써서 언제 평가하든 동일하게 만든다.
	 */
	UPROPERTY(EditAnywhere, Category = "Targeting|Weights", meta = (ClampMin = "1"))
	int32 CrowdSaturationCount = 3;

	// 이 시간 안에 나를 때린 상대에게 가산점을 준다.
	UPROPERTY(EditAnywhere, Category = "Targeting|Weights", meta = (ClampMin = "0.0"))
	float RecentDamageWindow = 3.0f;

	/*
	 * 전환 문턱의 시작값. 1.25면 새 후보가 25% 높은 점수를 내야 갈아탄다.
	 * MMO의 어그로 전환 임계값(근접 110% / 원거리 130%)과 같은 개념이다.
	 */
	UPROPERTY(EditAnywhere, Category = "Targeting|Switching", meta = (ClampMin = "1.0"))
	float SwitchScoreRatio = 1.25f;

	// 이 시간까지는 문턱을 그대로 유지한다.
	UPROPERTY(EditAnywhere, Category = "Targeting|Switching", meta = (ClampMin = "0.0"))
	float SwitchGraceTime = 3.0f;

	// 유예가 끝난 뒤 문턱이 1.0까지 내려가는 데 걸리는 시간.
	UPROPERTY(EditAnywhere, Category = "Targeting|Switching", meta = (ClampMin = "0.1"))
	float SwitchDecayTime = 7.0f;
};
