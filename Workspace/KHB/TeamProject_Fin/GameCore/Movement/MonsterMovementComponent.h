#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameCore/Combat/EnemyPatternMovement.h"
#include "GameCore/Movement/EnemyMovementProfileReceiver.h"
#include "MonsterMovementComponent.generated.h"

class UAnimMontage;
class UEnemyMovementProfileDataAsset;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnPatternMovementArrived, FName, ArrivalLoopSectionName, FName, ArrivalJumpSectionName);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPatternMovementEnded, bool, bArrived);

/** 패턴 이동 한 틱의 진행 판정입니다. */
enum class EMonsterPatternMovementProgress : uint8
{
	// 계속 이동한다.
	Continue,
	// 도착했지만 도착 타이밍(ArrivalLoop)을 기다리는 중이라 이번 틱은 움직이지 않는다.
	Hold,
	// 도착 없이 이동만 끝낸다(타깃 상실, 공전 시간 만료 등).
	Finished,
	// 목적지에 도착했다. 도착 처리(섹션 점프 방송 등)를 한다.
	Arrived
};

/**
 * 모든 몬스터 이동 컴포넌트의 공통 부모입니다.
 *
 * 컨트롤러 추격(UpdateMovementTowardTarget)과 함께, Enemy Framework의 이동 계약
 * (IEnemyPatternMovement, IEnemyMovementProfileReceiver)을 구현한다.
 *
 * 패턴 이동(시전 전 이동, 몽타주 병행 이동)의 진행 절차는 여기서 공통으로 돌린다.
 * 지상/비행/보스는 아래 가상 함수로 "무엇을 지원하는지"와 "어떻게 움직이는지"만 바꾼다.
 *  - 이 클래스: Approach Target, Move To Anchor(앵커가 있을 때), Move During Ability, 도착 섹션 점프
 *  - 비행: 높이(Height From Floor), Match Height, Descend To Floor
 *  - 보스: 아레나 앵커, 공전, 페이즈 전환 이동
 */
