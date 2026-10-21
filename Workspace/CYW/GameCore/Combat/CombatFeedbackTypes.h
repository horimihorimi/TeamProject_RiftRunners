#pragma once

#include "CoreMinimal.h"
#include "CombatFeedbackTypes.generated.h"

USTRUCT(BlueprintType)
struct TEAMPROJECT_FIN_API FDamageFeedbackData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Damage")
	FVector_NetQuantize WorldLocation = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Damage")
	float DamageAmount = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Damage")
	bool bWeakpoint = false;
};
