#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

namespace SmartTrackingPathLibrary
{
	inline FVector GetTargetPoint(const AActor* TargetActor)
	{
		if (!TargetActor)
		{
			return FVector::ZeroVector;
		}

		FVector Origin = FVector::ZeroVector;
		FVector Extent = FVector::ZeroVector;
		TargetActor->GetActorBounds(true, Origin, Extent);
		return Origin + FVector(0.0f, 0.0f, Extent.Z * 0.25f);
	}

	inline float GetSideDirection(const AActor* TargetActor)
	{
		if (!TargetActor)
		{
			return 1.0f;
		}

		return (GetTypeHash(TargetActor->GetFName()) & 1u) == 0u ? 1.0f : -1.0f;
	}

	inline void BuildPathPoints(
		const FVector& StartLocation,
		const FVector& EndLocation,
		float CurveHeight,
		float CurveSideOffset,
		float SideDirection,
		TArray<FVector>& OutPoints)
	{
		const FVector Direction = (EndLocation - StartLocation).GetSafeNormal();
		const FVector Side = FVector::CrossProduct(Direction, FVector::UpVector).GetSafeNormal();
		const FVector MidLocation = FMath::Lerp(StartLocation, EndLocation, 0.5f)
			+ FVector::UpVector * FMath::Max(CurveHeight, 0.0f)
			+ Side * FMath::Max(CurveSideOffset, 0.0f) * SideDirection;

		OutPoints.Reset(3);
		OutPoints.Add(StartLocation);
		OutPoints.Add(MidLocation);
		OutPoints.Add(EndLocation);
	}
}
