#pragma once

#include "GameCore/Ability/GA_BossPhaseTransitionBase.h"
#include "GameCore/Projectile/BossRingWave.h"
#include "GA_BossPhaseTransition.generated.h"

class UAbilityTask_WaitGameplayEvent;
class UGameplayEffect;

USTRUCT(BlueprintType)
struct FPhaseTransitionRingSpec
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ring")
	EBossRingWaveOrientation Orientation = EBossRingWaveOrientation::WorldHorizontal;
};

/** 한 Spawn/Fire 반복에서 동시에 생성·발사할 링 묶음이다. */
USTRUCT(BlueprintType)
struct FPhaseTransitionRingBurst
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ring")
	TArray<FPhaseTransitionRingSpec> Rings;
};

/** P1→P2 전환의 링 버스트 확장 GA. 공용 이동/몽타주 흐름은 Base가 담당한다. */
UCLASS(Abstract, Blueprintable)
class TEAMPROJECT_FIN_API UGA_BossPhaseTransition : public UGA_BossPhaseTransitionBase
{
	GENERATED_BODY()

public:
	UGA_BossPhaseTransition();
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Phase Transition|Ring") FName RingBurstSectionName = TEXT("RingBurstLoop");
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Phase Transition|Ring") FName RingBurstLoopSectionName = TEXT("RingBurstLoop");
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Phase Transition|Ring") FName NextSectionAfterRingBursts = TEXT("TentacleSequence");
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Phase Transition|Ring") TArray<FPhaseTransitionRingBurst> RingBursts;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Phase Transition|Ring") TSubclassOf<ABossRingWave> RingWaveClass;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Phase Transition|Ring", meta = (ClampMin = "0.0")) float ChargeRadius = 500.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Phase Transition|Ring", meta = (ClampMin = "0.0")) float ChargeExpansionSpeed = 350.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Phase Transition|Ring", meta = (ClampMin = "0.0")) float MaxRadius = 3000.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Phase Transition|Ring", meta = (ClampMin = "0.0")) float LaunchExpansionSpeed = 2500.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Phase Transition|Ring", meta = (ClampMin = "0.0")) float BandHalfWidth = 45.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Phase Transition|Ring", meta = (ClampMin = "0.0")) float PlaneHalfThickness = 30.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Phase Transition|Ring") FGameplayTag RingAttackTag;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Phase Transition|Ring") TSubclassOf<UGameplayEffect> RingDamageEffectClass;

	virtual void SetupTransitionSpecificEvents() override;
	virtual void HandleTransitionMoveCompleted() override;

	UFUNCTION() void OnRingSpawnEvent(FGameplayEventData Payload);
	UFUNCTION() void OnRingLaunchEvent(FGameplayEventData Payload);

private:
	void SpawnCurrentBurst();
	FRotator GetRingRotation(EBossRingWaveOrientation Orientation) const;

	/*
	 * 링이 겨냥할 대상. 전환 시작 때 저장한 타깃 → 현재 타깃 → 가장 가까운 플레이어 순이다.
	 * 모두 없으면 null이고, 이때 링은 보스 정면을 기준으로 선다.
	 */
	AActor* ResolveRingTarget() const;

	/*
	 * 전환을 시작할 때 보던 타깃.
	 *
	 * 전환 중 보스는 바닥 밑에서 올라오느라 시야가 막히고, 그 사이 컨트롤러의 주기 재평가가
	 * 후보 없음으로 타깃을 지운다. 올라온 직후 스폰되는 링이 그 빈틈에 걸리지 않게 여기서 붙잡아 둔다.
	 */
	TWeakObjectPtr<AActor> CachedRingTarget;

	UPROPERTY(Transient) TObjectPtr<UAbilityTask_WaitGameplayEvent> RingSpawnEventTask;
	UPROPERTY(Transient) TObjectPtr<UAbilityTask_WaitGameplayEvent> RingLaunchEventTask;
	TArray<TWeakObjectPtr<ABossRingWave>> PendingBurstRings;
	/** 이 전환 GA가 만든 모든 링. 발사된 링도 종료 시 반드시 제거한다. */
	TArray<TWeakObjectPtr<ABossRingWave>> ActiveTransitionRings;
	int32 CurrentBurstIndex = 0;
};
