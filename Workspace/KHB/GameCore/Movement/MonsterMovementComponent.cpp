#include "MonsterMovementComponent.h"

#include "Animation/AnimMontage.h"
#include "GameCore/Character/CommonEnemyCharacter.h"
#include "GameCore/Enemy/Data/EnemyDataAssets.h"
#include "GameFramework/Character.h"
#include "GameCore/AI/MonsterAIController.h"
#include "MonsterChaseSlotSubsystem.h"
#include "Engine/World.h"
#include "DrawDebugHelpers.h"
#include "Components/CapsuleComponent.h"

namespace
{
	FVector ObservedPatternTargetLocation(const ACharacter* Character, const AActor* Target)
	{
		FVector Location = Character->GetActorLocation();
		const AMonsterAIController* Controller = Cast<AMonsterAIController>(Character->GetController());
		if (Controller) { Controller->GetPursuitTargetLocation(Target, Location); }
		else if (IsValid(Target)) { Location = Target->GetActorLocation(); }
		return Location;
	}
}

void UMonsterMovementComponent::TickComponent(const float DeltaTime, const ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// CMC 물리 계산과 루트모션 처리가 끝난 뒤에 돈다. 몽타주 중에도 병행 이동이 그 결과에 먹히지 않는다.
	if (bPatternMovementActive)
	{
		TickPatternMovement(DeltaTime);
	}
}

void UMonsterMovementComponent::UpdateMovementTowardTarget(AActor* InTargetActor, const float DeltaSeconds)
{
	// 이동 방식은 비행형/지상형 파생 컴포넌트가 구현한다.
}

void UMonsterMovementComponent::UpdateMovementTowardLocation(const FVector& Destination, const float DeltaSeconds)
{
	// 이동 방식은 비행형/지상형 파생 컴포넌트가 구현한다.
}

void UMonsterMovementComponent::ResetMovementState()
{
	ReleaseChaseSlot();
	StopMovementImmediately();
}

void UMonsterMovementComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	if (ABaseCharacter* Character = Cast<ABaseCharacter>(GetOwner()))
	{
		Character->OnDeathStarted.RemoveAll(this);
	}
	ReleaseChaseSlot();
	Super::EndPlay(Reason);
}

void UMonsterMovementComponent::BeginPlay()
{
	Super::BeginPlay();
	if (ABaseCharacter* Character = Cast<ABaseCharacter>(GetOwner()))
	{
		Character->OnDeathStarted.AddUObject(this, &UMonsterMovementComponent::HandleChaseOwnerDeath);
	}
}

void UMonsterMovementComponent::HandleChaseOwnerDeath(ABaseCharacter* Character)
{
	ReleaseChaseSlot();
}

void UMonsterMovementComponent::ReleaseChaseSlot()
{
	if (UWorld* World = GetWorld())
	{
		if (UMonsterChaseSlotSubsystem* Slots = World->GetSubsystem<UMonsterChaseSlotSubsystem>())
		{
			Slots->Release(GetOwner());
		}
	}
	bWaitingForChaseSlot = false;
	bChaseSlotHolding = false;
	ChaseSlotTarget.Reset();
	NextChaseSlotRetryTime = 0.0;
}

bool UMonsterMovementComponent::ResolveChaseSlotGoal(AActor* Target, bool bFlying, FVector& OutFloorGoal)
{
	if (ChaseSlotTarget.Get() != Target)
	{
		ReleaseChaseSlot();
		ChaseSlotTarget = Target;
	}
	const double Now = GetWorld()->GetTimeSeconds();
	if (Now < NextChaseSlotRetryTime) { return false; }
	UMonsterChaseSlotSubsystem* Slots = GetWorld()->GetSubsystem<UMonsterChaseSlotSubsystem>();
	const bool bAssigned = Slots && Slots->ResolveGoal(GetOwner(), Target, bFlying, ChaseSlotCount,
		ChaseSlotRadius, ChaseWaitingRingSpacing, ChaseWaitingRingCount, ChaseSlotRefreshInterval,
		OutFloorGoal, bWaitingForChaseSlot);
	if (!bAssigned)
	{
		// 대기 자리까지 막히면 타깃 중심으로 몰리지 않고 현재 위치에서 재시도한다.
		bWaitingForChaseSlot = true;
		bChaseSlotHolding = true;
		NextChaseSlotRetryTime = Now + FMath::Max(ChaseSlotRefreshInterval, 0.05f);
		return false;
	}
	const float StopRadius = ChaseSlotAcceptanceRadius * (bChaseSlotHolding ? 2.f : 1.f);
	bChaseSlotHolding = FVector::DistSquared2D(GetOwner()->GetActorLocation(), OutFloorGoal) <= FMath::Square(StopRadius);
	if (!bFlying && bChaseSlotHolding)
	{
		const ACharacter* Character = GetCharacterOwner();
		const float FeetZ = Character->GetActorLocation().Z - Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
		bChaseSlotHolding = FMath::Abs(FeetZ - OutFloorGoal.Z) <= MaxStepHeight + 20.f;
	}
#if ENABLE_DRAW_DEBUG
	if (bDebugDrawChaseSlots)
	{
		const FColor Color = bWaitingForChaseSlot ? FColor::Orange : FColor::Green;
		DrawDebugSphere(GetWorld(), OutFloorGoal + FVector(0, 0, 10.f), ChaseSlotAcceptanceRadius, 12, Color, false, 0.f);
		DrawDebugLine(GetWorld(), GetOwner()->GetActorLocation(), OutFloorGoal, Color, false, 0.f);
	}
#endif
	return true;
}

