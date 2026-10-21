#include "BossMovementComponent.h"

#include "GameCore/Character/BossMonsterCharacter.h"
#include "GameCore/Enemy/Data/EnemyDataAssets.h"
#include "GameFramework/Character.h"

UBossMovementComponent::UBossMovementComponent()
{
	// 보스는 조우·피격 전부터 공중 이동체다. BeginPlay 전에도 Walking으로 생성되지 않게 한다.
	DefaultLandMovementMode = MOVE_Flying;
}

void UBossMovementComponent::BeginPlay()
{
	Super::BeginPlay();
	SetMovementMode(MOVE_Flying);
}

void UBossMovementComponent::TickComponent(const float DeltaTime, const ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	/*
	 * 바라보기는 패턴 이동 틱 안에서만 돈다. 제자리 전투 페이즈는 이동이 없어 그 틱이 돌지 않으므로,
	 * 아레나 중심을 보는 페이즈는 여기서 방향을 유지한다. 이동 틱이 돌 때는 그쪽이 이미 돌린다
	 */
	if (bPatternMovementActive)
	{
		return;
	}

	const ABossMonsterCharacter* Boss = Cast<ABossMonsterCharacter>(GetCharacterOwner());
	if (!IsValid(Boss) || !Boss->HasAuthority() || Boss->HasStartedDeath() || !Boss->ShouldFaceArenaCenter())
	{
		return;
	}

	RotateTowardAnchor(DeltaTime, false);
}

void UBossMovementComponent::RotateTowardAnchor(const float DeltaTime, const bool bSnap)
{
	ACharacter* Character = GetCharacterOwner();
	const AActor* Anchor = GetPatternMovementAnchor();
	if (!IsValid(Character) || !IsValid(Anchor))
	{
		return;
	}

	FVector ToAnchor = Anchor->GetActorLocation() - Character->GetActorLocation();
	ToAnchor.Z = 0.0f;
	if (ToAnchor.IsNearlyZero())
	{
		return;
	}

	const FRotator Desired = ToAnchor.Rotation();
	const UEnemyMovementProfileDataAsset* Profile = GetActiveMovementProfile();
	const float TurnSpeed = Profile ? Profile->FacingTurnSpeed : 3.0f;
	Character->SetActorRotation(bSnap ? Desired : FMath::RInterpTo(Character->GetActorRotation(), Desired, DeltaTime, TurnSpeed));
}

bool UBossMovementComponent::BeginIdleMovement(AActor* InTargetActor)
{
	/*
	 * 제자리 전투 페이즈는 기본 움직임이 없다. false를 돌려주면 Idle Movement 태스크가
	 * 기다리지 않고 곧바로 다음 패턴 선택으로 넘어간다(IEnemyPatternMovement 계약).
	 */
	if (bStationary)
	{
		return false;
	}

	// 공전 계산과 이동 적용은 패턴 이동과 같은 경로를 쓴다. 차이는 스스로 끝나지 않는다는 점뿐이다.
	FEnemyPatternMovementOption IdleOption;
	IdleOption.Intent = EEnemyMovementIntent::OrbitAnchor;
	IdleOption.bMoveDuringAbility = false;

	if (BeginPatternPreparation(FEnemyPatternDefinition(), IdleOption, InTargetActor)
		== EEnemyPatternPreparationStatus::Failed)
	{
		return false;
	}

	bIdleMovementActive = true;
	return true;
}

EEnemyPatternPreparationStatus UBossMovementComponent::BeginPatternPreparation(
	const FEnemyPatternDefinition& Pattern,
	const FEnemyPatternMovementOption& MovementOption,
	AActor* InTargetActor)
{
	if (!bStationary)
	{
		return Super::BeginPatternPreparation(Pattern, MovementOption, InTargetActor);
	}

	/*
	 * 제자리 전투 중에는 패턴 데이터에 이동 후보가 있어도 움직이지 않는다.
	 * 이동 없는 패턴과 같은 경로(Hold Position)를 타므로 남은 속도도 정리되고 곧바로 Ready가 된다.
	 */
	FEnemyPatternMovementOption HoldOption;
	HoldOption.Intent = EEnemyMovementIntent::HoldPosition;
	return Super::BeginPatternPreparation(Pattern, HoldOption, InTargetActor);
}

