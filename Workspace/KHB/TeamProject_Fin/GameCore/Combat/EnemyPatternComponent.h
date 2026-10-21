#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameCore/Enemy/Data/EnemyDataAssets.h"
#include "EnemyPatternComponent.generated.h"

class AActor;
class UEnemyPatternSetDataAsset;

/**
 * PatternSet DA의 정적 패턴 정의를 전투 중 선택 가능한 상태로 해석합니다.
 *
 * PatternDefinition 자체는 복사·소유하지 않습니다. 이 컴포넌트는 활성 PatternSet,
 * Current/Pending 인덱스, 쿨다운, 최근 사용 이력만 보관합니다.
 */
UCLASS(ClassGroup = (Enemy), BlueprintType, meta = (BlueprintSpawnableComponent))
class TEAMPROJECT_FIN_API UEnemyPatternComponent : public UActorComponent
{
	GENERATED_BODY()


public:
	UEnemyPatternComponent();

	/** 페이즈 전환 또는 Definition 초기화 시 활성 패턴 묶음을 교체합니다. */
	UFUNCTION(BlueprintCallable, Category = "Enemy|Pattern")
	void SetActivePatternSet(UEnemyPatternSetDataAsset* InPatternSet);

	UFUNCTION(BlueprintPure, Category = "Enemy|Pattern")

	UEnemyPatternSetDataAsset* GetActivePatternSet() const
	{
		return ActivePatternSet;
	}

	/** 후보를 평가해 CurrentPattern 또는 PendingPattern으로 저장합니다. */
	UFUNCTION(BlueprintCallable, Category = "Enemy|Pattern")
	bool SelectPattern(AActor* TargetActor, float HealthRatio, bool bStoreAsPending);

	bool SelectPrioritySplitPattern(AActor* TargetActor, float HealthRatio);

	/** PendingPattern이 현재 전투 조건에서도 유효한지 다시 검사합니다. */
	UFUNCTION(BlueprintCallable, Category = "Enemy|Pattern")
	bool ValidatePendingPattern(AActor* TargetActor, float HealthRatio) const;

	/** PendingPattern을 다음 실행할 CurrentPattern으로 승격합니다. */
	UFUNCTION(BlueprintCallable, Category = "Enemy|Pattern")
	bool PromotePendingPattern();

	/** CurrentPattern의 GA 실행이 실제로 시작될 때 호출해 쿨다운·이력을 기록합니다. */
	UFUNCTION(BlueprintCallable, Category = "Enemy|Pattern")
	bool CommitCurrentPattern();

	UFUNCTION(BlueprintCallable, Category = "Enemy|Pattern")
	void ClearCurrentPattern();

	UFUNCTION(BlueprintCallable, Category = "Enemy|Pattern")
	void ClearPendingPattern();

	UFUNCTION(BlueprintCallable, Category = "Enemy|Pattern")
	void ClearRuntimeState();

	UFUNCTION(BlueprintPure, Category = "Enemy|Pattern")
	bool HasCurrentPattern() const;

	UFUNCTION(BlueprintPure, Category = "Enemy|Pattern")
	bool HasPendingPattern() const;

	UFUNCTION(BlueprintPure, Category = "Enemy|Pattern")
	bool GetCurrentPattern(FEnemyPatternDefinition& OutPattern) const;

	UFUNCTION(BlueprintPure, Category = "Enemy|Pattern")
	bool GetPendingPattern(FEnemyPatternDefinition& OutPattern) const;

	/*
	 * CurrentPattern이 확정될 때 함께 뽑아 둔 이동 후보입니다.
	 *
	 * Prepare는 물론 Execute와 GA도 같은 후보를 봐야 한다. 몽타주 병행 여부에 따라
	 * 이동 잠금과 노티파이 처리가 갈리기 때문이다.
	 */
	UFUNCTION(BlueprintPure, Category = "Enemy|Pattern")
	bool GetCurrentMovementOption(FEnemyPatternMovementOption& OutOption) const;

private:
	const FEnemyPatternDefinition* GetPatternAt(int32 PatternIndex) const;

	/** 패턴이 Current로 확정될 때 이동 후보를 Weight로 하나 뽑습니다. */
	void ResolveCurrentMovementOption();

	bool IsPatternEligible(const FEnemyPatternDefinition& Pattern, AActor* TargetActor, float HealthRatio,
	                       double CurrentTimeSeconds) const;

	FString GetPatternIneligibilityReason(const FEnemyPatternDefinition& Pattern, AActor* TargetActor,
	                                      float HealthRatio, double CurrentTimeSeconds) const;

	int32 ChooseWeightedEligiblePattern(AActor* TargetActor, float HealthRatio) const;

	int32 GetConsecutiveUseCount(const FGameplayTag& PatternId) const;

	/** 소유 액터의 이동 컴포넌트가 잰 바닥 기준 고도. 이동 컴포넌트가 없으면 지상(0)으로 본다. */
	float GetOwnerHeightFromFloor() const;

	/** 소유 적의 이동 프로필이 정한 거리 기준. 프로필이 없으면 수평으로 본다. */
	EEnemyDistanceMeasure GetOwnerDistanceMeasure() const;

	/*
	 * 패턴의 GA가 지금 발동 가능한지 반환합니다.
	 *
	 * GAS 쿨다운 태그, 발동 차단 태그처럼 GA가 스스로 거는 조건을 본다.
	 * 여기서 거르지 않으면 발동이 실패할 패턴을 골라 루프가 헛돈다.
	 */
	bool CanActivatePatternAbility(const FGameplayTag& AbilityTag) const;

	double GetCurrentTimeSeconds() const;

	UPROPERTY(Transient)
	TObjectPtr<UEnemyPatternSetDataAsset> ActivePatternSet;

	int32 CurrentPatternIndex = INDEX_NONE;

	int32 PendingPatternIndex = INDEX_NONE;

	/** CurrentPattern과 함께 확정된 이동 후보. 이동이 없는 패턴이면 무효 상태로 둡니다. */
	FEnemyPatternMovementOption CurrentMovementOption;

	bool bHasCurrentMovementOption = false;

	/** PatternId별 다음 선택 가능 시간입니다. */
	TMap<FGameplayTag, double> CooldownEndTimes;

	/** 최근에 커밋된 PatternId. 마지막 원소가 가장 최근 사용입니다. */
	TArray<FGameplayTag> RecentPatternHistory;

	/** Select 실패가 매 틱 반복될 때 Output Log가 과도하게 쌓이는 것을 막습니다. */
	double LastSelectionDebugLogTime = -1.0;
};