EEnemyPatternPreparationStatus UMonsterMovementComponent::BeginPatternPreparation(
	const FEnemyPatternDefinition& Pattern,
	const FEnemyPatternMovementOption& MovementOption,
	AActor* TargetActor)
{
	ReleaseChaseSlot();
	// 기본 움직임에서 곧바로 실제 이동이 이어지면 속도를 끊지 않고 이어받는다.
	// 이동이 없는 패턴은 제자리에서 시전해야 하므로 멈춘다.
	const bool bHasMovement = MovementOption.Intent != EEnemyMovementIntent::None
		&& MovementOption.Intent != EEnemyMovementIntent::HoldPosition;
	bPreserveVelocityOnStop = bHasMovement && ShouldPreserveVelocityOnPatternBegin();
	CancelPatternPreparation();
	bPreserveVelocityOnStop = false;
	ActivePatternOption = MovementOption;
	PatternMovementTarget = TargetActor;

	if (!bHasMovement)
	{
		return PreparationStatus = EEnemyPatternPreparationStatus::Ready;
	}

	if (!ResolvePatternMovementContext(MovementOption))
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[PatternMovement] 이 이동 컴포넌트가 처리할 수 없는 패턴 이동입니다. | Owner=%s Component=%s Pattern=%s Intent=%d Target=%s Anchor=%s"),
			*GetNameSafe(GetOwner()), *GetNameSafe(this), *Pattern.PatternId.ToString(),
			static_cast<int32>(MovementOption.Intent), *GetNameSafe(TargetActor), *GetNameSafe(GetPatternMovementAnchor()));
		ActivePatternOption = FEnemyPatternMovementOption();
		PatternMovementTarget = nullptr;
		return PreparationStatus = EEnemyPatternPreparationStatus::Failed;
	}

	PatternElapsedTime = 0.0f;
	ConcurrentPatternVelocity = FVector::ZeroVector;
	PatternSpeedOverride = 0.0f;
	bUseArrivalLoopTiming = false;
	ArrivalTransitionTime = 0.0f;
	NextMoveToAnchorDebugLogTime = 0.0f;
	bPatternMovementActive = true;
	return PreparationStatus = EEnemyPatternPreparationStatus::Preparing;
}

EEnemyPatternPreparationStatus UMonsterMovementComponent::UpdatePatternPreparation(
	const FEnemyPatternDefinition& Pattern,
	const FEnemyPatternMovementOption& MovementOption,
	AActor* TargetActor,
	const float DeltaSeconds)
{
	// 실제 이동은 TickComponent가 돌린다. 여기서는 진행 결과만 알려 준다.
	return PreparationStatus;
}

void UMonsterMovementComponent::CancelPatternPreparation()
{
	StopPatternMovement();
	PatternMovementTarget = nullptr;
	ActivePatternOption = FEnemyPatternMovementOption();
}

bool UMonsterMovementComponent::CanContinuePatternMovementWithoutTarget() const
{
	return bPatternMovementActive && ActivePatternOption.Intent != EEnemyMovementIntent::ApproachTarget;
}