bool UBossMovementComponent::CanFinishIdleMovement() const
{
	return !bIdleMovementActive || PatternElapsedTime >= OrbitDuration;
}

void UBossMovementComponent::EndIdleMovement()
{
	if (!bIdleMovementActive)
	{
		return;
	}

	/*
	 * StopPatternMovement를 부르지 않는다. 그 경로는 속도를 0으로 급정지시켜
	 * 공전하던 보스가 다음 이동 직전에 멈칫하게 만든다.
	 */
	bIdleMovementActive = false;
	bPatternMovementActive = false;
	PreparationStatus = EEnemyPatternPreparationStatus::Ready;
	PatternMovementTarget = nullptr;
	ActivePatternOption = FEnemyPatternMovementOption();
	RestoreBaseMovementStats();
	bIdleHandoffPending = true;
}

bool UBossMovementComponent::TeleportToArenaAnchorOffset(const FVector& AnchorOffset)
{
	ACharacter* Character = GetCharacterOwner();
	const AActor* Anchor = GetPatternMovementAnchor();
	if (!IsValid(Character) || !IsValid(Anchor)) return false;

	CancelPatternPreparation();

	// 도착할 자리에서 앵커를 향하는 방향으로 돌려서 보낸다. 앵커 바로 아래처럼 수평 차이가 없으면 지금 회전을 유지한다
	FRotator Rotation = Character->GetActorRotation();
	FVector ToAnchor = -AnchorOffset;
	ToAnchor.Z = 0.0f;
	if (!ToAnchor.IsNearlyZero())
	{
		Rotation = ToAnchor.Rotation();
	}
	return Character->TeleportTo(Anchor->GetActorLocation() + AnchorOffset, Rotation, false, true);
}

bool UBossMovementComponent::BeginTransitionMoveToAnchorOffset(const FVector& TargetAnchorOffset,
	const float SpeedOverride, const float AccelerationOverride, const float AcceptanceTolerance,
	const float DurationOverride)
{
	ACharacter* Character = GetCharacterOwner();
	const AActor* Anchor = GetPatternMovementAnchor();
	if (!IsValid(Character) || !IsValid(Anchor)) return false;

	CancelPatternPreparation();
	ActivePatternOption = FEnemyPatternMovementOption();
	ActivePatternOption.Intent = EEnemyMovementIntent::MoveToAnchor;
	ActivePatternOption.bMoveDuringAbility = true;
	ActivePatternOption.MaxDuration = 0.0f;
	PatternDestination = Anchor->GetActorLocation() + TargetAnchorOffset;
	PatternTargetWorldZ = PatternDestination.Z;

	// 속도 오버라이드는 이동이 끝날 때 부모의 StopPatternMovement가 프로필 값으로 되돌린다.
	if (SpeedOverride > 0.0f) MaxFlySpeed = SpeedOverride;
	if (AccelerationOverride > 0.0f) MaxAcceleration = AccelerationOverride;
	TransitionRiseAcceptanceTolerance = FMath::Max(AcceptanceTolerance, 0.0f);
	PatternElapsedTime = 0.0f;
	ConcurrentPatternVelocity = FVector::ZeroVector;
	bUseTransitionMoveDuration = DurationOverride > KINDA_SMALL_NUMBER;
	TransitionMoveDuration = bUseTransitionMoveDuration ? DurationOverride : 0.0f;
	TransitionMoveStartLocation = Character->GetActorLocation();
	bTransitionRiseActive = true;
	bPatternMovementActive = true;
	PreparationStatus = EEnemyPatternPreparationStatus::Preparing;
	return true;
}

