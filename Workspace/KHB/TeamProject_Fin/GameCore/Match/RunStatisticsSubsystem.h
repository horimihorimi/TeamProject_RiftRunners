#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "RunStatisticsSubsystem.generated.h"

// 서버의 런 타이머. 월드가 교체되어도 유지하고 새 게임/런 포기 때 초기화한다.
UCLASS()
class TEAMPROJECT_FIN_API URunStatisticsSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()
public:
	void StartIfNeeded();
	void ResetRun();
	float FinalizeDuration();
	bool IsCollecting() const { return bStarted && !bFinalized; }
	bool IsFinalized() const { return bFinalized; }
	float GetFinalDuration() const { return FinalDuration; }
private:
	double StartClockSeconds = 0.0;
	float FinalDuration = 0.0f;
	bool bStarted = false;
	bool bFinalized = false;
};