void UMonsterMovementComponent::StopPatternMovement()
{
	const bool bWasActive = bPatternMovementActive;
	const bool bWasMovingDuringAbility = bWasActive && ActivePatternOption.bMoveDuringAbility;
	bPatternMovementActive = false;
	PreparationStatus = EEnemyPatternPreparationStatus::Ready;
	ConcurrentPatternVelocity = FVector::ZeroVector;

	// ArrivalLoop 역산 속도나 파생 클래스의 속도 오버라이드가 다음 이동까지 남지 않게 한다.
	bUseArrivalLoopTiming = false;
	PatternSpeedOverride = 0.0f;
	RestoreBaseMovementStats();

	/*
	 * 병행 이동 종료는 다음 몽타주 루트모션의 Velocity를 지우지 않는다.
	 * 기본 움직임에서 실제 이동으로 넘어가는 경우도 속도를 이어받도록 지우지 않는다.
	 * 일반 몬스터는 진행 중인 패턴 이동이 없었다면 컨트롤러 추격 속도를 건드리지 않는다.
	 */
	if (!bWasMovingDuringAbility && !bPreserveVelocityOnStop && (bWasActive || ShouldAlwaysHaltOnPatternStop()))
	{
		StopMovementImmediately();
	}

	OnPatternMovementStopped();

	// 상태 정리가 모두 끝난 뒤 알린다. 받는 쪽이 여기서 곧바로 다음 이동을 시작해도 된다.
	if (bWasActive)
	{
		const bool bArrived = bStoppingOnArrival;
		bStoppingOnArrival = false;
		OnPatternMovementEnded.Broadcast(bArrived);
	}
}

bool UMonsterMovementComponent::ConfigureArrivalLoopTiming(const UAnimMontage* Montage, const float MontagePlayRate)
{
	if (!bPatternMovementActive || !ActivePatternOption.bMoveDuringAbility ||
		ActivePatternOption.ArrivalJumpSectionName.IsNone() || ActivePatternOption.ArrivalLoopSectionName.IsNone() ||
		ActivePatternOption.ArrivalLoopCount < 1 || !IsValid(Montage))
	{
		return false;
	}

	const int32 LoopSectionIndex = Montage->GetSectionIndex(ActivePatternOption.ArrivalLoopSectionName);
	const float SafePlayRate = FMath::Max(MontagePlayRate, KINDA_SMALL_NUMBER);
	const float LoopDuration = LoopSectionIndex != INDEX_NONE
		? Montage->GetSectionLength(LoopSectionIndex) / SafePlayRate
		: 0.0f;
	if (LoopDuration <= KINDA_SMALL_NUMBER)
	{
		return false;
	}

	const ACharacter* Character = GetCharacterOwner();
	if (!IsValid(Character))
	{
		return false;
	}

	const FVector Location = Character->GetActorLocation();
	float PlannedDistance = 0.0f;
	if (ActivePatternOption.Intent == EEnemyMovementIntent::ApproachTarget)
	{
		PlannedDistance = IsValid(PatternMovementTarget)
			? FMath::Max(FVector::Dist2D(Location, ObservedPatternTargetLocation(Character, PatternMovementTarget.Get())) - ApproachStopDistance, 0.0f)
			: 0.0f;
	}
	else
	{
		const FVector PlannedDestination(PatternDestination.X, PatternDestination.Y,
			UsesVerticalPatternMovement() ? PatternTargetWorldZ : Location.Z);
		PlannedDistance = FVector::Dist(Location, PlannedDestination);
	}

	const float PlannedDuration = LoopDuration * ActivePatternOption.ArrivalLoopCount;
	const float RequiredSpeed = PlannedDistance / PlannedDuration;
	if (RequiredSpeed > 0.0f)
	{
		PatternSpeedOverride = RequiredSpeed;
	}

	bUseArrivalLoopTiming = true;
	ArrivalTransitionTime = PatternElapsedTime + PlannedDuration;
	return true;
}

