#include "DashedSplineRenderer.h"

#include "Components/SplineComponent.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialInterface.h"

void UDashedSplineRenderer::Initialize(AActor* Owner)
{
	OwnerActor = Owner;
	EnsureSpline();
}

void UDashedSplineRenderer::Update(
	const TArray<FVector>& Positions, const FDashedSplineStyle& Style, float WorldTime)
{
	UpdateSplinePath(Positions);
	UpdateDashSegments(Style, WorldTime);
}

void UDashedSplineRenderer::Shutdown()
{
	for (USplineMeshComponent* DashSegment : DashSegments)
	{
		if (IsValid(DashSegment))
		{
			DashSegment->DestroyComponent();
		}
	}
	DashSegments.Reset();

	if (IsValid(PreviewSplineComponent))
	{
		PreviewSplineComponent->DestroyComponent();
		PreviewSplineComponent = nullptr;
	}

	OwnerActor.Reset();
}

void UDashedSplineRenderer::EnsureSpline()
{
	if (PreviewSplineComponent)
	{
		return;
	}

	AActor* Owner = OwnerActor.Get();
	if (!Owner)
	{
		return;
	}

	PreviewSplineComponent = NewObject<USplineComponent>(Owner, TEXT("TrajectoryPreviewSpline"));
	if (!PreviewSplineComponent)
	{
		return;
	}

	PreviewSplineComponent->SetMobility(EComponentMobility::Movable);
	PreviewSplineComponent->SetHiddenInGame(true);
	PreviewSplineComponent->ClearSplinePoints(false);

	if (USceneComponent* RootComponent = Owner->GetRootComponent())
	{
		PreviewSplineComponent->SetupAttachment(RootComponent);
	}

	PreviewSplineComponent->RegisterComponent();
}

void UDashedSplineRenderer::Hide() const
{
	if (PreviewSplineComponent && PreviewSplineComponent->GetNumberOfSplinePoints() > 0)
	{
		PreviewSplineComponent->ClearSplinePoints(true);
	}

	for (USplineMeshComponent* DashSegment : DashSegments)
	{
		if (DashSegment)
		{
			DashSegment->SetVisibility(false);
		}
	}
}

void UDashedSplineRenderer::UpdateSplinePath(const TArray<FVector>& Positions) const
{
	if (!PreviewSplineComponent)
	{
		return;
	}

	PreviewSplineComponent->ClearSplinePoints(false);
	for (const FVector& Position : Positions)
	{
		PreviewSplineComponent->AddSplinePoint(Position, ESplineCoordinateSpace::World, false);
	}
	PreviewSplineComponent->UpdateSpline();
}

void UDashedSplineRenderer::UpdateDashSegments(const FDashedSplineStyle& Style, float WorldTime)
{
	if (!PreviewSplineComponent || PreviewSplineComponent->GetNumberOfSplinePoints() < 2 || !Style.Mesh)
	{
		HideUnusedDashSegments(0);
		return;
	}

	const float SplineLength = PreviewSplineComponent->GetSplineLength();
	const float SafeDashLength = FMath::Max(Style.Length, 1.0f);
	const float DashStride = FMath::Max(Style.Length + Style.Gap, 1.0f);
	const int32 RequiredDashCount = FMath::CeilToInt(SplineLength / DashStride) + 1;
	EnsureDashSegments(RequiredDashCount);

	const float AnimatedOffset = FMath::Fmod(WorldTime * FMath::Max(Style.MoveSpeed, 0.0f), DashStride);

	for (int32 DashIndex = 0; DashIndex < DashSegments.Num(); ++DashIndex)
	{
		USplineMeshComponent* DashSegment = DashSegments[DashIndex];
		if (!IsValid(DashSegment))
		{
			continue;
		}

		if (DashIndex >= RequiredDashCount)
		{
			DashSegment->SetVisibility(false);
			continue;
		}

		const float StartDistance = DashIndex * DashStride + AnimatedOffset - DashStride;
		const float EndDistance = FMath::Min(StartDistance + SafeDashLength, SplineLength);
		const float ClampedStartDistance = FMath::Max(StartDistance, 0.0f);
		if (EndDistance <= ClampedStartDistance)
		{
			DashSegment->SetVisibility(false);
			continue;
		}

		const FVector StartLocation = PreviewSplineComponent->GetLocationAtDistanceAlongSpline(ClampedStartDistance, ESplineCoordinateSpace::World);
		const FVector EndLocation = PreviewSplineComponent->GetLocationAtDistanceAlongSpline(EndDistance, ESplineCoordinateSpace::World);
		const FVector StartLocationLocal = PreviewSplineComponent->GetComponentTransform().InverseTransformPosition(StartLocation);
		const FVector EndLocationLocal = PreviewSplineComponent->GetComponentTransform().InverseTransformPosition(EndLocation);
		const FVector StartTangentLocal = PreviewSplineComponent->GetDirectionAtDistanceAlongSpline(
			ClampedStartDistance,
			ESplineCoordinateSpace::Local
		) * SafeDashLength;
		const FVector EndTangentLocal = PreviewSplineComponent->GetDirectionAtDistanceAlongSpline(
			EndDistance,
			ESplineCoordinateSpace::Local
		) * SafeDashLength;

		DashSegment->SetStaticMesh(Style.Mesh);
		DashSegment->SetForwardAxis(Style.ForwardAxis, false);
		DashSegment->SetStartAndEnd(StartLocationLocal, StartTangentLocal, EndLocationLocal, EndTangentLocal, false);
		DashSegment->SetStartScale(Style.Scale, false);
		DashSegment->SetEndScale(Style.Scale, false);

		if (Style.Material)
		{
			DashSegment->SetMaterial(0, Style.Material);
		}
		DashSegment->SetVisibility(true);
		DashSegment->UpdateMesh();
	}
}

void UDashedSplineRenderer::EnsureDashSegments(int32 RequiredCount)
{
	AActor* Owner = OwnerActor.Get();
	if (!Owner || !PreviewSplineComponent)
	{
		return;
	}

	while (DashSegments.Num() < RequiredCount)
	{
		USplineMeshComponent* DashSegment = NewObject<USplineMeshComponent>(Owner);
		if (!DashSegment)
		{
			return;
		}

		DashSegment->CreationMethod = EComponentCreationMethod::UserConstructionScript;
		DashSegment->SetMobility(EComponentMobility::Movable);
		DashSegment->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		DashSegment->SetCastShadow(false);
		DashSegment->SetupAttachment(PreviewSplineComponent);
		DashSegment->RegisterComponent();
		DashSegments.Add(DashSegment);
	}
}

void UDashedSplineRenderer::HideUnusedDashSegments(int32 UsedSegmentCount) const
{
	for (int32 Index = UsedSegmentCount; Index < DashSegments.Num(); ++Index)
	{
		if (DashSegments[Index])
		{
			DashSegments[Index]->SetVisibility(false);
		}
	}
}
