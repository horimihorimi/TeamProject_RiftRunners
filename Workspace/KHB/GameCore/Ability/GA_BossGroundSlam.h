#pragma once

#include "GameCore/Ability/GA_EnemyPatternBase.h"
#include "GA_BossGroundSlam.generated.h"

class UAbilityTask_WaitGameplayEvent;
class ABossRingWave;
class UGameplayEffect;

/** 중앙 앵커 이동 중 몽타주를 재생하고, 강타/링 수축 프레임을 이벤트로 받는다. */
UCLASS()
class TEAMPROJECT_FIN_API UGA_BossGroundSlam : public UGA_EnemyPatternBase
{
	GENERATED_BODY()

public:
	UGA_BossGroundSlam();
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
	UFUNCTION() void OnImpactEvent(FGameplayEventData Payload);
	UFUNCTION() void OnRetractEvent(FGameplayEventData Payload);

	UPROPERTY(EditDefaultsOnly, Category="Ground Slam") TSubclassOf<ABossRingWave> GroundSlamRingClass;
	UPROPERTY(EditDefaultsOnly, Category="Ground Slam") float ImpactRadius = 250.0f;
	UPROPERTY(EditDefaultsOnly, Category="Ground Slam") float RingMaxRadius = 3000.0f;
	UPROPERTY(EditDefaultsOnly, Category="Ground Slam") float RingExpandSpeed = 2500.0f;
	UPROPERTY(EditDefaultsOnly, Category="Ground Slam") float RingRetractSpeed = 1800.0f;
	/** 보스 액터 위치를 기준으로 한 수평 토러스의 Z 오프셋이다. */
	UPROPERTY(EditDefaultsOnly, Category="Ground Slam") float RingZOffset = 0.0f;
	UPROPERTY(EditDefaultsOnly, Category="Ground Slam") float RingBandHalfWidth = 60.0f;
	/** 내려찍는 순간의 보스 주변 즉발 피해에 사용할 공격 태그. */
	UPROPERTY(EditDefaultsOnly, Category="Ground Slam|Impact", meta=(Categories="Action.AI")) FGameplayTag ImpactAttackTag;
	UPROPERTY(EditDefaultsOnly, Category="Ground Slam") TSubclassOf<UGameplayEffect> ImpactDamageEffectClass;
	UPROPERTY(EditDefaultsOnly, Category="Ground Slam") FGameplayTag RingAttackTag;
	UPROPERTY(EditDefaultsOnly, Category="Ground Slam") TSubclassOf<UGameplayEffect> RingDamageEffectClass;

	/** 에디터/개발 빌드에서 내려찍기 즉발 피해 반경을 표시한다. */
	UPROPERTY(EditDefaultsOnly, Category="Ground Slam|Impact|Debug") bool bDebugDrawImpact = false;
	UPROPERTY(EditDefaultsOnly, Category="Ground Slam|Impact|Debug", meta=(EditCondition="bDebugDrawImpact", ClampMin="0.0")) float DebugImpactDrawDuration = 2.0f;

private:
	void ApplyImpactDamage(AActor* Avatar);

	UPROPERTY(Transient) TObjectPtr<UAbilityTask_WaitGameplayEvent> ImpactEventTask;
	UPROPERTY(Transient) TObjectPtr<UAbilityTask_WaitGameplayEvent> RetractEventTask;
	/** 이 강타 GA가 만든 링. GA가 끝나면 수축 이벤트를 기다리지 않고 정리한다. */
	TWeakObjectPtr<ABossRingWave> ActiveRing;
	bool bHasAppliedImpactDamage = false;
};