bool UMonsterMovementComponent::ResolvePatternMovementContext(const FEnemyPatternMovementOption& MovementOption)
{
	const ACharacter* Character = GetCharacterOwner();
	if (!IsValid(Character))
	{
		return false;
	}

	// 높이를 쓰지 않는 방식은 현재 높이를 목표로 둔다. 높이를 쓰는 파생 클래스가 덮어쓴다.
	PatternTargetWorldZ = Character->GetActorLocation().Z;

	switch (MovementOption.Intent)
	{
	case EEnemyMovementIntent::ApproachTarget:
	{
		if (!IsValid(PatternMovementTarget))
		{
			return false;
		}

		const float MinDistance = FMath::Max(MovementOption.MinTargetDistance, 0.0f);
		const float MaxDistance = FMath::Max(MovementOption.MaxTargetDistance, MinDistance);
		ApproachStopDistance = FMath::FRandRange(MinDistance, MaxDistance);
		ApproachOffsetAngleDegrees = 0.0f;
		if (!MovementOption.OffsetAngleChoices.IsEmpty())
		{
			ApproachOffsetAngleDegrees = MovementOption.OffsetAngleChoices[
				FMath::RandRange(0, MovementOption.OffsetAngleChoices.Num() - 1)]
				* (FMath::RandBool() ? 1.0f : -1.0f);
		}
		return true;
	}
	case EEnemyMovementIntent::MoveToAnchor:
	{
		const AActor* Anchor = GetPatternMovementAnchor();
		if (!IsValid(Anchor))
		{
			return false;
		}

		PatternDestination = Anchor->GetActorLocation();
		return true;
	}
	default:
		return false;
	}
}

float UMonsterMovementComponent::GetPatternFloorZ() const
{
	const ACharacter* Character = GetCharacterOwner();
	return IsValid(Character) ? Character->GetActorLocation().Z : 0.0f;
}

void UMonsterMovementComponent::TickPatternMovement(const float DeltaTime)
{
	ACharacter* Character = GetCharacterOwner();
	if (!IsValid(Character))
	{
		StopPatternMovement();
		return;
	}

	PatternElapsedTime += DeltaTime;

	switch (EvaluatePatternMovementProgress())
	{
	case EMonsterPatternMovementProgress::Hold:
		return;
	case EMonsterPatternMovementProgress::Finished:
		StopPatternMovement();
		return;
	case EMonsterPatternMovementProgress::Arrived:
		HandlePatternMovementArrived();
		return;
	default:
		break;
	}

	// 공전 외 이동의 MaxDuration은 끼임 방지용 탈출 장치다. 0이면 사용하지 않는다.
	if (!bUseArrivalLoopTiming && ActivePatternOption.Intent != EEnemyMovementIntent::OrbitAnchor &&
		ActivePatternOption.MaxDuration > 0.0f && PatternElapsedTime >= ActivePatternOption.MaxDuration)
	{
		StopPatternMovement();
		return;
	}

	const FVector DesiredDirection = GetPatternMovementDirection();
	if (ActivePatternOption.Intent == EEnemyMovementIntent::MoveToAnchor && PatternElapsedTime >= NextMoveToAnchorDebugLogTime)
	{
		NextMoveToAnchorDebugLogTime = PatternElapsedTime + 0.25f;
		UE_LOG(LogTemp, Verbose,
			TEXT("[PatternMoveToAnchor] Tick | Owner=%s Location=%s Destination=%s Horizontal=%.1f Vertical=%.1f Direction=%s Velocity=%s"),
			*GetNameSafe(Character), *Character->GetActorLocation().ToCompactString(), *PatternDestination.ToCompactString(),
			FVector::Dist2D(Character->GetActorLocation(), PatternDestination),
			PatternTargetWorldZ - Character->GetActorLocation().Z, *DesiredDirection.ToCompactString(),
			*ConcurrentPatternVelocity.ToCompactString());
	}

	ApplyPatternMovement(DesiredDirection, DeltaTime);
	FacePatternMovementTarget(DeltaTime);
}

EMonsterPatternMovementProgress UMonsterMovementComponent::EvaluatePatternMovementProgress() const
{
	const ACharacter* Character = GetCharacterOwner();
	const FVector Location = Character->GetActorLocation();
	const bool bVertical = UsesVerticalPatternMovement();
	const float VerticalDistance = bVertical ? FMath::Abs(Location.Z - PatternTargetWorldZ) : 0.0f;
	const bool bHeightReached = VerticalDistance <= GetPatternArrivalHeightTolerance();

	bool bArrived = false;
	if (ActivePatternOption.Intent == EEnemyMovementIntent::ApproachTarget)
	{
		if (!IsValid(PatternMovementTarget))
		{
			return EMonsterPatternMovementProgress::Finished;
		}

		/*
		 * 접근은 정지 거리 안에 들어오면 끝난다. 높이는 도착 조건으로 보지 않는다.
		 * 비행 중인 타깃 쪽으로 가다가 높이 오차 때문에 영영 멈추지 않는 일을 막는다.
		 */
		bArrived = FVector::Dist2D(Location, ObservedPatternTargetLocation(Character, PatternMovementTarget.Get())) <= ApproachStopDistance;
	}
	else
	{
		const bool bHeightOnly = ActivePatternOption.Intent == EEnemyMovementIntent::MatchHeight ||
			ActivePatternOption.Intent == EEnemyMovementIntent::DescendToFloor;
		const UEnemyMovementProfileDataAsset* Profile = GetActiveMovementProfile();
		const float ArrivalRadius = Profile ? Profile->ArrivalRadius : 100.0f;
		bArrived = (bHeightOnly || FVector::Dist2D(Location, PatternDestination) <= ArrivalRadius) && bHeightReached;
	}

	if (!bArrived)
	{
		return EMonsterPatternMovementProgress::Continue;
	}

	return bUseArrivalLoopTiming && PatternElapsedTime < ArrivalTransitionTime
		? EMonsterPatternMovementProgress::Hold
		: EMonsterPatternMovementProgress::Arrived;
}

