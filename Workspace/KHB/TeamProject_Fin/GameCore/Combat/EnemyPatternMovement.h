#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "GameCore/Enemy/Data/EnemyDataAssets.h"
#include "EnemyPatternMovement.generated.h"

class AActor;

/** 패턴 실행 전 이동 준비의 현재 결과입니다. */
UENUM(BlueprintType)
enum class EEnemyPatternPreparationStatus : uint8
{
	Ready UMETA(DisplayName = "Ready"),
	Preparing UMETA(DisplayName = "Preparing"),
	Failed UMETA(DisplayName = "Failed")
};

/**
 * StateTree의 공용 Prepare 태스크가 몬스터별 이동 구현체와 통신하는 계약입니다.
 *
 * 패턴 선택과 StateTree는 이동 방식(비행 상승, 지상 접근, 순간이동)을 알지 않습니다.
 * 각 몬스터의 MovementComponent가 PatternDefinition을 해석해 준비를 수행합니다.
 */
UINTERFACE(BlueprintType)
class TEAMPROJECT_FIN_API UEnemyPatternMovement : public UInterface
{
	GENERATED_BODY()
};

class TEAMPROJECT_FIN_API IEnemyPatternMovement
{
	GENERATED_BODY()

public:
	/** 선택된 이동 후보로 준비를 시작합니다. */
	virtual EEnemyPatternPreparationStatus BeginPatternPreparation(
		const FEnemyPatternDefinition& Pattern,
		const FEnemyPatternMovementOption& MovementOption,
		AActor* TargetActor) = 0;

	/*
	 * 준비의 현재 상태를 반환합니다.
	 *
	 * 실제 이동 적분은 구현체가 자기 Tick에서 수행한다. 몽타주와 병행하는 이동은
	 * StateTree가 이미 Prepare를 빠져나간 뒤에도 계속되어야 하므로,
	 * 이 함수를 이동의 유일한 구동원으로 두면 그 경우가 끊긴다.
	 */
	virtual EEnemyPatternPreparationStatus UpdatePatternPreparation(
		const FEnemyPatternDefinition& Pattern,
		const FEnemyPatternMovementOption& MovementOption,
		AActor* TargetActor,
		float DeltaSeconds) = 0;

	virtual void CancelPatternPreparation() = 0;

	/**
	 * 타깃을 잃어도 현재 이동을 유지해도 되는지 반환합니다.
	 *
	 * 앵커 공전·높이 보정처럼 타깃이 필요 없는 이동은 true를 반환한다.
	 * 타깃 추적 이동은 false여야 기존 Target Lost 정책이 즉시 정지시킬 수 있다.
	 */
	virtual bool CanContinuePatternMovementWithoutTarget() const { return false; }

	/*
	 * 패턴 사이의 기본 움직임을 시작합니다.
	 *
	 * false를 반환하면 이 이동 방식은 기본 움직임을 지원하지 않는다. 호출한 쪽은
	 * 기다리지 않고 다음 패턴 선택으로 넘어가면 된다.
	 */
	virtual bool BeginIdleMovement(AActor* TargetActor) { return false; }

	/*
	 * 기본 움직임을 끝내도 되는지 반환합니다.
	 *
	 * 기준(시간, 도착 등)은 이동 방식마다 다르므로 구현체가 정한다. 참이 돼도 움직임은
	 * 스스로 멈추지 않으며, 실제로 끝낼 시점은 호출한 쪽이 정한다.
	 */
	virtual bool CanFinishIdleMovement() const { return true; }

	/** 기본 움직임을 끝냅니다. 다음 이동으로 이어질 수 있도록 속도를 급정지시키지 않습니다. */
	virtual void EndIdleMovement() {}

	/*
	 * 바닥 기준 현재 고도(cm)를 반환합니다.
	 *
	 * 바닥을 무엇으로 볼지(아레나 앵커, 지면 트레이스 등)는 이동 방식마다 달라 구현체가 정한다.
	 * 기본값 0은 지상 이동으로 취급한다는 뜻이다. 패턴 선택의 고도 조건이 이 값을 쓴다.
	 */
	virtual float GetHeightFromFloor() const { return 0.0f; }
};