UCLASS(ClassGroup = Movement, meta = (BlueprintSpawnableComponent))
class TEAMPROJECT_FIN_API UMonsterMovementComponent
	: public UCharacterMovementComponent
	, public IEnemyPatternMovement
	, public IEnemyMovementProfileReceiver
{
	GENERATED_BODY()

public:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

	UFUNCTION(BlueprintPure, Category = "Enemy|Movement|Chase Slots")
	bool IsWaitingForChaseSlot() const { return bWaitingForChaseSlot; }
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

	/** AIController가 이동을 허용했을 때 호출하는 타깃 추적 공통 진입점. */
	virtual void UpdateMovementTowardTarget(AActor* InTargetActor, float DeltaSeconds);

	/** 순찰 등에서 계산된 월드 목적지로 이동하는 공통 진입점. */
	virtual void UpdateMovementTowardLocation(const FVector& Destination, float DeltaSeconds);

	/** 타깃 상실, 조우 초기화, 사망 시 파생 이동 상태를 초기화한다. */
	virtual void ResetMovementState();

	/*
	 * 이동 옵션을 해석해 패턴 이동을 시작한다.
	 *
	 * 이 이동 방식이 처리할 수 없는 Intent는 실패로 돌려준다. 패턴 DA 설정 실수를 로그로 드러낸다.
	 */
	virtual EEnemyPatternPreparationStatus BeginPatternPreparation(
		const FEnemyPatternDefinition& Pattern,
		const FEnemyPatternMovementOption& MovementOption,
		AActor* TargetActor) override;
	virtual EEnemyPatternPreparationStatus UpdatePatternPreparation(
		const FEnemyPatternDefinition& Pattern,
		const FEnemyPatternMovementOption& MovementOption,
		AActor* TargetActor,
		float DeltaSeconds) override;
	virtual void CancelPatternPreparation() override;
	virtual bool CanContinuePatternMovementWithoutTarget() const override;

	/*
	 * 프로필의 가속도와 이동 방식별 속도를 평소 값으로 저장하고, 지금 바꿔도 되면 바로 적용한다.
	 *
	 * 프로필 값이 0이면 처음 프로필을 받을 때의 컴포넌트 값을 유지한다.
	 */
	virtual void HandleMovementProfileChanged(const UEnemyMovementProfileDataAsset* Profile) override;

	/** 몽타주 Notify(AN_StopPatternMovement)가 병행 이동을 끝낼 때 호출한다. */
	UFUNCTION(BlueprintCallable, Category = "Enemy|Movement")
	void StopPatternMovement();

	UFUNCTION(BlueprintPure, Category = "Enemy|Movement")
	bool IsPatternMovementActive() const { return bPatternMovementActive; }

	/** 현재 병행 이동을 몽타주 루프 횟수 기준의 도착 타이밍으로 설정한다. */
	bool ConfigureArrivalLoopTiming(const UAnimMontage* Montage, float MontagePlayRate);

	/** Move During Ability 이동이 목적지에 도달했고, DA에 섹션 이름이 있을 때만 방송된다. */
	UPROPERTY(BlueprintAssignable, Category = "Enemy|Movement")
	FOnPatternMovementArrived OnPatternMovementArrived;

	/*
	 * 진행 중이던 패턴 이동이 끝날 때마다 방송된다. 도착했으면 true, 시간 만료·타깃 상실·노티파이 정지면 false.
	 *
	 * 몽타주 루프 섹션으로 이동을 기다리는 GA는 도착하지 못한 경우에도 다음 섹션으로 넘겨야 한다.
	 * 그러지 않으면 루프 섹션이 끝없이 돌고 GA가 끝나지 않는다.
	 */
	UPROPERTY(BlueprintAssignable, Category = "Enemy|Movement")
	FOnPatternMovementEnded OnPatternMovementEnded;

protected:
	/** 타깃은 공격/회전에 유지하고 추적 목적지만 공유 슬롯으로 바꾼다. */
	bool ResolveChaseSlotGoal(AActor* Target, bool bFlying, FVector& OutFloorGoal);
	void ReleaseChaseSlot();
	void HandleChaseOwnerDeath(class ABaseCharacter* Character);

	UPROPERTY(EditDefaultsOnly, Category = "Movement|Chase Slots")
	bool bUseChaseSlots = true;

	UPROPERTY(EditDefaultsOnly, Category = "Movement|Chase Slots", meta = (ClampMin = "1", ClampMax = "32"))
	int32 ChaseSlotCount = 8;

	/** 공격 사거리와 캡슐 크기에 맞춰 몬스터 BP에서 조절한다. 단위 cm. */
	UPROPERTY(EditDefaultsOnly, Category = "Movement|Chase Slots", meta = (ClampMin = "1.0"))
	float ChaseSlotRadius = 200.f;

	UPROPERTY(EditDefaultsOnly, Category = "Movement|Chase Slots", meta = (ClampMin = "1.0"))
	float ChaseWaitingRingSpacing = 150.f;

	UPROPERTY(EditDefaultsOnly, Category = "Movement|Chase Slots", meta = (ClampMin = "0", ClampMax = "4"))
	int32 ChaseWaitingRingCount = 2;

	UPROPERTY(EditDefaultsOnly, Category = "Movement|Chase Slots", meta = (ClampMin = "0.05"))
	float ChaseSlotRefreshInterval = 0.25f;

	UPROPERTY(EditDefaultsOnly, Category = "Movement|Chase Slots", meta = (ClampMin = "1.0"))
	float ChaseSlotAcceptanceRadius = 35.f;

	UPROPERTY(EditDefaultsOnly, Category = "Movement|Chase Slots")
	bool bDebugDrawChaseSlots = false;

	bool bWaitingForChaseSlot = false;
	bool bChaseSlotHolding = false;
	TWeakObjectPtr<AActor> ChaseSlotTarget;
	double NextChaseSlotRetryTime = 0.0;

	/** 이 컴포넌트가 기대하는 프로필 이동 방식. 맞지 않으면 경고만 남긴다. */
	virtual EEnemyMovementType GetExpectedMovementType() const { return EEnemyMovementType::Ground; }

	/*
	 * 프로필 MoveSpeed를 적용할 속도 값을 읽고 쓴다.
	 *
	 * 기본(지상)은 MovementSpeed 어트리뷰트가 속도를 정하므로 프로필 속도를 쓰지 않는다(음수 반환).
	 */
	virtual float GetProfileControlledSpeed() const { return -1.0f; }
	virtual void SetProfileControlledSpeed(float NewSpeed) {}

	/** 지금 평소 속도로 되돌려도 되는지. 패턴 이동이 속도를 임시로 바꾼 동안은 되돌리지 않는다. */
	virtual bool CanRestoreBaseMovementStatsNow() const { return !bPatternMovementActive; }

	/** 패턴 이동이 임시로 바꾼 속도와 가속도를 프로필 기본값으로 되돌린다. */
	void RestoreBaseMovementStats();

	/** 캐릭터의 현재 활성 프로필. Enemy Framework 캐릭터가 아니면 널이다. */
	const UEnemyMovementProfileDataAsset* GetActiveMovementProfile() const;

	/*
	 * ---- 패턴 이동 확장 지점 ----
	 */

	/*
	 * 이동 옵션에서 목적지·목표 높이 같은 이동 문맥을 정한다. false 면 이 방식이 지원하지 않는 이동이다.
	 *
	 * 기본은 Approach Target과 Move To Anchor(앵커가 있을 때)만 처리한다.
	 */
	virtual bool ResolvePatternMovementContext(const FEnemyPatternMovementOption& MovementOption);

	/** 고도까지 맞추는 이동인지. 지상은 수평만 움직이고 높이 오차를 보지 않는다. */
	virtual bool UsesVerticalPatternMovement() const { return false; }

	/** Move To Anchor의 기준점. 기본은 없음이라 일반 몬스터의 Move To Anchor는 실패한다. */
	virtual AActor* GetPatternMovementAnchor() const { return nullptr; }

	/** 바닥에 서 있을 때의 액터 Z. Height From Floor의 0 기준이다. */
	virtual float GetPatternFloorZ() const;

	/** 기본 움직임(공전 등)에서 곧바로 이어지는 이동이라 속도를 끊지 않고 이어받을지. */
	virtual bool ShouldPreserveVelocityOnPatternBegin() const { return false; }

	/** 진행 중인 패턴 이동이 없어도 정지 요청이 오면 속도를 급정지시킬지. */
	virtual bool ShouldAlwaysHaltOnPatternStop() const { return false; }

	virtual void TickPatternMovement(float DeltaTime);
	virtual EMonsterPatternMovementProgress EvaluatePatternMovementProgress() const;
	virtual void HandlePatternMovementArrived();
	virtual FVector GetPatternMovementDirection() const;
	virtual void ApplyPatternMovement(const FVector& Direction, float DeltaTime);

	/** 병행 이동을 충돌 스윕으로 할지. 끄면 지면을 관통해 움직인다(보스 페이즈 전환 상승). */
	virtual bool ShouldSweepPatternMovement() const { return true; }

	virtual float GetPatternArrivalHeightTolerance() const;
	virtual const AActor* GetPatternFacingTarget() const { return PatternMovementTarget.Get(); }
	virtual void FacePatternMovementTarget(float DeltaTime);

	/** 패턴 이동 상태가 정리된 직후 호출된다. 파생 클래스의 부가 상태를 여기서 정리한다. */
	virtual void OnPatternMovementStopped() {}

	UPROPERTY(Transient)
	TObjectPtr<AActor> PatternMovementTarget = nullptr;

	FEnemyPatternMovementOption ActivePatternOption;
	EEnemyPatternPreparationStatus PreparationStatus = EEnemyPatternPreparationStatus::Ready;
	bool bPatternMovementActive = false;
	float PatternElapsedTime = 0.0f;
	FVector PatternDestination = FVector::ZeroVector;
	float PatternTargetWorldZ = 0.0f;

	/** 접근 시작 때 MinTargetDistance~MaxTargetDistance에서 뽑은 종료 거리다. */
	float ApproachStopDistance = 0.0f;
	/** 접근 시작 때 뽑은 타깃 방향 기준 편차. 타깃 위치는 매 Tick 다시 읽는다. */
	float ApproachOffsetAngleDegrees = 0.0f;

	/** 몽타주 병행 이동의 현재 속도. CMC Velocity와 별개로 직접 적분한다. */
	FVector ConcurrentPatternVelocity = FVector::ZeroVector;
	/** ArrivalLoop 역산 속도. 0이면 GetMaxSpeed()를 쓴다. */
	float PatternSpeedOverride = 0.0f;
	bool bUseArrivalLoopTiming = false;
	float ArrivalTransitionTime = 0.0f;

	/** 이번 정리에서 급정지를 건너뛸지. BeginPatternPreparation이 취소 직전에만 켠다. */
	bool bPreserveVelocityOnStop = false;

	/** 이번 정리가 도착으로 인한 것인지. OnPatternMovementEnded에 실어 보낸다. */
	bool bStoppingOnArrival = false;

private:
	float NextMoveToAnchorDebugLogTime = 0.0f;

	/** 프로필 값이 0일 때 되돌아갈 값. 처음 프로필을 받을 때(스탯 적용 이후) 저장한다. */
	float ComponentDefaultSpeed = 0.0f;
	float ComponentDefaultMaxAcceleration = 0.0f;
	bool bHasComponentDefaults = false;

	/** 프로필에서 정한 평소 속도와 가속도. */
	float BaseSpeed = 0.0f;
	float BaseMaxAcceleration = 0.0f;
	bool bHasBaseMovementStats = false;
};