void UMonsterMovementComponent::HandlePatternMovementArrived()
{
	const FName ArrivalJumpSectionName = ActivePatternOption.ArrivalJumpSectionName;
	const FName ArrivalLoopSectionName = ActivePatternOption.ArrivalLoopSectionName;
	const bool bBroadcastArrival = ActivePatternOption.bMoveDuringAbility && !ArrivalJumpSectionName.IsNone();
	bStoppingOnArrival = true;
	StopPatternMovement();
	if (bBroadcastArrival)
	{
		OnPatternMovementArrived.Broadcast(ArrivalLoopSectionName, ArrivalJumpSectionName);
	}
}

FVector UMonsterMovementComponent::GetPatternMovementDirection() const
{
	const ACharacter* Character = GetCharacterOwner();
	if (!IsValid(Character))
	{
		return FVector::ZeroVector;
	}

	// 기본은 수평 방향만 낸다. 고도를 쓰는 방식은 파생 클래스가 수직 성분을 더한다.
	const FVector Location = Character->GetActorLocation();
	if (ActivePatternOption.Intent == EEnemyMovementIntent::ApproachTarget)
	{
		const AActor* Target = PatternMovementTarget.Get();
		const FVector ToTarget = IsValid(Target)
			? ObservedPatternTargetLocation(Character, Target) - Location
			: Character->GetActorForwardVector();
		return ToTarget.GetSafeNormal2D().RotateAngleAxis(ApproachOffsetAngleDegrees, FVector::UpVector);
	}

	return (PatternDestination - Location).GetSafeNormal2D();
}

void UMonsterMovementComponent::ApplyPatternMovement(const FVector& Direction, const float DeltaTime)
{
	ACharacter* Character = GetCharacterOwner();
	if (!IsValid(Character) || Direction.IsNearlyZero())
	{
		return;
	}

	if (!ActivePatternOption.bMoveDuringAbility)
	{
		Character->AddMovementInput(Direction, 1.0f);
		return;
	}

	/*
	 * TickComponent의 Super 호출로 CMC/루트모션 처리가 끝난 뒤 실행된다.
	 * 따라서 이 경로는 몽타주 중 AddMovementInput이 Velocity로 해석되지 않는 문제를 우회한다.
	 */
	const float Speed = PatternSpeedOverride > 0.0f ? PatternSpeedOverride : GetMaxSpeed();
	const FVector TargetVelocity = Direction.GetSafeNormal() * Speed;
	ConcurrentPatternVelocity = FMath::VInterpConstantTo(
		ConcurrentPatternVelocity, TargetVelocity, DeltaTime, MaxAcceleration);
	const FVector DeltaLocation = ConcurrentPatternVelocity * DeltaTime;
	if (DeltaLocation.IsNearlyZero())
	{
		return;
	}

	FHitResult Hit;
	if (!ShouldSweepPatternMovement())
	{
		// 이동 Sweep만 끈다. Capsule/Mesh의 Query Collision은 바꾸지 않으므로 피격 판정은 유지된다.
		MoveUpdatedComponent(DeltaLocation, UpdatedComponent->GetComponentQuat(), false, &Hit);
		return;
	}

	SafeMoveUpdatedComponent(DeltaLocation, UpdatedComponent->GetComponentQuat(), true, Hit);
	if (Hit.IsValidBlockingHit())
	{
		if (ActivePatternOption.Intent == EEnemyMovementIntent::MoveToAnchor)
		{
			UE_LOG(LogTemp, Warning,
				TEXT("[PatternMoveToAnchor] BlockingHit | Owner=%s HitActor=%s Time=%.3f Normal=%s Requested=%s"),
				*GetNameSafe(Character), *GetNameSafe(Hit.GetActor()), Hit.Time,
				*Hit.Normal.ToCompactString(), *DeltaLocation.ToCompactString());
		}
		SlideAlongSurface(DeltaLocation, 1.0f - Hit.Time, Hit.Normal, Hit, true);
	}
}

