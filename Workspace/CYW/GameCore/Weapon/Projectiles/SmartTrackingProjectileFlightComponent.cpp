#include "SmartTrackingProjectileFlightComponent.h"
#include "WeaponProjectile.h"
#include "GameCore/Weapon/Mods/SmartTracking/SmartTrackingPathComponent.h"
#include "Components/SplineComponent.h"
#include "Net/UnrealNetwork.h"

USmartTrackingProjectileFlightComponent::USmartTrackingProjectileFlightComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	SetIsReplicatedByDefault(true);
}

void USmartTrackingProjectileFlightComponent::StartFlight(USplineComponent* SharedPath)
{
	StopFlight();
	if (!GetOwner()->HasAuthority() || !IsValid(SharedPath)) return;
	Path = SharedPath;
	PathOwner = SharedPath->GetOwner()->FindComponentByClass<USmartTrackingPathComponent>();
	if (USmartTrackingPathComponent* Manager = PathOwner.Get()) AddTickPrerequisiteComponent(Manager);
	PathState.bIsActive = true;
	PathState.Start = Path->GetLocationAtSplinePoint(0, ESplineCoordinateSpace::World);
	PathState.Mid = Path->GetLocationAtSplinePoint(1, ESplineCoordinateSpace::World);
	PathState.End = Path->GetLocationAtSplinePoint(2, ESplineCoordinateSpace::World);
	SetComponentTickEnabled(true);
}

void USmartTrackingProjectileFlightComponent::ReleasePath()
{
	if (USmartTrackingPathComponent* Manager = PathOwner.Get())
	{
		RemoveTickPrerequisiteComponent(Manager);
		Manager->ReleasePathReference(Path);
	}
	Path = nullptr;
	PathOwner.Reset();
}

void USmartTrackingProjectileFlightComponent::StopFlight()
{
	ReleasePath();
	PathState = FSmartTrackingPathState();
	ClientDistance = 0.0f;
	SetComponentTickEnabled(false);
}

void USmartTrackingProjectileFlightComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	AWeaponProjectile* Projectile = Cast<AWeaponProjectile>(GetOwner());
	if (!Projectile || !Projectile->IsActiveProjectile() || !PathState.bIsActive) return;
	if (Projectile->HasAuthority()) UpdateServerFlight(Projectile, DeltaTime);
	else UpdateClientFlight(Projectile, DeltaTime);
}

void USmartTrackingProjectileFlightComponent::UpdateServerFlight(AWeaponProjectile* Projectile, float DeltaTime)
{
	if (!IsValid(Path))
	{
		Projectile->DeactivateToPool();
		return;
	}
	const float Length = Path->GetSplineLength();
	PathState.Start = Path->GetLocationAtSplinePoint(0, ESplineCoordinateSpace::World);
	PathState.Mid = Path->GetLocationAtSplinePoint(1, ESplineCoordinateSpace::World);
	PathState.End = Path->GetLocationAtSplinePoint(2, ESplineCoordinateSpace::World);
	PathState.DistanceTravelled = FMath::Min(PathState.DistanceTravelled + Projectile->GetProjectileSpeed() * DeltaTime, Length);
	const FVector NewLocation = Path->GetLocationAtDistanceAlongSpline(PathState.DistanceTravelled, ESplineCoordinateSpace::World);
	const FVector Direction = NewLocation - Projectile->GetActorLocation();
	if (!Direction.IsNearlyZero()) Projectile->SetActorRotation(Direction.Rotation());
	FHitResult Hit;
	Projectile->SetActorLocation(NewLocation, true, &Hit);
	// Swept movement can synchronously hit and return the projectile to its pool.
	if (!Projectile->IsActiveProjectile() || !PathState.bIsActive || !IsValid(Path)) return;
	if (PathState.DistanceTravelled >= Length)
	{
		const FVector ExitDirection = Path->GetDirectionAtDistanceAlongSpline(Length, ESplineCoordinateSpace::World);
		StopFlight();
		Projectile->ResumeLinearFlight(ExitDirection);
	}
}

void USmartTrackingProjectileFlightComponent::OnRep_PathState()
{
	SetComponentTickEnabled(PathState.bIsActive);
	if (!PathState.bIsActive) return;
	if (!ClientPath)
	{
		ClientPath = NewObject<USplineComponent>(GetOwner());
		ClientPath->SetAbsolute(true, true, true);
		ClientPath->RegisterComponent();
	}
	ClientPath->ClearSplinePoints(false);
	ClientPath->AddSplinePoint(PathState.Start, ESplineCoordinateSpace::World, false);
	ClientPath->AddSplinePoint(PathState.Mid, ESplineCoordinateSpace::World, false);
	ClientPath->AddSplinePoint(PathState.End, ESplineCoordinateSpace::World, false);
	for (int32 Index = 0; Index < 3; ++Index) ClientPath->SetSplinePointType(Index, ESplinePointType::Curve, false);
	ClientPath->UpdateSpline();
	ClientDistance = PathState.DistanceTravelled;
}

void USmartTrackingProjectileFlightComponent::UpdateClientFlight(AWeaponProjectile* Projectile, float DeltaTime)
{
	if (!ClientPath) return;
	ClientDistance = FMath::Min(ClientDistance + Projectile->GetProjectileSpeed() * DeltaTime, ClientPath->GetSplineLength());
	const FVector Location = ClientPath->GetLocationAtDistanceAlongSpline(ClientDistance, ESplineCoordinateSpace::World);
	const FVector Direction = ClientPath->GetDirectionAtDistanceAlongSpline(ClientDistance, ESplineCoordinateSpace::World);
	Projectile->SetActorLocationAndRotation(Location, Direction.Rotation());
}

void USmartTrackingProjectileFlightComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopFlight();
	if (ClientPath) ClientPath->DestroyComponent();
	ClientPath = nullptr;
	Super::EndPlay(EndPlayReason);
}

void USmartTrackingProjectileFlightComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(USmartTrackingProjectileFlightComponent, PathState);
}
