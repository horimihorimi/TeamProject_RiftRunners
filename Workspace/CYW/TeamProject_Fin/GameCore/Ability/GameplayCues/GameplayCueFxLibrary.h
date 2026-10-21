#pragma once

#include "CoreMinimal.h"
#include "NiagaraCommon.h"

class AActor;
class UAudioComponent;
class UNiagaraComponent;
class UNiagaraSystem;
class USceneComponent;
class USoundAttenuation;
class USoundBase;
class UWorld;

namespace GameplayCueFxLibrary
{
	TEAMPROJECT_FIN_API FVector GetActorFeetLocation(const AActor* Actor);
	TEAMPROJECT_FIN_API USoundBase* SelectRandomSound(
		const TArray<TObjectPtr<USoundBase>>& RandomSounds,
		USoundBase* FallbackSound);
	TEAMPROJECT_FIN_API float GetRandomPitch(float PitchMin, float PitchMax, float MinimumPitch = 0.1f);

	TEAMPROJECT_FIN_API UNiagaraComponent* SpawnNiagaraAtLocation(
		UWorld* World,
		UNiagaraSystem* Effect,
		const FVector& Location,
		const FRotator& Rotation,
		ENCPoolMethod PoolMethod = ENCPoolMethod::None,
		bool bAutoDestroy = true,
		bool bAutoActivate = true);

	TEAMPROJECT_FIN_API UAudioComponent* SpawnSoundAtLocation(
		UWorld* World,
		USoundBase* Sound,
		const FVector& Location,
		const FRotator& Rotation,
		float Volume = 1.0f,
		float Pitch = 1.0f,
		USoundAttenuation* Attenuation = nullptr);

	TEAMPROJECT_FIN_API UNiagaraComponent* SpawnNiagaraAttached(
		UNiagaraSystem* Effect,
		USceneComponent* AttachComponent,
		FName AttachSocket = NAME_None,
		EAttachLocation::Type AttachLocationType = EAttachLocation::KeepRelativeOffset,
		ENCPoolMethod PoolMethod = ENCPoolMethod::None,
		bool bAutoDestroy = true,
		bool bAutoActivate = true);

	TEAMPROJECT_FIN_API UAudioComponent* SpawnSoundAttached(
		USoundBase* Sound,
		USceneComponent* AttachComponent,
		FName AttachSocket = NAME_None,
		EAttachLocation::Type AttachLocationType = EAttachLocation::KeepRelativeOffset,
		float Volume = 1.0f,
		float Pitch = 1.0f,
		USoundAttenuation* Attenuation = nullptr,
		bool bStopWhenAttachedToDestroyed = true);
}
