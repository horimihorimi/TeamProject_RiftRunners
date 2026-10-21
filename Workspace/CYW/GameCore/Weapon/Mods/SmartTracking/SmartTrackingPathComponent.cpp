#include "SmartTrackingPathComponent.h"
#include "SmartTrackingPathLibrary.h"
#include "GameCore/Weapon/Core/WeaponBase.h"
#include "Components/SplineComponent.h"

USmartTrackingPathComponent::USmartTrackingPathComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

USplineComponent* USmartTrackingPathComponent::CreatePath(
	AActor* InstigatorActor,
	AActor* TargetActor,
	const FVector& StartLocation,
	const FVector& EndLocation)
{
	AWeaponBase* Weapon = GetWeapon();
	if (!Weapon || !Weapon->HasAuthority() || !Weapon->GetWeaponData())
	{
		return nullptr;
	}

	USplineComponent* Path = NewObject<USplineComponent>(Weapon);
	if (!Path)
	{
		return nullptr;
	}
	Path->SetAbsolute(true, true, true);
	Path->RegisterComponent();
	FSmartTrackingPathEntry& State = Paths.Add(Path);
	State.Instigator = InstigatorActor;
	State.Target = TargetActor;
	State.SideDirection = SmartTrackingPathLibrary::GetSideDirection(TargetActor);
	RebuildPath(Path, StartLocation, EndLocation);
	SetComponentTickEnabled(true);
	return Path;
}

void USmartTrackingPathComponent::RebuildPath(
	USplineComponent* Path,
	const FVector& StartLocation,
	const FVector& EndLocation) const
{
	const AWeaponBase* Weapon = GetWeapon();
	if (!Path || !Weapon || !Weapon->GetWeaponData())
	{
		return;
	}
	const FSmartTrackingPathEntry* State = Paths.Find(Path);

	TArray<FVector> Points;
	SmartTrackingPathLibrary::BuildPathPoints(
		StartLocation, EndLocation,
		Weapon->GetWeaponData()->SmartTrackingPath.CurveHeight,
		Weapon->GetWeaponData()->SmartTrackingPath.CurveSideOffset,
		State ? State->SideDirection : 1.0f,
		Points);
	Path->ClearSplinePoints(false);
	for (const FVector& Point : Points)
	{
		Path->AddSplinePoint(Point, ESplineCoordinateSpace::World, false);
	}
	for (int32 Index = 0; Index < Points.Num(); ++Index)
	{
		Path->SetSplinePointType(Index, ESplinePointType::Curve, false);
	}
	Path->UpdateSpline();
}

void USmartTrackingPathComponent::UpdateActivePaths()
{
	AWeaponBase* Weapon = GetWeapon();
	if (!Weapon)
	{
		return;
	}

	for (const TPair<TObjectPtr<USplineComponent>, FSmartTrackingPathEntry>& Pair : Paths)
	{
		USplineComponent* Path = Pair.Key;
		AActor* InstigatorActor = Pair.Value.Instigator;
		AActor* TargetActor = Pair.Value.Target;
		if (!IsValid(Path) || !IsValid(InstigatorActor) || !IsValid(TargetActor))
		{
			continue;
		}

		const FVector TargetPoint = SmartTrackingPathLibrary::GetTargetPoint(TargetActor);
		RebuildPath(Path, Weapon->GetFireTransform(InstigatorActor).GetLocation(), TargetPoint);
	}
}

void USmartTrackingPathComponent::TickComponent(
	float DeltaTime,
	ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	UpdateActivePaths();
}

void USmartTrackingPathComponent::AddPathReference(USplineComponent* SharedPath)
{
	if (FSmartTrackingPathEntry* State = Paths.Find(SharedPath))
	{
		++State->ReferenceCount;
	}
}

void USmartTrackingPathComponent::ReleasePathReference(USplineComponent* SharedPath)
{
	if (!IsValid(SharedPath))
	{
		return;
	}

	FSmartTrackingPathEntry* State = Paths.Find(SharedPath);
	if (!State)
	{
		return;
	}
	if (--State->ReferenceCount <= 0)
	{
		Paths.Remove(SharedPath);
		SharedPath->DestroyComponent();
		SetComponentTickEnabled(!Paths.IsEmpty());
	}
}

void USmartTrackingPathComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	for (const TPair<TObjectPtr<USplineComponent>, FSmartTrackingPathEntry>& Pair : Paths)
	{
		if (Pair.Key)
		{
			Pair.Key->DestroyComponent();
		}
	}
	Paths.Reset();
	SetComponentTickEnabled(false);
	Super::EndPlay(EndPlayReason);
}

AWeaponBase* USmartTrackingPathComponent::GetWeapon() const
{
	return Cast<AWeaponBase>(GetOwner());
}
