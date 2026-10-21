#include "WeaponFeedbackTypes.h"

#include "GameCore/Ability/GameplayCues/GameplayCueFxLibrary.h"

FWeaponResolvedSound FWeaponSoundSettings::Resolve() const
{
	FWeaponResolvedSound Result;
	Result.Sound = GameplayCueFxLibrary::SelectRandomSound(Sounds, nullptr);
	Result.Volume = Volume;
	Result.Pitch = GameplayCueFxLibrary::GetRandomPitch(PitchRange.X, PitchRange.Y);
	Result.Attenuation = Attenuation;
	return Result;
}

#include "Sound/SoundAttenuation.h"
#include "Sound/SoundBase.h"

USoundBase* FWeaponFeedbackSet::GetRandomFireSound() const
{
	return GetRandomSound(FireSounds);
}

USoundBase* FWeaponFeedbackSet::GetRandomImpactSound() const
{
	return GetRandomSound(ImpactSounds);
}

USoundBase* FWeaponFeedbackSet::GetRandomExplosionSound() const
{
	return GetRandomSound(ExplosionSounds);
}

USoundBase* FWeaponFeedbackSet::GetRandomBounceSound() const
{
	return GetRandomSound(BounceSounds);
}

USoundAttenuation* FWeaponFeedbackSet::GetBounceSoundAttenuation() const
{
	return BounceSoundAttenuation ? BounceSoundAttenuation.Get() : SoundAttenuation.Get();
}

FWeaponResolvedSound FWeaponFeedbackSet::ResolveFireSound() const
{
	FWeaponResolvedSound ResolvedSound;
	ResolvedSound.Sound = GetRandomFireSound();
	ResolvedSound.Volume = FireVolume;
	ResolvedSound.Pitch = FMath::FRandRange(
		FMath::Min(FirePitchRange.X, FirePitchRange.Y),
		FMath::Max(FirePitchRange.X, FirePitchRange.Y));
	ResolvedSound.Attenuation = SoundAttenuation;
	return ResolvedSound;
}

FWeaponResolvedSound FWeaponFeedbackSet::ResolveImpactSound() const
{
	return MakeResolvedSound(GetRandomImpactSound(), ImpactVolume, 1.0f, SoundAttenuation.Get());
}

FWeaponResolvedSound FWeaponFeedbackSet::ResolveExplosionSound() const
{
	return MakeResolvedSound(GetRandomExplosionSound(), ExplosionVolume, 1.0f, SoundAttenuation.Get());
}

FWeaponResolvedSound FWeaponFeedbackSet::ResolveBounceSound() const
{
	return MakeResolvedSound(GetRandomBounceSound(), BounceVolume, 1.0f, GetBounceSoundAttenuation());
}

FWeaponResolvedSound FWeaponFeedbackSet::MakeResolvedSound(
	USoundBase* Sound,
	float Volume,
	float Pitch,
	USoundAttenuation* Attenuation)
{
	FWeaponResolvedSound ResolvedSound;
	ResolvedSound.Sound = Sound;
	ResolvedSound.Volume = Volume;
	ResolvedSound.Pitch = Pitch;
	ResolvedSound.Attenuation = Attenuation;
	return ResolvedSound;
}

USoundBase* FWeaponFeedbackSet::GetRandomSound(const TArray<TObjectPtr<USoundBase>>& Sounds)
{
	if (Sounds.IsEmpty())
	{
		return nullptr;
	}

	const int32 StartIndex = FMath::RandHelper(Sounds.Num());
	for (int32 Offset = 0; Offset < Sounds.Num(); ++Offset)
	{
		if (USoundBase* Candidate = Sounds[(StartIndex + Offset) % Sounds.Num()])
		{
			return Candidate;
		}
	}

	return nullptr;
}
