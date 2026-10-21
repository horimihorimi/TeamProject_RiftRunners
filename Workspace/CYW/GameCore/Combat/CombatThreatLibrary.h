#pragma once
#include "CoreMinimal.h"
#include "Engine/HitResult.h"

struct FProjectileThreatRequest
{
	UWorld* World = nullptr;
	AActor* Causer = nullptr;
	AActor* Instigator = nullptr;
	FVector Origin = FVector::ZeroVector;
	FVector Direction = FVector::ForwardVector;
	float Speed = 0.0f;
	float Range = 0.0f;
	/** Predicted path 주변에서 evade 후보를 찾는 sweep 반경. */
	float TraceRadius = 12.0f;
	float GravityZ = 0.0f;
};

namespace CombatThreatLibrary
{
	bool NotifyHitScanThreat(AActor* Instigator, const FHitResult& Hit);
	void NotifyProjectileThreats(const FProjectileThreatRequest& Request);
}
