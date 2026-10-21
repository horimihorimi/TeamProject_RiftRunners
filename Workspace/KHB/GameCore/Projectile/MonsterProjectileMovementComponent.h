#pragma once

#include "CoreMinimal.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "MonsterProjectileMovementComponent.generated.h"

/**
 * 정해진 시간 동안만 타깃을 추적하는 "반쪽 호밍" 투사체 무브먼트.
 *
 * 엔진 기본 호밍(bIsHomingProjectile)은 HomingTargetComponent를 끝까지
 * 따라가서 절대 못 피하는 탄이 된다. 대신 ComputeAcceleration을
 * 오버라이드해 직접 가속을 넣고, HomingRemainingTime이 다하면 유도를
 * 꺼서 그 시점의 속도로 등속 직진시킨다. 이동 적분과 충돌 처리는 부모
 * 구현을 그대로 쓴다.
 *
 * 추적이 끝난 뒤가 플레이어의 회피 창이다. 지속시간이 곧 난이도이며,
 * 선회 반경은 대략 (속도^2 / HomingAccelerationMagnitude)로 정해진다.
 */
UCLASS()
class TEAMPROJECT_FIN_API UMonsterProjectileMovementComponent : public UProjectileMovementComponent
{
	GENERATED_BODY()

public:
	UMonsterProjectileMovementComponent();

	/*
	 * InHomingDuration 초 동안만 타깃을 추적하고, 그 뒤로는 직진한다.
	 *
	 * InInitialSpeed가 0보다 크면 UpdatedComponent의 정면 방향으로 속도를
	 * 덮어쓴다. 0이면 컴포넌트의 InitialSpeed가 그대로 남는다.
	 */
	void StartHomingToTarget(
		AActor* InTarget,
		float InHomingDuration,
		float InInitialSpeed = 0.0f);

	bool IsHomingActive() const { return bHomingActive; }
	float GetHomingRemainingTime() const { return HomingRemainingTime; }

	virtual void TickComponent(
		float DeltaTime,
		ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

	virtual FVector ComputeAcceleration(const FVector& InVelocity, float DeltaTime) const override;
	virtual EHandleBlockingHitResult HandleBlockingHit(
		const FHitResult& Hit,
		float TimeTick,
		const FVector& MoveDelta,
		float& SubTickTimeRemaining) override;
	virtual void HandleImpact(const FHitResult& Hit, float TimeSlice, const FVector& MoveDelta) override;

private:
	TWeakObjectPtr<AActor> HomingTarget = nullptr;

	// 남은 추적 시간. 0이 되면 유도를 끄고 이후로는 등속 직진한다.
	float HomingRemainingTime = 0.0f;

	bool bHomingActive = false;
};
