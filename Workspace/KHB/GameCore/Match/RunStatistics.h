#pragma once

#include "CoreMinimal.h"
#include "RunStatistics.generated.h"

// 서버 집계와 최종 결과를 같은 구조체로 복제하여 마지막 타격/확정의 도착 순서를 보장한다.
USTRUCT(BlueprintType)
struct FRunStatistics
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	double TotalDamage = 0.0;
	UPROPERTY(BlueprintReadOnly)
	double BossDamage = 0.0;
	UPROPERTY(BlueprintReadOnly)
	int32 KillCount = 0;
	UPROPERTY(BlueprintReadOnly)
	double DamageTaken = 0.0;
	UPROPERTY(BlueprintReadOnly)
	int32 ReviveCount = 0;
	UPROPERTY(BlueprintReadOnly)
	float ClearDurationSeconds = 0.0f;
	UPROPERTY(BlueprintReadOnly)
	bool bFinalized = false;
};
