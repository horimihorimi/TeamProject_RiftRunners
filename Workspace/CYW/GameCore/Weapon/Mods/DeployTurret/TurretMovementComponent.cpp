#include "TurretMovementComponent.h"

#include "GameFramework/Actor.h"
#include "Net/UnrealNetwork.h"

UTurretMovementComponent::UTurretMovementComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UTurretMovementComponent::Initialize()
{
	RandomOffset = FVector(FMath::FRandRange(-FollowOffsetRandomRange, FollowOffsetRandomRange),
		FMath::FRandRange(-FollowOffsetRandomRange, FollowOffsetRandomRange),
		FMath::FRandRange(-FollowOffsetRandomRange, FollowOffsetRandomRange));
	RandomizedMoveInterpSpeed = MoveInterpSpeed + FMath::FRandRange(0.0f, MoveInterpRandModifier);
	RandomizedRotateInterpSpeed = RotateInterpSpeed + FMath::FRandRange(0.0f, RotateInterpRandModifier);
	HoverTime = FMath::FRandRange(0.0f, 100.0f);
	BaseLocation = GetFollowLocation();
	GetOwner()->SetActorLocation(BaseLocation);
	FlightTransform = GetOwner()->GetActorTransform();
}

FVector UTurretMovementComponent::GetFollowLocation() const
{
	const AActor* Player = GetOwner()->GetOwner();
	return Player->GetActorLocation() + Player->GetActorQuat().RotateVector(FollowOffset) + RandomOffset;
}

void UTurretMovementComponent::FollowPlayer(float DeltaTime)
{
	BaseLocation = FMath::VInterpTo(BaseLocation, GetFollowLocation(), DeltaTime, RandomizedMoveInterpSpeed);
	AActor* Turret = GetOwner();
	Turret->SetActorRotation(FMath::RInterpTo(Turret->GetActorRotation(), Turret->GetOwner()->GetActorRotation(),
		DeltaTime, RandomizedRotateInterpSpeed));
}

void UTurretMovementComponent::ChooseNextPosition(const AActor* Target)
{
	const FVector RandomDirection = FMath::VRand();
	InterceptTargetOffset = FVector(RandomDirection.X, RandomDirection.Y, 0.0f).GetSafeNormal() * InterceptRadius
		+ FVector(0.0f, 0.0f, FMath::FRandRange(InterceptMinHeightOffset, InterceptMaxHeightOffset));
	// Freeze each move's destination so a moving target cannot prevent arrival indefinitely.
	InterceptDestination = Target->GetActorLocation() + InterceptTargetOffset;
}

void UTurretMovementComponent::StartIntercept(const AActor* Target)
{
	ChooseNextPosition(Target);
	InterceptStartLocation = BaseLocation;
	InterceptAlpha = 0.0f;
	const float CurveScale = FMath::Clamp(FVector::Dist(InterceptStartLocation, InterceptDestination) / 1200.0f, 0.2f, 1.0f);
	const float CurveStrength = InterceptCurveStrength * CurveScale;
	const FVector RandomSide = GetOwner()->GetActorRightVector() * FMath::FRandRange(-1.0f, 1.0f);
	const FVector RandomForward = GetOwner()->GetActorForwardVector() * FMath::FRandRange(-0.5f, 0.5f);
	InterceptControlOffset = (RandomSide + RandomForward).GetSafeNormal() * CurveStrength + FVector::UpVector * CurveStrength;
}

bool UTurretMovementComponent::MoveToIntercept(float DeltaTime)
{
	const float Distance = FMath::Max(FVector::Dist(InterceptStartLocation, InterceptDestination), 1.0f);
	InterceptAlpha = FMath::Min(InterceptAlpha + DeltaTime * InterceptMoveSpeed / Distance, 1.0f);
	const FVector ControlPoint = (InterceptStartLocation + InterceptDestination) * 0.5f + InterceptControlOffset;
	const FVector A = FMath::Lerp(InterceptStartLocation, ControlPoint, InterceptAlpha);
	const FVector B = FMath::Lerp(ControlPoint, InterceptDestination, InterceptAlpha);
	BaseLocation = FMath::Lerp(A, B, InterceptAlpha);
	const FVector Direction = BaseLocation - GetOwner()->GetActorLocation();
	if (!Direction.IsNearlyZero())
	{
		GetOwner()->SetActorRotation(FMath::RInterpTo(GetOwner()->GetActorRotation(), Direction.Rotation(),
			DeltaTime, RandomizedRotateInterpSpeed));
	}
	return InterceptAlpha >= 1.0f;
}

bool UTurretMovementComponent::FollowTarget(const AActor* Target, float DeltaTime, bool bRepositioning)
{
	const FVector Destination = bRepositioning ? InterceptDestination : Target->GetActorLocation() + InterceptTargetOffset;
	BaseLocation = FMath::VInterpTo(BaseLocation, Destination, DeltaTime, InterceptPositionMoveSpeed);
	RotateToTarget(Target, DeltaTime);
	return FVector::DistSquared(BaseLocation, Destination) <= FMath::Square(InterceptPositionArriveDistance);
}

void UTurretMovementComponent::RotateToTarget(const AActor* Target, float DeltaTime)
{
	const FVector Direction = Target->GetActorLocation() - GetOwner()->GetActorLocation();
	if (!Direction.IsNearlyZero())
	{
		GetOwner()->SetActorRotation(FMath::RInterpTo(GetOwner()->GetActorRotation(), Direction.Rotation(),
			DeltaTime, RandomizedRotateInterpSpeed));
	}
}

void UTurretMovementComponent::AddKickback()
{
	CurrentKickback += FMath::FRandRange(KickbackStrength * 0.7f, KickbackStrength * 1.3f);
}

void UTurretMovementComponent::UpdateVisualOffset(float DeltaTime)
{
	CurrentKickback = FMath::FInterpTo(CurrentKickback, 0.0f, DeltaTime, KickbackRecoverSpeed);
	HoverTime += DeltaTime;
	const FVector HoverOffset(0.0f, 0.0f, FMath::Sin(HoverTime * HoverFrequency) * HoverAmplitude);
	GetOwner()->SetActorLocation(BaseLocation + HoverOffset - GetOwner()->GetActorForwardVector() * CurrentKickback);
	FlightTransform = GetOwner()->GetActorTransform();
}

void UTurretMovementComponent::InterpolateReplicatedMovement(float DeltaTime)
{
	AActor* Turret = GetOwner();
	Turret->SetActorLocationAndRotation(
		FMath::VInterpTo(Turret->GetActorLocation(), FlightTransform.GetLocation(), DeltaTime, NetworkInterpSpeed),
		FMath::RInterpTo(Turret->GetActorRotation(), FlightTransform.Rotator(), DeltaTime, NetworkInterpSpeed));
}

void UTurretMovementComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UTurretMovementComponent, FlightTransform);
}
