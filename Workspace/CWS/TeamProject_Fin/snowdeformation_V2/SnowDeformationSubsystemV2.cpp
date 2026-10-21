// Copyright Epic Games, Inc. All Rights Reserved.

#include "SnowDeformationSubsystemV2.h"
#include "SnowDeformationVolumeV2.h"
#include "Engine/World.h"

USnowDeformationSubsystemV2::USnowDeformationSubsystemV2()
{
}

bool USnowDeformationSubsystemV2::ShouldCreateSubsystem(UObject* Outer) const
{
	if (IsRunningDedicatedServer())
	{
		return false;
	}

	UWorld* World = Cast<UWorld>(Outer);
	if (!World || !World->IsGameWorld())
	{
		return false;
	}

	return true;
}

void USnowDeformationSubsystemV2::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
}

void USnowDeformationSubsystemV2::Deinitialize()
{
	ActiveVolumes.Empty();
	Super::Deinitialize();
}

void USnowDeformationSubsystemV2::RegisterVolume(ASnowDeformationVolumeV2* Volume)
{
	if (Volume && !ActiveVolumes.Contains(Volume))
	{
		ActiveVolumes.Add(Volume);
	}
}

void USnowDeformationSubsystemV2::UnregisterVolume(ASnowDeformationVolumeV2* Volume)
{
	if (Volume)
	{
		ActiveVolumes.Remove(Volume);
	}
}

void USnowDeformationSubsystemV2::RegisterDeformationPayload(const FSnowDeformationPayloadV2& InPayload)
{
	for (ASnowDeformationVolumeV2* Volume : ActiveVolumes)
	{
		if (Volume && Volume->IsInsideSnowArea(InPayload.HitLocationWorld))
		{
			Volume->RegisterDeformationPayload(InPayload);
			return;
		}
	}
}

bool USnowDeformationSubsystemV2::GetSnowSurfaceZAtLocation(const FVector& WorldLocation, float& OutSurfaceZ, float& OutThickness) const
{
	for (ASnowDeformationVolumeV2* Volume : ActiveVolumes)
	{
		if (Volume && Volume->IsInsideSnowArea(WorldLocation))
		{
			OutSurfaceZ = Volume->GetSnowSurfaceZ();
			OutThickness = Volume->GetSnowThickness();
			return true;
		}
	}

	OutSurfaceZ = 0.0f;
	OutThickness = 0.0f;
	return false;
}

void USnowDeformationSubsystemV2::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// 각 볼륨별로 모인 발자국들을 일괄 렌더링(도장 찍기)하도록 지시
	for (ASnowDeformationVolumeV2* Volume : ActiveVolumes)
	{
		if (Volume)
		{
			Volume->ProcessDeformationBatch(DeltaTime);
		}
	}
}

TStatId USnowDeformationSubsystemV2::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(USnowDeformationSubsystemV2, STATGROUP_Tickables);
}

