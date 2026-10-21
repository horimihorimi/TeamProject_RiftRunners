#pragma once

#include "CoreMinimal.h"
#include "MonsterMovementComponent.h"
#include "FlyingMonsterMovementComponent.generated.h"

UENUM(BlueprintType)
enum class EMonsterFlightApproachMode : uint8
{
	DirectTarget UMETA(DisplayName = "Direct Target"),
	DynamicHover UMETA(DisplayName = "Dynamic Hover"),
};

UCLASS(ClassGroup = Movement, meta = (BlueprintSpawnableComponent))
class TEAMPROJECT_FIN_API UFlyingMonsterMovementComponent : public UMonsterMovementComponent
{
	GENERATED_BODY()

public:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;
	virtual void UpdateMovementTowardTarget(AActor* InTargetActor, float DeltaSeconds) override;
	virtual void UpdateMovementTowardLocation(const FVector& Destination, float DeltaSeconds) override;
	virtual void ResetMovementState() override;

	/** 공격 애님 NotifyState 구간의 공용 비행 위치 보정이다. */
	void BeginAttackPositioning(AActor* InTargetActor, float InAttackDistance, float InHeightFromFloor, float InAcceptanceRadius,
		float InSpeedOverride = 0.0f, float InAccelerationOverride = 0.0f);
	/** NotifyState는 타깃만 갱신한다. 실제 이동은 이 컴포넌트 Tick의 마지막에 처리한다. */
	void TickAttackPositioning(AActor* InTargetActor);
	void EndAttackPositioning();

	/** 지면 트레이스로 잰 바닥 기준 현재 고도. 패턴의 고도 조건과 이동의 Height From Floor가 같은 기준을 쓴다. */
	virtual float GetHeightFromFloor() const override;

	EMonsterFlightApproachMode GetFlightApproachMode() const { return FlightApproachMode; }
	void SetFlightApproachMode(EMonsterFlightApproachMode NewMode);

	bool IsStrafeEnabled() const { return bStrafeEnabled; }
	void SetStrafeEnabled(bool bEnabled);

protected:
	virtual EEnemyMovementType GetExpectedMovementType() const override { return EEnemyMovementType::Flying; }
	/** 비행형은 프로필 MoveSpeed로 MaxFlySpeed를 정한다. */
	virtual float GetProfileControlledSpeed() const override { return MaxFlySpeed; }
	virtual void SetProfileControlledSpeed(const float NewSpeed) override { MaxFlySpeed = NewSpeed; }

	/*
	 * 비행은 공용 패턴 이동(접근, 앵커 이동)에 높이를 더하고, 높이 전용 이동(Match Height, Descend To Floor)을 지원한다.
	 * 높이는 옵션의 Height From Floor, 없으면 프로필 Flying.DefaultHeightFromFloor, 그것도 없으면 현재 고도다.
	 */
	virtual bool ResolvePatternMovementContext(const FEnemyPatternMovementOption& MovementOption) override;
	virtual bool UsesVerticalPatternMovement() const override { return true; }
	/** 지면 트레이스 지점에 캡슐 반높이를 더한 값. 바닥에 서 있을 때의 액터 Z 다. */
	virtual float GetPatternFloorZ() const override;
	virtual FVector GetPatternMovementDirection() const override;

	/** 이동 옵션이 요구하는 바닥 기준 높이(cm). */
	float ResolvePatternHeightFromFloor(const FEnemyPatternMovementOption& MovementOption) const;

	UPROPERTY(EditDefaultsOnly, Category = "Movement")
	float MovementRangeThreshold = 400.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Movement", meta = (ClampMin = "0.0"))
	float MovementModeHysteresis = 50.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Movement|Strafe")
	float StrafeRadialCorrectionSpeed = 2.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Movement|Fly", meta = (ClampMin = "0.0"))
	float BaseHoverHeight = 200.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Movement|Fly")
	float RandomHeightOffsetMin = 0.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Movement|Fly")
	float RandomHeightOffsetMax = 0.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Movement|Fly", meta = (ClampMin = "0.1"))
	float HoverTargetChangeInterval = 2.5f;

