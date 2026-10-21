#pragma once

#include "CoreMinimal.h"

class UAbilitySystemComponent;
class UWorld;

namespace CombatTargetLibrary
{
	TEAMPROJECT_FIN_API bool AreHostile(
		const AActor* SourceActor,
		const AActor* TargetActor);
	TEAMPROJECT_FIN_API bool AreHostile(
		const UAbilitySystemComponent* SourceASC,
		const UAbilitySystemComponent* TargetASC);
	TEAMPROJECT_FIN_API bool AreFriendly(
		const UAbilitySystemComponent* SourceASC,
		const UAbilitySystemComponent* TargetASC);

	TEAMPROJECT_FIN_API bool IsHostileTarget(
		const AActor* TargetActor,
		const UAbilitySystemComponent* SourceASC,
		const AActor* SourceActor = nullptr,
		bool bAllowDeadTarget = false);

	TEAMPROJECT_FIN_API FVector GetTargetPoint(const AActor* TargetActor, FName SocketName = NAME_None);

	TEAMPROJECT_FIN_API TArray<AActor*> SelectRandomHostileTargets(
		UWorld* World,
		const FVector& Origin,
		const AActor* PrimaryTarget,
		const UAbilitySystemComponent* SourceASC,
		AActor* SourceActor,
		float SearchRadius,
		float MaxStepDistance,
		int32 MaxTargets,
		bool bRequireLineOfSight,
		FName TargetSocketName = NAME_None);
}
