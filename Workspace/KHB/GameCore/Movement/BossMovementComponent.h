#pragma once

#include "CoreMinimal.h"
#include "GameCore/Movement/FlyingMonsterMovementComponent.h"
#include "BossMovementComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnBossTransitionRiseCompleted);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnBossTransitionMoveCompleted);

/**
 * 보스의 실제 CharacterMovement 구현체입니다.
 *
 * 패턴 이동의 진행(접근, 앵커 이동, 높이 이동, Move During Ability)은 부모가 공통으로 처리한다.
 * 이 클래스는 보스에만 있는 것만 더한다.
 *  - 바닥과 앵커의 기준을 아레나 중심으로 잡는다.
 *  - 패턴 사이의 기본 움직임인 앵커 공전.
 *  - 페이즈 전환의 상승/이동과 아레나 중심 바라보기.
 */
UCLASS(ClassGroup = Movement, BlueprintType, meta = (BlueprintSpawnableComponent))
class TEAMPROJECT_FIN_API UBossMovementComponent : public UFlyingMonsterMovementComponent
{
	GENERATED_BODY()

public:
	UBossMovementComponent();
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

	/*
	 * 제자리 전투 여부. 현재 페이즈 정의의 bStationary를 보스 캐릭터가 페이즈마다 넣어 준다.
	 * 켜져 있으면 공전을 시작하지 않고, 패턴 준비 이동은 제자리 시전으로 바꾼다.
	 */
	void SetStationary(bool bInStationary) { bStationary = bInStationary; }
	bool IsStationary() const { return bStationary; }

	/** 보스의 기본 움직임은 아레나 앵커 공전이다. 프로필 Orbit 설정을 쓴다. */
	virtual bool BeginIdleMovement(AActor* InTargetActor) override;
	/** 제자리 전투 중이면 어떤 이동 후보든 제자리 시전(Hold Position)으로 처리한다. */
	virtual EEnemyPatternPreparationStatus BeginPatternPreparation(
		const FEnemyPatternDefinition& Pattern,
		const FEnemyPatternMovementOption& MovementOption,
		AActor* InTargetActor) override;
	/** 프로필 공전 시간이 지났는지 반환한다. 공전은 이 뒤에도 계속된다. */
	virtual bool CanFinishIdleMovement() const override;
	virtual void EndIdleMovement() override;

	bool TeleportToArenaAnchorOffset(const FVector& AnchorOffset);
	/** 앵커 기준 목표 Offset까지 XY/Z를 함께 이동한다. 페이즈 전환 전용 이동은 지면 Sweep를 무시한다. */
	bool BeginTransitionMoveToAnchorOffset(const FVector& TargetAnchorOffset, float SpeedOverride,
		float AccelerationOverride, float AcceptanceTolerance, float DurationOverride = 0.0f);

	UPROPERTY(BlueprintAssignable, Category = "Boss|Movement")
	FOnBossTransitionRiseCompleted OnTransitionRiseCompleted;

	UPROPERTY(BlueprintAssignable, Category = "Boss|Movement")
	FOnBossTransitionMoveCompleted OnTransitionMoveCompleted;

protected:
	/** 보스 이동 컴포넌트는 Boss Orbit 프로필을 쓴다. 속도 저장과 복구는 부모가 맡는다. */
	virtual EEnemyMovementType GetExpectedMovementType() const override { return EEnemyMovementType::BossOrbit; }

	/** 공전을 더한다. 나머지 이동은 비행 공용 처리를 쓴다. */
	virtual bool ResolvePatternMovementContext(const FEnemyPatternMovementOption& MovementOption) override;
	/** 아레나 중심이 앵커다. */
	virtual AActor* GetPatternMovementAnchor() const override;
	/** 아레나 앵커 높이를 바닥으로 본다. 앵커가 없으면 비행 공용 지면 트레이스를 쓴다. */
	virtual float GetPatternFloorZ() const override;
	virtual bool ShouldPreserveVelocityOnPatternBegin() const override { return bIdleHandoffPending; }
	/** 공전이 끝난 뒤 이동 없는 패턴이 오면 제자리에서 시전해야 하므로 항상 멈춘다. */
	virtual bool ShouldAlwaysHaltOnPatternStop() const override { return true; }
	virtual void TickPatternMovement(float DeltaTime) override;
	virtual EMonsterPatternMovementProgress EvaluatePatternMovementProgress() const override;
	virtual void HandlePatternMovementArrived() override;
	virtual FVector GetPatternMovementDirection() const override;
	/** 페이즈 전환 상승은 맵 아래에서 지면을 관통해 올라와야 한다. */
	virtual bool ShouldSweepPatternMovement() const override { return !bTransitionRiseActive; }
	virtual float GetPatternArrivalHeightTolerance() const override;
	virtual const AActor* GetPatternFacingTarget() const override;
	virtual void OnPatternMovementStopped() override;

private:
	/*
	 * 수평으로 아레나 중심(앵커)을 향해 돌린다. bSnap이면 보간 없이 바로 맞춘다.
	 * 이동 틱이 돌지 않는 구간(제자리 전투, 전환 상승)에서도 쓰려고 이동 틱과 따로 둔다.
	 */
	void RotateTowardAnchor(float DeltaTime, bool bSnap);

	FVector OrbitCenter = FVector::ZeroVector;
	float OrbitRadius = 0.0f;
	float OrbitDirection = 1.0f;
	/** 이번 공전을 끝낼 시간(초). 시작할 때 프로필의 공전 시간 범위에서 뽑는다. */
	float OrbitDuration = 0.0f;
	/** 직전 공전 방향. 0이면 아직 공전한 적이 없다. */
	float LastOrbitDirection = 0.0f;

	/** 기본 움직임 중인지. 패턴 이동과 같은 틱 경로를 쓰지만 공전 시간이 지나도 스스로 끝나지 않는다. */
	bool bIdleMovementActive = false;

	/** 현재 페이즈가 제자리 전투인지. SetStationary 로만 바뀐다. */
	bool bStationary = false;
	/*
	 * 기본 움직임이 방금 끝났는지.
	 *
	 * 곧바로 실제 이동이 이어지면 속도를 끊지 않고 이어받게 하는 표식이다.
	 * 다음 이동 정리(StopPatternMovement)에서 소모된다.
	 */
	bool bIdleHandoffPending = false;

	bool bTransitionRiseActive = false;
	float TransitionRiseAcceptanceTolerance = 75.0f;
	bool bUseTransitionMoveDuration = false;
	float TransitionMoveDuration = 0.0f;
	FVector TransitionMoveStartLocation = FVector::ZeroVector;
};
