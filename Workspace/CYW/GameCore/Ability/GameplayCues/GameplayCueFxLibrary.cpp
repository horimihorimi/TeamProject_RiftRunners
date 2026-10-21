#include "GameplayCueFxLibrary.h"

#include "Components/AudioComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"

namespace GameplayCueFxLibrary
{
	FVector GetActorFeetLocation(const AActor* Actor)
	{
		const UCharacterMovementComponent* CharacterMovement = Actor
			? Actor->FindComponentByClass<UCharacterMovementComponent>()
			: nullptr;
		return CharacterMovement
			? CharacterMovement->GetActorFeetLocation()
			: (Actor ? Actor->GetActorLocation() : FVector::ZeroVector);
	}

	USoundBase* SelectRandomSound(
		const TArray<TObjectPtr<USoundBase>>& RandomSounds,
		USoundBase* FallbackSound)
	{
		if (!RandomSounds.IsEmpty())
		{
			const int32 StartIndex = FMath::RandHelper(RandomSounds.Num());
			for (int32 Offset = 0; Offset < RandomSounds.Num(); ++Offset)
			{
				if (USoundBase* Candidate = RandomSounds[(StartIndex + Offset) % RandomSounds.Num()])
				{
					return Candidate;
				}
			}
		}
		return FallbackSound;
	}

	float GetRandomPitch(float PitchMin, float PitchMax, float MinimumPitch)
	{
		const float MinPitch = FMath::Max(FMath::Min(PitchMin, PitchMax), MinimumPitch);
		const float MaxPitch = FMath::Max(FMath::Max(PitchMin, PitchMax), MinPitch);
		return FMath::IsNearlyEqual(MinPitch, MaxPitch)
			? MinPitch
			: FMath::FRandRange(MinPitch, MaxPitch);
	}

	UNiagaraComponent* SpawnNiagaraAtLocation(
		UWorld* World,
		UNiagaraSystem* Effect,
		const FVector& Location,
		const FRotator& Rotation,
		ENCPoolMethod PoolMethod,
		bool bAutoDestroy,
		bool bAutoActivate)
	{
		if (!World || !Effect)
		{
			return nullptr;
		}

		return UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			World,
			Effect,
			Location,
			Rotation,
			FVector::OneVector,
			bAutoDestroy,
			bAutoActivate,
			PoolMethod
		);
	}

	UAudioComponent* SpawnSoundAtLocation(
		UWorld* World,
		USoundBase* Sound,
		const FVector& Location,
		const FRotator& Rotation,
		float Volume,
		float Pitch,
		USoundAttenuation* Attenuation)
	{
		if (!World || !Sound)
		{
			return nullptr;
		}

		return UGameplayStatics::SpawnSoundAtLocation(
			World,
			Sound,
			Location,
			Rotation,
			Volume,
			Pitch,
			0.0f,
			Attenuation
		);
	}

	UNiagaraComponent* SpawnNiagaraAttached(
		UNiagaraSystem* Effect,
		USceneComponent* AttachComponent,
		FName AttachSocket,
		EAttachLocation::Type AttachLocationType,
		ENCPoolMethod PoolMethod,
		bool bAutoDestroy,
		bool bAutoActivate)
	{
		if (!Effect || !AttachComponent)
		{
			return nullptr;
		}

		return UNiagaraFunctionLibrary::SpawnSystemAttached(
			Effect,
			AttachComponent,
			AttachSocket,
			FVector::ZeroVector,
			FRotator::ZeroRotator,
			AttachLocationType,
			bAutoDestroy,
			bAutoActivate,
			PoolMethod
		);
	}

	UAudioComponent* SpawnSoundAttached(
		USoundBase* Sound,
		USceneComponent* AttachComponent,
		FName AttachSocket,
		EAttachLocation::Type AttachLocationType,
		float Volume,
		float Pitch,
		USoundAttenuation* Attenuation,
		bool bStopWhenAttachedToDestroyed)
	{
		if (!Sound || !AttachComponent)
		{
			return nullptr;
		}

		return UGameplayStatics::SpawnSoundAttached(
			Sound,
			AttachComponent,
			AttachSocket,
			FVector::ZeroVector,
			FRotator::ZeroRotator,
			AttachLocationType,
			bStopWhenAttachedToDestroyed,
			Volume,
			Pitch,
			0.0f,
			Attenuation
		);
	}
}