bool UBossMovementComponent::ResolvePatternMovementContext(const FEnemyPatternMovementOption& MovementOption)
{
	if (MovementOption.Intent != EEnemyMovementIntent::OrbitAnchor)
	{
		return Super::ResolvePatternMovementContext(MovementOption);
	}

	const UEnemyMovementProfileDataAsset* Profile = GetActiveMovementProfile();
	const AActor* Anchor = GetPatternMovementAnchor();
	if (!IsValid(GetCharacterOwner()) || !IsValid(Profile) || !IsValid(Anchor)) return false;

	const FEnemyOrbitMovementSettings& OrbitSettings = Profile->Orbit;
	// 공전을 시작할 때마다 반지름을 새로 뽑아 매번 다른 궤도를 돈다.
	const float MinRadius = FMath::Max(OrbitSettings.RadiusMin, 0.0f);
	const float PickedRadius = FMath::FRandRange(MinRadius, FMath::Max(MinRadius, OrbitSettings.RadiusMax));
	if (PickedRadius <= 0.0f) return false;
	OrbitCenter = Anchor->GetActorLocation();
	OrbitRadius = PickedRadius;
	// 직전 방향을 기억해 두고 확률로만 뒤집는다. 처음 공전이면 반반으로 정한다.
	if (FMath::IsNearlyZero(LastOrbitDirection))
	{
		OrbitDirection = FMath::RandBool() ? 1.0f : -1.0f;
	}
	else
	{
		const bool bReverse = FMath::FRand() < OrbitSettings.ReverseDirectionChance;
		OrbitDirection = bReverse ? -LastOrbitDirection : LastOrbitDirection;
	}
	LastOrbitDirection = OrbitDirection;
	const float MinDuration = FMath::Max(OrbitSettings.CruiseDurationMin, 0.0f);
	OrbitDuration = FMath::FRandRange(MinDuration, FMath::Max(MinDuration, OrbitSettings.CruiseDurationMax));
	// 높이도 공전을 시작할 때마다 새로 뽑는다. 패턴 옵션에 높이가 있으면 그 값을 우선한다.
	const float MinHeight = FMath::Max(OrbitSettings.CruiseHeightFromFloorMin, 0.0f);
	const float PickedHeight = FMath::FRandRange(MinHeight, FMath::Max(MinHeight, OrbitSettings.CruiseHeightFromFloorMax));
	PatternTargetWorldZ = GetPatternFloorZ() + (MovementOption.HeightFromFloor >= 0.0f
		? MovementOption.HeightFromFloor
		: PickedHeight);
	return true;
}

AActor* UBossMovementComponent::GetPatternMovementAnchor() const
{
	const ABossMonsterCharacter* Boss = Cast<ABossMonsterCharacter>(GetCharacterOwner());
	return IsValid(Boss) ? Boss->GetArenaCenter() : nullptr;
}

float UBossMovementComponent::GetPatternFloorZ() const
{
	if (const AActor* Anchor = GetPatternMovementAnchor())
	{
		return Anchor->GetActorLocation().Z;
	}
	return Super::GetPatternFloorZ();
}

void UBossMovementComponent::TickPatternMovement(const float DeltaTime)
{
	// Duration이 지정된 전환 상승만 시간 비율로 목표 위치에 정확히 보간한다. 나머지는 공용 이동이다.
	if (!bTransitionRiseActive || !bUseTransitionMoveDuration)
	{
		Super::TickPatternMovement(DeltaTime);
		return;
	}

	ACharacter* Character = GetCharacterOwner();
	if (!IsValid(Character))
	{
		StopPatternMovement();
		return;
	}

	PatternElapsedTime += DeltaTime;
	const float Alpha = FMath::Clamp(PatternElapsedTime / TransitionMoveDuration, 0.0f, 1.0f);
	const FVector TimedLocation = FMath::Lerp(TransitionMoveStartLocation, PatternDestination, Alpha);
	const FVector DeltaLocation = TimedLocation - Character->GetActorLocation();
	if (!DeltaLocation.IsNearlyZero())
	{
		FHitResult Hit;
		MoveUpdatedComponent(DeltaLocation, UpdatedComponent->GetComponentQuat(), false, &Hit);
	}

	// 위치를 옮기는 동안 아레나 중심을 향해 돌린다. 이 시점은 아직 이전 페이즈라 Facing Mode를 보지 않는다
	RotateTowardAnchor(DeltaTime, false);

	if (Alpha >= 1.0f)
	{
		// 누적 오차 없이 지정된 목표 위치에서 전환 완료를 알린다.
		// 보간으로 못 다 돈 오차를 여기서 지워, 이후 페이즈가 정확히 중심을 향한 채로 시작하게 한다
		RotateTowardAnchor(DeltaTime, true);

		const FVector FinalDelta = PatternDestination - Character->GetActorLocation();
		if (!FinalDelta.IsNearlyZero())
		{
			FHitResult Hit;
			MoveUpdatedComponent(FinalDelta, UpdatedComponent->GetComponentQuat(), false, &Hit);
		}

		StopPatternMovement();
		OnTransitionRiseCompleted.Broadcast();
		OnTransitionMoveCompleted.Broadcast();
	}
}

