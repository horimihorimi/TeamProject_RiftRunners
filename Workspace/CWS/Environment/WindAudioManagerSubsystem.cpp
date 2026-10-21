#include "Environment/WindAudioManagerSubsystem.h"

#include "Components/AudioComponent.h"
#include "Engine/World.h"
#include "Environment/WindVolumeActor.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

namespace
{
	struct FWindAudioCandidate
	{
		TObjectPtr<AWindVolumeActor> WindVolume;
		float Score = 0.0f;
	};
}

bool UWindAudioManagerSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	if (IsRunningDedicatedServer())
	{
		return false;
	}

	const UWorld* World = Cast<UWorld>(Outer);
	return World && World->IsGameWorld();
}

void UWindAudioManagerSubsystem::Deinitialize()
{
	StopAllWindAudio();
	RegisteredWindVolumes.Empty();
	Super::Deinitialize();
}

bool UWindAudioManagerSubsystem::IsTickable() const
{
	return !RegisteredWindVolumes.IsEmpty();
}

void UWindAudioManagerSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	UpdateAccumulator += DeltaTime;
	if (UpdateAccumulator < UpdateInterval)
	{
		return;
	}

	UpdateAccumulator = 0.0f;
	UpdateActiveWindAudio();
}

TStatId UWindAudioManagerSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UWindAudioManagerSubsystem, STATGROUP_Tickables);
}

void UWindAudioManagerSubsystem::RegisterWindVolume(AWindVolumeActor* WindVolume)
{
	if (IsValid(WindVolume) && !RegisteredWindVolumes.Contains(WindVolume))
	{
		RegisteredWindVolumes.Add(WindVolume);
	}
}

void UWindAudioManagerSubsystem::UnregisterWindVolume(AWindVolumeActor* WindVolume)
{
	if (!WindVolume)
	{
		return;
	}

	if (UAudioComponent* WindAudio = WindVolume->WindAudioComponent)
	{
		WindAudio->Stop();
	}

	RegisteredWindVolumes.Remove(WindVolume);
	ActiveWindVolumes.Remove(WindVolume);
}

void UWindAudioManagerSubsystem::UpdateActiveWindAudio()
{
	UWorld* World = GetWorld();
	APlayerController* PlayerController = World ? World->GetFirstPlayerController() : nullptr;
	APawn* LocalPawn = PlayerController && PlayerController->IsLocalController()
		? PlayerController->GetPawn()
		: nullptr;

	if (!IsValid(LocalPawn))
	{
		StopAllWindAudio();
		return;
	}

	const FVector ListenerLocation = LocalPawn->GetActorLocation();
	TArray<FWindAudioCandidate> Candidates;

	for (int32 Index = RegisteredWindVolumes.Num() - 1; Index >= 0; --Index)
	{
		AWindVolumeActor* WindVolume = RegisteredWindVolumes[Index].Get();
		if (!IsValid(WindVolume))
		{
			RegisteredWindVolumes.RemoveAtSwap(Index);
			continue;
		}

		UAudioComponent* WindAudio = WindVolume->WindAudioComponent;
		const float AudioRange = WindVolume->AudioRange;
		if (!IsValid(WindAudio) || !WindAudio->GetSound() || AudioRange <= 0.0f)
		{
			continue;
		}

		const float DistanceSquared = FVector::DistSquared(ListenerLocation, WindAudio->GetComponentLocation());
		if (DistanceSquared > FMath::Square(AudioRange))
		{
			continue;
		}

		const float NormalizedDistance = FMath::Sqrt(DistanceSquared) / AudioRange;
		FWindAudioCandidate& Candidate = Candidates.AddDefaulted_GetRef();
		Candidate.WindVolume = WindVolume;
		Candidate.Score = WindVolume->AudioPriority + (1.0f - NormalizedDistance);
	}

	Candidates.Sort([](const FWindAudioCandidate& Left, const FWindAudioCandidate& Right)
	{
		return Left.Score > Right.Score;
	});

	TSet<AWindVolumeActor*> DesiredWindVolumes;
	for (const FWindAudioCandidate& Candidate : Candidates)
	{
		if (DesiredWindVolumes.Num() >= MaxSimultaneousWindSounds)
		{
			break;
		}

		DesiredWindVolumes.Add(Candidate.WindVolume);
	}

	for (const TWeakObjectPtr<AWindVolumeActor>& ActiveVolume : ActiveWindVolumes)
	{
		AWindVolumeActor* WindVolume = ActiveVolume.Get();
		if (IsValid(WindVolume) && !DesiredWindVolumes.Contains(WindVolume))
		{
			WindVolume->WindAudioComponent->Stop();
		}
	}

	for (AWindVolumeActor* WindVolume : DesiredWindVolumes)
	{
		if (!ActiveWindVolumes.Contains(WindVolume))
		{
			WindVolume->WindAudioComponent->FadeIn(WindVolume->AudioFadeInDuration);
		}
	}

	ActiveWindVolumes.Reset();
	for (AWindVolumeActor* WindVolume : DesiredWindVolumes)
	{
		ActiveWindVolumes.Add(WindVolume);
	}
}

void UWindAudioManagerSubsystem::StopAllWindAudio()
{
	for (const TWeakObjectPtr<AWindVolumeActor>& ActiveVolume : ActiveWindVolumes)
	{
		if (AWindVolumeActor* WindVolume = ActiveVolume.Get())
		{
			if (UAudioComponent* WindAudio = WindVolume->WindAudioComponent)
			{
				WindAudio->Stop();
			}
		}
	}

	ActiveWindVolumes.Empty();
}
