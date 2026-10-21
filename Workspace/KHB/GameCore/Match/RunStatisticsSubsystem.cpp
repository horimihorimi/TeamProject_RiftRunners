#include "RunStatisticsSubsystem.h"

#include "HAL/PlatformTime.h"

void URunStatisticsSubsystem::StartIfNeeded()
{
	if (!bStarted)
	{
		StartClockSeconds = FPlatformTime::Seconds();
		bStarted = true;
	}
}

void URunStatisticsSubsystem::ResetRun()
{
	StartClockSeconds = 0.0;
	FinalDuration = 0.0f;
	bStarted = false;
	bFinalized = false;
}

float URunStatisticsSubsystem::FinalizeDuration()
{
	if (bStarted && !bFinalized)
	{
		FinalDuration = static_cast<float>(FMath::Max(0.0, FPlatformTime::Seconds() - StartClockSeconds));
		bFinalized = true;
	}
	return FinalDuration;
}