	UPROPERTY(EditDefaultsOnly, Category = "Movement|Fly", meta = (ClampMin = "0.0"))
	float MinimumGroundClearance = 150.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Movement|Fly", meta = (ClampMin = "0.0"))
	float MaximumGroundClearance = 800.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Movement|Fly", meta = (ClampMin = "0.0"))
	float GroundTraceDistance = 3000.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Movement|Fly", meta = (ClampMin = "0.0"))
	float FlightHeightInterpSpeed = 2.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Movement|Fly", meta = (ClampMin = "0.0"))
	float HeightTolerance = 20.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Movement|Fly", meta = (ClampMin = "1.0"))
	float VerticalCorrectionRange = 200.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Movement|Fly", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MaxVerticalMovementInputScale = 0.5f;

	UPROPERTY(EditDefaultsOnly, Category = "Movement|Fly", meta = (ClampMin = "0.0"))
	float FlyTurnInterpSpeed = 180.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Movement|Fly", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float FlyMovementInputScale = 1.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Movement|Fly")
	EMonsterFlightApproachMode FlightApproachMode = EMonsterFlightApproachMode::DirectTarget;

	UPROPERTY(EditDefaultsOnly, Category = "Movement|DynamicHover", meta = (ClampMin = "0.1"))
	float HoverAnchorRefreshInterval = 1.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Movement|DynamicHover", meta = (ClampMin = "0.0", ClampMax = "180.0"))
	float HoverAnchorRefreshAngle = 35.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Movement|DynamicHover", meta = (ClampMin = "0.0"))
	float HoverAnchorTargetMoveThreshold = 200.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Movement")
	bool bFaceTarget = true;

	UPROPERTY(EditDefaultsOnly, Category = "Movement", meta = (EditCondition = "bFaceTarget", ClampMin = "0.0"))
	float FacingInterpSpeed = 8.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Debug|Movement")
	bool bDebugDrawFlightDirection = false;

	UPROPERTY(EditDefaultsOnly, Category = "Debug|Movement", meta = (ClampMin = "1.0"))
	float DebugFlightDirectionLength = 300.0f;

	/*
	 * 몹이 타깃을 추적하는 이동 경로를 그린다. 초록 = 경로를 따라감, 빨강 = 통과 검사 실패로 대기 후 재탐색,
	 * 노랑 = 경로 없이 직선(뚫려 있음), 주황 = 직선이 막혔는데 경로도 못 구함, 하늘색 세로선 = 경로 지점별 캡슐 중심 허용 높이,
	 * 자홍색 점 = 지금 목표 부유 높이. 개발 빌드에서만 그려진다
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Debug|Movement")
	bool bDebugDrawChasePath = false;

	/*
	 * 위치 이동(순찰 등)에서 경로 지점에 닿았다고 볼 수평 거리(cm).
	 * 비행은 NavMesh 위를 걷지 못하지만 바닥 경로는 물어볼 수 있다. 그 지점들 위를 날아 벽을 돌아간다
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Movement|Location Path", meta = (ClampMin = "10.0"))
	float LocationPathPointAcceptance = 100.0f;

	/** 목적지가 이만큼 바뀌면 경로를 다시 구한다(cm). */
	UPROPERTY(EditDefaultsOnly, Category = "Movement|Location Path", meta = (ClampMin = "1.0"))
	float LocationPathRebuildDistance = 50.0f;

	/** 공중에서 바닥 NavMesh를 찾을 때 아래로 내려다볼 거리(cm). 비행 높이보다 커야 한다. */
	UPROPERTY(EditDefaultsOnly, Category = "Movement|Location Path", meta = (ClampMin = "100.0"))
	float LocationPathProjectDepth = 3000.0f;

	/*
	 * 타깃 추적 중 몹에서 타깃까지 직선이 벽에 막히면, 위치 이동과 같은 바닥 경로를 따라 벽을 돌아간다.
	 * 막힘은 퍼셉션(보이는가)이 아니라 이동 컴포넌트가 직접 트레이스로 판단한다. 끄면 예전처럼 항상 직선으로 간다
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Movement|Chase Path")
	bool bUseChasePath = true;

	// 직선이 막혔는지 다시 재는 간격. 결과는 이 시간 동안 재사용해 몹마다 매 프레임 트레이스하지 않는다
	UPROPERTY(EditDefaultsOnly, Category = "Movement|Chase Path", meta = (ClampMin = "0.05", Units = "s"))
	float StraightPathCheckInterval = 0.1f;

	// 막혀 있는 동안 경로를 다시 구하는 최소 간격. 경로 탐색은 동기 호출이라 너무 짧게 잡지 않는다
	UPROPERTY(EditDefaultsOnly, Category = "Movement|Chase Path", meta = (ClampMin = "0.1", Units = "s"))
	float ChasePathRebuildInterval = 0.5f;

	// 직선 막힘과 천장 높이를 잴 때 쓰는 트레이스 채널
	UPROPERTY(EditDefaultsOnly, Category = "Movement|Chase Path")
	TEnumAsByte<ECollisionChannel> ObstacleTraceChannel = ECC_Visibility;

	/*
	 * 목표 높이가 천장에 닿지 않게 낮춘다. 바닥 기준 높이를 고정으로 유지하면 천장이 낮은 곳이나
	 * 문틀 아래에서 천장에 박히므로, 위쪽을 트레이스해 (천장 - 캡슐 - 여유)를 넘지 않게 한다
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Movement|Ceiling")
	bool bLimitHeightByCeiling = true;

	// 몹 위쪽으로 천장을 찾는 최대 거리(cm). 이 안에 천장이 없으면 제한하지 않는다
	UPROPERTY(EditDefaultsOnly, Category = "Movement|Ceiling", meta = (ClampMin = "100.0", Units = "cm"))
	float CeilingTraceDistance = 1500.0f;

	// 캡슐 윗면과 천장 사이에 남길 여유(cm)
	UPROPERTY(EditDefaultsOnly, Category = "Movement|Ceiling", meta = (ClampMin = "0.0", Units = "cm"))
	float CeilingClearance = 30.0f;

	// 평소에는 천장과 충분히 떨어지고, 좁은 통로에서는 CeilingClearance까지 허용한다.
	UPROPERTY(EditDefaultsOnly, Category = "Movement|Ceiling", meta = (ClampMin = "0.0", Units = "cm"))
	float ComfortableCeilingClearance = 70.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Movement|Ceiling", meta = (ClampMin = "0.0", Units = "s"))
	float CeilingReleaseDelay = 0.5f;

	// 이동 방향 앞쪽에서도 천장을 재는 거리(cm). 문틀 앞에서 미리 높이를 낮추기 위해서다
	UPROPERTY(EditDefaultsOnly, Category = "Movement|Ceiling", meta = (ClampMin = "0.0", Units = "cm"))
	float CeilingLookAheadDistance = 250.0f;

	// 천장을 다시 재는 간격. 결과는 이 시간 동안 재사용한다
	UPROPERTY(EditDefaultsOnly, Category = "Movement|Ceiling", meta = (ClampMin = "0.05", Units = "s"))
	float CeilingCheckInterval = 0.1f;

	/*
	 * 바닥 네비 경로를 구한 뒤, 경로를 따라 몹의 몸(캡슐)이 실제로 지나갈 수 있는지 검사한다.
	 * 네비메시는 걷는 에이전트 크기로 만들어져서, 경로가 나와도 캡슐이 더 크거나 부유 높이가 높으면 문틀에 걸린다.
	 * 경로를 촘촘히 나눠 각 지점의 바닥과 천장으로 캡슐 중심의 허용 높이 범위를 만들고, 인접 지점 사이는 캡슐로 쓸어 폭을 확인한다.
	 * 지나갈 수 없는 경로는 따라가지 않고 로그를 남긴다. 지나갈 수 있으면 그 범위로 부유 높이를 미리 맞춘다
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Movement|Path Clearance")
	bool bValidateBodyClearance = true;

	// 경로를 나눠 바닥·천장을 재는 간격(cm)
	UPROPERTY(EditDefaultsOnly, Category = "Movement|Path Clearance", meta = (ClampMin = "50.0", Units = "cm"))
	float ClearanceSampleSpacing = 150.0f;

	// 캡슐 바닥과 경로 바닥 사이에 남길 여유(cm)
	UPROPERTY(EditDefaultsOnly, Category = "Movement|Path Clearance", meta = (ClampMin = "0.0", Units = "cm"))
	float FloorClearanceMargin = 10.0f;

	// 이동 속도에 이 시간을 곱한 거리만큼 앞 구간까지 보고 높이를 미리 맞춘다(초). 낮은 문틀에 닿기 전에 내려가기 위해서다
	UPROPERTY(EditDefaultsOnly, Category = "Movement|Path Clearance", meta = (ClampMin = "0.1", Units = "s"))
	float ClearanceLookAheadTime = 1.0f;

	// 낮은 구간에 맞춰 내려갈 때 높이 보간 속도에 곱하는 배율. 1이면 평소 속도로 내려간다
	UPROPERTY(EditDefaultsOnly, Category = "Movement|Path Clearance", meta = (ClampMin = "1.0"))
	float ClearanceDescentInterpMultiplier = 3.0f;

	// 폭 검사에서 한 구간을 캡슐로 쓸어 볼 높이 단계 수. 이 높이들 중 하나라도 지나가면 그 구간은 지나갈 수 있다
	UPROPERTY(EditDefaultsOnly, Category = "Movement|Path Clearance", meta = (ClampMin = "2", ClampMax = "12"))
	int32 ClearanceHeightLevels = 5;

	// 가장 낮은 허용 높이에서 위로 이만큼까지만 높이를 바꿔 가며 쓸어 본다(cm)
	UPROPERTY(EditDefaultsOnly, Category = "Movement|Path Clearance", meta = (ClampMin = "0.0", Units = "cm"))
	float ClearanceSearchSpan = 300.0f;

	// 폭 검사에 쓰는 캡슐의 크기 배율. 경로 지점이 벽 모서리에 바짝 붙어 있어서 실제 크기 그대로 쓸면 모서리마다 걸린다
	UPROPERTY(EditDefaultsOnly, Category = "Movement|Path Clearance", meta = (ClampMin = "0.3", ClampMax = "1.0"))
	float ClearanceCapsuleScale = 0.9f;

private:
	/*
	 * Destination 까지의 바닥 경로를 구해 둔다. 경로 지점마다 목적지와 같은 비행 높이를 더해 쓴다.
	 * 부분 경로는 도달 가능한 끝까지 사용한다. 재탐색 실패 시 기존 경로를 보존한다.
	 */
	bool BuildLocationPath(const FVector& Destination);
	void ClearLocationPath();

	// 추적 경로를 화면에 그린다. bDebugDrawChasePath가 꺼져 있거나 개발 빌드가 아니면 아무것도 하지 않는다
	void DrawChaseDebug(const AActor* Target) const;

	// 방금 구한 바닥 경로를 따라 몹 캡슐이 지나갈 수 있는지 재고 지점별 허용 높이를 만든다. 못 지나가면 bPathBodyPassable이 거짓이 된다
	void BuildPathClearance(bool bUseFullCapsule = false);

	// 몹 앞쪽(이동 속도 x ClearanceLookAheadTime)의 경로 지점들이 정한, 캡슐 중심이 있어야 할 높이 범위. 따라갈 경로가 없으면 거짓
	bool GetPathHeightBand(float& OutMinCenterZ, float& OutMaxCenterZ);

	// From에서 To까지 직선이 벽에 막혀 있는가. IgnoreActor(타깃)와 다른 폰에 걸린 것은 막힘으로 보지 않는다
	bool IsStraightPathBlocked(const FVector& From, const FVector& To, const AActor* IgnoreActor);

	/*
	 * 추적 목적지를 정한다. 직선이 뚫려 있으면 Destination 그대로, 막혀 있으면 바닥 경로의 다음 지점이다.
	 * 막힌 직선으로 폴백하지 않는다. 경로가 없거나 통과 불가이면 현재 위치에서 재탐색을 기다린다.
	 */
	FVector ResolveChaseDestination(const FVector& Destination, const AActor* IgnoreActor);

	// Heading 방향의 앞쪽까지 포함해 몹 위 천장 높이를 잰다. 결과는 CeilingCheckInterval 동안 재사용한다
	void RefreshCeiling(const FVector& Heading);

	// WorldZ(캡슐 중심 기준)가 천장에 닿지 않게 낮춘 값. 천장을 못 찾았으면 그대로다
	float LimitHeightByCeiling(float WorldZ) const;

	TArray<FVector> FlightJoinPoints;
	int32 FlightJoinIndex = 0;
	double NextFlightJoinSearchTime = 0.0;
	TArray<FVector> LocationPathPoints;
	int32 LocationPathIndex = 0;
	FVector LocationPathDestination = FVector::ZeroVector;
	bool bHasLocationPathQuery = false;
	bool bLocationPathPartial = false;
	bool bLastPathBuildFailed = false;
	FVector ChaseMovementDestination = FVector::ZeroVector;
	bool bChaseWaitingForPath = false;
	bool bChaseWaitingForHeight = false;

	// 경로 지점 하나에서 캡슐 중심이 있을 수 있는 높이 범위. 바닥 + 반높이 + 여유 ~ 천장 - 반높이 - 여유
	struct FPathClearanceSample
	{
		FVector Location = FVector::ZeroVector;
		float MinCenterZ = 0.0f;
		float MaxCenterZ = 0.0f;
	};

	// 네비 경로의 원본 지점(바닥 위). LocationPathPoints는 첫 지점을 빼고 높이를 얹어 저장해서 바닥 높이가 남지 않는다
	TArray<FVector> NavPathFloorPoints;
	TArray<FPathClearanceSample> ClearanceSamples;
	int32 ClearanceSampleIndex = 0;
	bool bPathBodyPassable = true;
	double NextClearanceLogTime = 0.0;

	// 천장·경로 제한을 걸기 전의 목표 부유 높이(캡슐 중심 Z). 직선 막힘 판단의 캡슐 스윕 높이다
	float PreferredHoverHeight = 0.0f;
	bool bHasPreferredHoverHeight = false;

	bool bStraightPathBlocked = false;
	double NextStraightPathCheckTime = 0.0;
	double NextChasePathRebuildTime = 0.0;

	double NextCeilingTraceTime = 0.0;
	bool bHasCeiling = false;
	double LastCeilingHitTime = -1.0;
	float CachedCeilingZ = 0.0f;

	void UpdateDesiredFlightHeight(float DeltaSeconds);
	void ApplyVerticalMovement() const;
	void PerformStrafeMovement();
	void UpdateDynamicHoverAnchor();
	FVector GetFlyDestination() const;
	void PerformFlyMovement(float DeltaSeconds);
	void RotateTowardTarget(float DeltaSeconds) const;
	void MoveTowardLocation(const FVector& Destination, float DeltaSeconds);
	float GetGroundZForAttackPositioning() const;
	void ApplyAttackPositioningMovement(float DeltaSeconds);

	TWeakObjectPtr<AActor> TargetActor;
	bool bIsInStrafeMode = false;
	bool bStrafeEnabled = true;
	float StrafeDirection = 1.0f;
	bool bHasStrafeDirection = false;
	float DesiredFlightHeight = 0.0f;
	bool bHasDesiredFlightHeight = false;
	float CurrentHoverOffset = 0.0f;
	bool bHasHoverOffset = false;
	double NextHoverOffsetChangeTime = 0.0;
	float SmoothedGroundHeight = 0.0f;
	bool bHasGroundHeight = false;
	double NextGroundTraceTime = 0.0;
	FVector SmoothedFlyDirection = FVector::ZeroVector;
	FVector HoverAnchorLocation = FVector::ZeroVector;
	FVector HoverAnchorTargetLocation = FVector::ZeroVector;
	bool bHasHoverAnchor = false;
	double NextHoverAnchorRefreshTime = 0.0;

	bool bAttackPositioningActive = false;
	float AttackPositioningDistance = 0.0f;
	float AttackPositioningHeightFromFloor = 0.0f;
	float AttackPositioningAcceptanceRadius = 0.0f;
	float AttackPositioningMaxSpeed = 0.0f;
	float AttackPositioningAcceleration = 0.0f;
	TWeakObjectPtr<AActor> AttackPositioningTarget;
	FVector AttackPositioningVelocity = FVector::ZeroVector;
};
