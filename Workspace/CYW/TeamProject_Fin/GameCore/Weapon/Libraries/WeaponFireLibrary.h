#pragma once

#include "CoreMinimal.h"

class AActor;

namespace WeaponFireLibrary
{
	TEAMPROJECT_FIN_API FVector CalculateAimDirection(
		UWorld* World,
		const AActor* SourceActor,
		AActor* InstigatorActor,
		const FVector& FireStart,
		const FVector& FallbackDirection,
		float Range);

	TEAMPROJECT_FIN_API bool PerformHitScan(
		UWorld* World,
		const AActor* SourceActor,
		AActor* InstigatorActor,
		const FVector& Start,
		const FVector& Direction,
		float Range,
		FHitResult& OutHit);

	TEAMPROJECT_FIN_API bool IsCharacterHit(const FHitResult& Hit);
}
