#include "SmartTrackingSplineVisualActor.h"

#include "Components/SplineComponent.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "GameCore/Weapon/Mods/SmartTracking/SmartTrackingPathLibrary.h"

ASmartTrackingSplineVisualActor::ASmartTrackingSplineVisualActor()
{
	PrimaryActorTick.bCanEverTick = false;

	SplineComponent = CreateDefaultSubobject<USplineComponent>(TEXT("SplineComponent"));
	SetRootComponent(SplineComponent);

	SplineComponent->SetMobility(EComponentMobility::Movable);

	NiagaraComponent = CreateDefaultSubobject<UNiagaraComponent>(TEXT("NiagaraComponent"));
	NiagaraComponent->SetupAttachment(SplineComponent);
	NiagaraComponent->SetAutoActivate(false);
	// The spline is driven by a user spline parameter, so Niagara's automatic
	// bounds can be too small and cull the line when the camera angle changes.
	SplineComponent->SetBoundsScale(1000.0f);
	NiagaraComponent->SetBoundsScale(1000.0f);
}

void ASmartTrackingSplineVisualActor::InitializeVisual(
	AActor* InSourceActor,
	AActor* InTargetActor)
{
	TrackedSourceActor = InSourceActor;
	TrackedTargetActor = InTargetActor;
	CurveSideDirection = SmartTrackingPathLibrary::GetSideDirection(InTargetActor);

	SetActorHiddenInGame(false);
	SetActorTickEnabled(false);
	SplineComponent->SetVisibility(true, true);

	if (NiagaraComponent)
	{
		NiagaraComponent->SetVisibility(true, true);
		if (NiagaraSystem)
		{
			NiagaraComponent->SetAsset(NiagaraSystem);
		}
	}

	OnVisualShown(InSourceActor, InTargetActor);
}

void ASmartTrackingSplineVisualActor::UpdateVisual(
	const FVector& StartLocation,
	const FVector& EndLocation,
	float CurveHeight,
	float CurveSideOffset)
{
	if (!SplineComponent)
	{
		return;
	}

	TArray<FVector> PathPoints;
	SmartTrackingPathLibrary::BuildPathPoints(
		StartLocation,
		EndLocation,
		CurveHeight,
		CurveSideOffset,
		CurveSideDirection,
		PathPoints);

	SplineComponent->ClearSplinePoints(false);

	for (const FVector& PathPoint : PathPoints)
	{
		SplineComponent->AddSplinePoint(
			PathPoint,
			ESplineCoordinateSpace::World,
			false);
	}

	for (int32 Index = 0; Index < PathPoints.Num(); ++Index)
	{
		SplineComponent->SetSplinePointType(Index, ESplinePointType::Curve, false);
	}

	SplineComponent->UpdateSpline();

	UpdateNiagaraParameters();

	SetActorHiddenInGame(false);
	SplineComponent->SetVisibility(true, true);
	if (NiagaraComponent)
	{
		NiagaraComponent->SetVisibility(true, true);
		if (!NiagaraComponent->IsActive())
		{
			NiagaraComponent->Activate(true);
		}
	}
}

void ASmartTrackingSplineVisualActor::HideVisual()
{
	OnVisualHidden(
		TrackedSourceActor.Get(),
		TrackedTargetActor.Get());

	if (NiagaraComponent)
	{
		NiagaraComponent->Deactivate();
	}

	SetActorHiddenInGame(true);

	TrackedSourceActor.Reset();
	TrackedTargetActor.Reset();
}

void ASmartTrackingSplineVisualActor::UpdateNiagaraParameters()
{
	if (!NiagaraComponent || !SplineComponent)
	{
		return;
	}

	NiagaraComponent->SetNiagaraVariableObject(
		TEXT("User.SplineObject"),
		SplineComponent
	);

	NiagaraComponent->SetVariableLinearColor(
		TEXT("User.LineColor"),
		LineColor
	);

	NiagaraComponent->SetVariableFloat(
		TEXT("User.LineThickness"),
		LineThickness
	);
}