EMonsterPatternMovementProgress UBossMovementComponent::EvaluatePatternMovementProgress() const
{
	if (ActivePatternOption.Intent != EEnemyMovementIntent::OrbitAnchor)
	{
		return Super::EvaluatePatternMovementProgress();
	}

	// 공전은 도착 개념이 없어 시간으로만 끝난다. 시간이 0이어도 멈춰 서지 않고 바로 끝낸다.
	// 기본 움직임은 시간이 지나도 계속 돌고, 끝낼 시점은 호출한 쪽이 정한다.
	return !bIdleMovementActive && PatternElapsedTime >= OrbitDuration
		? EMonsterPatternMovementProgress::Finished
		: EMonsterPatternMovementProgress::Continue;
}

void UBossMovementComponent::HandlePatternMovementArrived()
{
	if (!bTransitionRiseActive)
	{
		Super::HandlePatternMovementArrived();
		return;
	}

	StopPatternMovement();
	OnTransitionRiseCompleted.Broadcast();
	OnTransitionMoveCompleted.Broadcast();
}

FVector UBossMovementComponent::GetPatternMovementDirection() const
{
	const ACharacter* Character = GetCharacterOwner();
	if (!IsValid(Character) || ActivePatternOption.Intent != EEnemyMovementIntent::OrbitAnchor)
	{
		return Super::GetPatternMovementDirection();
	}

	const FVector Location = Character->GetActorLocation();
	const UEnemyMovementProfileDataAsset* Profile = GetActiveMovementProfile();
	const FEnemyOrbitMovementSettings OrbitSettings = Profile ? Profile->Orbit : FEnemyOrbitMovementSettings();
	FVector Radial = Location - OrbitCenter;
	Radial.Z = 0.0f;
	const float Distance = Radial.Size();
	Radial = Distance > KINDA_SMALL_NUMBER ? Radial / Distance : Character->GetActorForwardVector().GetSafeNormal2D();
	const FVector Tangent = FVector::CrossProduct(FVector::UpVector, Radial) * OrbitDirection;
	const float DesiredRadius = OrbitRadius + FMath::Sin(PatternElapsedTime * OrbitSettings.SwayFrequency * UE_TWO_PI) * OrbitSettings.SwayAmplitude;
	const float MaxRadial = FMath::Tan(FMath::DegreesToRadians(OrbitSettings.MaxApproachAngle));
	const FVector Horizontal = (Tangent + Radial * FMath::Clamp((DesiredRadius - Distance) * OrbitSettings.DistanceCorrectionSpeed / FMath::Max(MaxFlySpeed, 1.0f), -MaxRadial, MaxRadial)).GetSafeNormal();
	const float HoverZ = PatternTargetWorldZ + FMath::Sin(PatternElapsedTime * OrbitSettings.HoverFrequency * UE_TWO_PI) * OrbitSettings.HoverAmplitude;
	return (Horizontal + FVector::UpVector * FMath::Clamp((HoverZ - Location.Z) / FMath::Max(VerticalCorrectionRange, 1.0f), -1.0f, 1.0f)).GetClampedToMaxSize(1.0f);
}

float UBossMovementComponent::GetPatternArrivalHeightTolerance() const
{
	return bTransitionRiseActive ? TransitionRiseAcceptanceTolerance : Super::GetPatternArrivalHeightTolerance();
}

const AActor* UBossMovementComponent::GetPatternFacingTarget() const
{
	const ABossMonsterCharacter* Boss = Cast<ABossMonsterCharacter>(GetCharacterOwner());
	if (IsValid(Boss) && Boss->ShouldFaceArenaCenter())
	{
		return Boss->GetArenaCenter();
	}
	return Super::GetPatternFacingTarget();
}

void UBossMovementComponent::OnPatternMovementStopped()
{
	Super::OnPatternMovementStopped();
	bIdleMovementActive = false;
	bTransitionRiseActive = false;
	bUseTransitionMoveDuration = false;
	TransitionMoveDuration = 0.0f;
	bIdleHandoffPending = false;
}
