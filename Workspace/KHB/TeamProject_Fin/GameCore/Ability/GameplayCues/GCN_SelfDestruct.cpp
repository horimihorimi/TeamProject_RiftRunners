#include "GCN_SelfDestruct.h"

#include "GameplayCueFxLibrary.h"
#include "GameCore/Tags/GameplayTagSystem.h"
#include "NiagaraComponent.h"

UGCN_SelfDestruct::UGCN_SelfDestruct()
{
	GameplayCueTag = GameplayTagSystem::GameplayCue_AI_SelfDestruct;
	GameplayCueName = GameplayCueTag.GetTagName();
}

bool UGCN_SelfDestruct::OnExecute_Implementation(
	AActor* MyTarget,
	const FGameplayCueParameters& Parameters) const
{
	UWorld* World = MyTarget ? MyTarget->GetWorld() : nullptr;
	if (!World)
	{
		return false;
	}

	const FVector ParameterLocation = FVector(Parameters.Location);
	const FVector SpawnLocation =
		ParameterLocation.IsNearlyZero()
			? MyTarget->GetActorLocation()
			: ParameterLocation;

	const float RequestedRadius = Parameters.RawMagnitude > 0.0f
		? Parameters.RawMagnitude
		: EffectRadius;

	const float Scale = RequestedRadius / FMath::Max(EffectRadius, 1.0f);

	if (ExplosionEffect)
	{
		if (UNiagaraComponent* NiagaraComponent =
			GameplayCueFxLibrary::SpawnNiagaraAtLocation(
				World,
				ExplosionEffect,
				SpawnLocation,
				FRotator::ZeroRotator,
				ENCPoolMethod::AutoRelease))
		{
			NiagaraComponent->SetWorldScale3D(FVector(Scale));
		}
	}

	if (ExplosionSound)
	{
		GameplayCueFxLibrary::SpawnSoundAtLocation(
			World,
			ExplosionSound,
			SpawnLocation,
			FRotator::ZeroRotator,
			SoundVolume,
			SoundPitch,
			SoundAttenuation);
	}

	UE_LOG(LogTemp, Warning,
		TEXT("[SelfDestructCue] Execute | Target=%s Location=%s Radius=%.1f Scale=%.2f"),
		*GetNameSafe(MyTarget),
		*SpawnLocation.ToCompactString(),
		RequestedRadius,
		Scale);

	return ExplosionEffect != nullptr || ExplosionSound != nullptr;
}
