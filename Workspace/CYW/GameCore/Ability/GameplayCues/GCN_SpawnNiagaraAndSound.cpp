#include "GCN_SpawnNiagaraAndSound.h"

#include "../../Tags/GameplayTagSystem.h"
#include "GameplayCueFxLibrary.h"
#include "DrawDebugHelpers.h"

bool UGCN_SpawnNiagaraAndSound::OnExecute_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) const
{
	UWorld* World = MyTarget ? MyTarget->GetWorld() : nullptr;
	USoundBase* SelectedSound = GameplayCueFxLibrary::SelectRandomSound(RandomSounds, Sound);
	if (!World || (!Effect && !SelectedSound))
	{
		return false;
	}

	const FVector SpawnLocation = bUseParameterLocation
		? FVector(Parameters.Location)
		: MyTarget->GetActorLocation();
	const FRotator SpawnRotation = bUseTargetRotation && MyTarget
		? MyTarget->GetActorRotation()
		: Parameters.Normal.Rotation();

	if (Effect)
	{
		GameplayCueFxLibrary::SpawnNiagaraAtLocation(
			World,
			Effect,
			SpawnLocation,
			SpawnRotation
		);
	}

	if (SelectedSound)
	{
		GameplayCueFxLibrary::SpawnSoundAtLocation(
			World,
			SelectedSound,
			SpawnLocation,
			SpawnRotation,
			SoundVolume,
			SoundPitch,
			SoundAttenuation
		);
	}

#if ENABLE_DRAW_DEBUG
	if (Parameters.RawMagnitude > 0.0f
		&& Parameters.AggregatedSourceTags.HasTagExact(GameplayTagSystem::CueParameter_ShowDebug))
	{
		DrawDebugSphere(World, SpawnLocation, Parameters.RawMagnitude, 32, FColor::Cyan, false, 2.0f, 0, 2.0f);
	}
#endif

	return true;
}
