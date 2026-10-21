#pragma once

#include "GameCore/Ability/GA_EnemyPatternBase.h"
#include "GA_BossWhackTwist.generated.h"

class UMotionWarpingComponent;

/**
 * Whack Twist 전용 공격 GA다.
 *
 * 몽타주를 시작하기 전에 중앙 앵커 + 지정 높이를 Motion Warping 타깃으로 등록한다.
 * 실제 워프 적용 범위는 몽타주의 두 번째 하강 루트 모션 구간에 둔 NotifyState가 결정한다.
 */
UCLASS(Blueprintable)
class TEAMPROJECT_FIN_API UGA_BossWhackTwist : public UGA_EnemyPatternBase
{
	GENERATED_BODY()

public:
	UGA_BossWhackTwist();

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
	/** 몽타주의 Motion Warping NotifyState에 동일하게 입력할 이름이다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Whack Twist|Motion Warping")
	FName WarpTargetName = TEXT("BossCenterAnchor");

	/** 전투방 중앙 앵커 바닥에서의 최종 도착 높이(cm)다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Whack Twist|Motion Warping", meta = (ClampMin = "0.0"))
	float AnchorHeightFromFloor = 500.0f;

private:
	bool RegisterAnchorWarpTarget(const FGameplayAbilityActorInfo* ActorInfo);
	void ClearAnchorWarpTarget();

	TWeakObjectPtr<UMotionWarpingComponent> RegisteredMotionWarpingComponent;
	bool bWarpTargetRegistered = false;
};