float UMonsterMovementComponent::GetPatternArrivalHeightTolerance() const
{
	const UEnemyMovementProfileDataAsset* Profile = GetActiveMovementProfile();
	return Profile ? Profile->Flying.ArrivalHeightTolerance : 75.0f;
}

void UMonsterMovementComponent::FacePatternMovementTarget(const float DeltaTime)
{
	ACharacter* Character = GetCharacterOwner();
	const AActor* FacingTarget = GetPatternFacingTarget();
	if (!IsValid(Character) || !IsValid(FacingTarget))
	{
		return;
	}

	FVector ToTarget = ObservedPatternTargetLocation(Character, FacingTarget) - Character->GetActorLocation();
	ToTarget.Z = 0.0f;
	if (!ToTarget.IsNearlyZero())
	{
		const UEnemyMovementProfileDataAsset* Profile = GetActiveMovementProfile();
		const float TurnSpeed = Profile ? Profile->FacingTurnSpeed : 3.0f;
		Character->SetActorRotation(FMath::RInterpTo(Character->GetActorRotation(), ToTarget.Rotation(), DeltaTime, TurnSpeed));
	}
}

void UMonsterMovementComponent::HandleMovementProfileChanged(const UEnemyMovementProfileDataAsset* Profile)
{
	/*
	 * 기본값은 BeginPlay가 아니라 여기서 저장한다. 캐릭터는 스탯 초기화를 끝낸 뒤 프로필을 넘기므로,
	 * 이 시점의 값이 "프로필이 비어 있을 때 써야 할 값"이다.
	 */
	if (!bHasComponentDefaults)
	{
		ComponentDefaultSpeed = GetProfileControlledSpeed();
		ComponentDefaultMaxAcceleration = MaxAcceleration;
		bHasComponentDefaults = true;
	}

	BaseSpeed = IsValid(Profile) && Profile->MoveSpeed > 0.0f
		? Profile->MoveSpeed
		: ComponentDefaultSpeed;
	BaseMaxAcceleration = IsValid(Profile) && Profile->Acceleration > 0.0f
		? Profile->Acceleration
		: ComponentDefaultMaxAcceleration;
	bHasBaseMovementStats = true;

	if (IsValid(Profile))
	{
		if (Profile->MovementType != GetExpectedMovementType())
		{
			UE_LOG(LogTemp, Warning,
				TEXT("[MonsterMovement] 프로필의 이동 방식이 이동 컴포넌트와 맞지 않습니다. | Owner=%s Component=%s Profile=%s Type=%d Expected=%d"),
				*GetNameSafe(GetOwner()), *GetNameSafe(this), *GetNameSafe(Profile),
				static_cast<int32>(Profile->MovementType), static_cast<int32>(GetExpectedMovementType()));
		}

		if (Profile->MoveSpeed > 0.0f && GetProfileControlledSpeed() < 0.0f)
		{
			UE_LOG(LogTemp, Warning,
				TEXT("[MonsterMovement] 이 이동 방식은 프로필 MoveSpeed 를 쓰지 않습니다. 속도는 MovementSpeed 어트리뷰트가 정합니다. | Owner=%s Profile=%s"),
				*GetNameSafe(GetOwner()), *GetNameSafe(Profile));
		}
	}

	// 진행 중인 패턴 이동이 속도를 임시로 바꿨을 수 있다. 그 이동이 끝날 때 새 기본값으로 되돌린다.
	if (CanRestoreBaseMovementStatsNow())
	{
		RestoreBaseMovementStats();
	}
}

void UMonsterMovementComponent::RestoreBaseMovementStats()
{
	if (!bHasBaseMovementStats)
	{
		return;
	}

	if (BaseSpeed >= 0.0f)
	{
		SetProfileControlledSpeed(BaseSpeed);
	}
	MaxAcceleration = BaseMaxAcceleration;
}

const UEnemyMovementProfileDataAsset* UMonsterMovementComponent::GetActiveMovementProfile() const
{
	const ACommonEnemyCharacter* Character = Cast<ACommonEnemyCharacter>(GetCharacterOwner());
	return IsValid(Character) ? Character->GetActiveMovementProfile() : nullptr;
}
