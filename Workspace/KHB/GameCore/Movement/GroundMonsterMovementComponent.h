#pragma once

#include "CoreMinimal.h"
#include "MonsterMovementComponent.h"
#include "GroundMonsterMovementComponent.generated.h"

class AEnemyJumpNavLink;
class UAnimMontage;
class ACommonEnemyCharacter;

/*
 * 지상형 이동. 추격과 시전 전 이동을 NavMesh 경로로 한다.
 * 순간이동 재배치(Reposition For Action)는 이동이 아니라서 위치 보정 컴포넌트가 맡는다.
 */
UCLASS(ClassGroup = Movement, meta = (BlueprintSpawnableComponent))
class TEAMPROJECT_FIN_API UGroundMonsterMovementComponent : public UMonsterMovementComponent
{
	GENERATED_BODY()

  public:
	UGroundMonsterMovementComponent();
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
							   FActorComponentTickFunction* ThisTickFunction) override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void BeginPlay() override;
	bool TryStartNavLinkJump(AEnemyJumpNavLink* Link, const FVector& Destination);
	bool IsNavLinkJumpActive() const
	{
		return bNavLinkJumpActive;
	}
	void CancelNavLinkJump();
	// 도약 몽타주의 비행 구간 Notify State에서 호출합니다.
	void BeginNavLinkFlight();
	void EndNavLinkFlight();

	UPROPERTY(EditDefaultsOnly, Category = "Movement|NavLink")
	TObjectPtr<UAnimMontage> JumpUpMontage;
	UPROPERTY(EditDefaultsOnly, Category = "Movement|NavLink")
	TObjectPtr<UAnimMontage> JumpDownMontage;
	UPROPERTY(EditDefaultsOnly, Category = "Movement|NavLink")
	FName JumpWarpTargetName = TEXT("NavLinkLanding");
	UPROPERTY(EditDefaultsOnly, Category = "Movement|NavLink", meta = (ClampMin = "0.1"))
	float LandingWaitTimeout = 3.0f;
	UPROPERTY(EditDefaultsOnly, Category = "Movement|NavLink", meta = (ClampMin = "1.0"))
	float LandingTolerance = 60.0f;
	virtual void UpdateMovementTowardTarget(AActor* InTargetActor, float DeltaSeconds) override;
	virtual void UpdateMovementTowardLocation(const FVector& Destination, float DeltaSeconds) override;
	virtual void ResetMovementState() override;

  protected:
	virtual void HandleImpact(const FHitResult& Hit, float TimeSlice = 0.f,
		const FVector& MoveDelta = FVector::ZeroVector) override;
	/*
	 * 시전 전 이동은 NavMesh 경로로 간다. 벽이나 기둥을 돌아가야 하기 때문이다.
	 * 몽타주 병행 이동은 경로 추종이 루트모션과 충돌하므로 공용 스윕 이동을 그대로 쓴다.
	 */
	virtual void ApplyPatternMovement(const FVector& Direction, float DeltaTime) override;
	/** 시전 전 이동은 이동 방향으로 몸을 돌리는 CMC 설정에 맡기고, 병행 이동 중에만 타깃을 바라본다. */
	virtual void FacePatternMovementTarget(float DeltaTime) override;
	virtual void OnPatternMovementStopped() override;

	UPROPERTY(EditDefaultsOnly, Category = "Movement|Navigation", meta = (ClampMin = "0.0"))
	float AcceptanceRadius = 75.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Movement|Navigation", meta = (ClampMin = "0.05"))
	float RequestRefreshInterval = 0.25f;

	UPROPERTY(EditDefaultsOnly, Category = "Movement|Navigation", meta = (ClampMin = "0.0"))
	float DestinationRefreshDistance = 100.0f;

  private:
	void RecoverStoppedSplitMovement();
	void UpdateSplitMovementBlockers();
	TArray<TWeakObjectPtr<ACommonEnemyCharacter>> SplitMovementBlockers;
	FVector LastSplitRecoveryLocation = FVector::ZeroVector;
	double NextSplitRecoveryTime = 0.0;
	double LastMovementImpactTime = -1.0;
	FHitResult LastMovementImpact;

	UFUNCTION(NetMulticast, Reliable)
	void MulticastStartNavLinkMontage(UAnimMontage* Montage, FVector LandingLocation, FRotator LandingRotation);
	UFUNCTION(NetMulticast, Reliable)
	void MulticastFinishNavLinkMontage(bool bAbort);
	bool FindNavLinkLanding(FVector& OutLocation);
	bool ValidateNavLinkLanding(const FVector& FloorLocation, FVector& OutLocation);
	void IgnoreOtherMonsters();
	void RestoreNavLinkPresentation(bool bAbort);
	void FinishNavLinkJump(bool bSuccess);
	void HandleNavLinkOwnerDeath(class ABaseCharacter* Character);
	TWeakObjectPtr<AEnemyJumpNavLink> ActiveJumpLink;
	TWeakObjectPtr<UAnimMontage> ActiveJumpMontage;
	TArray<TWeakObjectPtr<AActor>> IgnoredMonsters;
	TArray<TWeakObjectPtr<AActor>> MonstersIgnoringOwner;
	FVector JumpFloorDestination = FVector::ZeroVector;
	FVector JumpLandingLocation = FVector::ZeroVector;
	double JumpDeadline = 0.0;
	double NextLandingSearchTime = 0.0;

	bool bNavLinkJumpActive = false;
	bool bJumpMontageStarted = false;
	bool bJumpPresentationStarted = false;
	bool bNavLinkFlight = false;
	bool bFlightNotifyReceived = false;
	bool bOwnsJumpActionTag = false;
	bool bSavedOrientToMovement = false;
	uint8 SavedRootMotionMode = 0;
	bool CanRefreshRequest(const FVector& Destination) const;
	void RecordRequest(const FVector& Destination);
	/** 패턴 이동의 경로 목적지. 접근은 편차 각도로 틀어진 방향의 정지 거리 지점이다. */
	FVector GetPatternNavDestination() const;

	TWeakObjectPtr<AActor> RequestedTarget;
	FVector LastRequestedDestination = FVector::ZeroVector;
	double NextRequestTime = 0.0;

	/** 패턴 이동이 경로 이동을 요청했는지. 끝날 때 그 경로만 멈추고 추격이 다시 요청하게 한다. */
	bool bPatternNavRequested = false;
};
