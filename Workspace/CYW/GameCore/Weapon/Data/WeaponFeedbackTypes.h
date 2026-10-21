#pragma once

#include "CoreMinimal.h"
#include "WeaponFeedbackTypes.generated.h"

class UNiagaraSystem;
class USoundAttenuation;
class USoundBase;

USTRUCT(BlueprintType)
struct TEAMPROJECT_FIN_API FWeaponResolvedSound
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Weapon|Feedback|Sound")
	TObjectPtr<USoundBase> Sound;

	UPROPERTY(BlueprintReadOnly, Category = "Weapon|Feedback|Sound")
	float Volume = 1.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Weapon|Feedback|Sound")
	float Pitch = 1.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Weapon|Feedback|Sound")
	TObjectPtr<USoundAttenuation> Attenuation;

	bool IsValid() const { return Sound != nullptr; }
};

USTRUCT(BlueprintType)
struct TEAMPROJECT_FIN_API FWeaponSoundSettings
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Feedback|Sound")
	TArray<TObjectPtr<USoundBase>> Sounds;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Feedback|Sound", meta = (ClampMin = "0.0"))
	float Volume = 1.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Feedback|Sound", meta = (ClampMin = "0.1"))
	FVector2D PitchRange = FVector2D(1.0f, 1.0f);
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Feedback|Sound")
	TObjectPtr<USoundAttenuation> Attenuation;

	FWeaponResolvedSound Resolve() const;
};

USTRUCT(BlueprintType)
struct TEAMPROJECT_FIN_API FWeaponFeedbackSet
{
	GENERATED_BODY()

	// 연출 효과

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Feedback")
	TObjectPtr<UNiagaraSystem> MuzzleFlashEffect;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Feedback")
	TObjectPtr<UNiagaraSystem> ShellEjectionEffect;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Feedback")
	TObjectPtr<UNiagaraSystem> SurfaceImpactEffect;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Feedback")
	TObjectPtr<UNiagaraSystem> CharacterImpactEffect;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Feedback")
	TObjectPtr<UNiagaraSystem> HitScanTracerEffect;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Feedback")
	TObjectPtr<UNiagaraSystem> ProjectileTrailEffect;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Feedback|Explosion")
	TObjectPtr<UNiagaraSystem> ExplosionEffect;


	// 발사 사운드

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Feedback|Sound")
	TArray<TObjectPtr<USoundBase>> FireSounds;

	UPROPERTY(
		EditDefaultsOnly,
		BlueprintReadOnly,
		Category = "Weapon|Feedback|Sound",
		meta = (ClampMin = "0.0", UIMin = "0.0")
	)
	float FireVolume = 1.0f;

	UPROPERTY(
		EditDefaultsOnly,
		BlueprintReadOnly,
		Category = "Weapon|Feedback|Sound",
		meta = (ClampMin = "0.1", UIMin = "0.1")
	)
	FVector2D FirePitchRange = FVector2D(1.0f, 1.0f);


	// 피격 사운드

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Feedback|Sound")
	TArray<TObjectPtr<USoundBase>> ImpactSounds;

	UPROPERTY(
		EditDefaultsOnly,
		BlueprintReadOnly,
		Category = "Weapon|Feedback|Sound",
		meta = (ClampMin = "0.0", UIMin = "0.0")
	)
	float ImpactVolume = 1.0f;


	// 도탄 사운드

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Feedback|Sound|Bounce")
	TArray<TObjectPtr<USoundBase>> BounceSounds;

	UPROPERTY(
		EditDefaultsOnly,
		BlueprintReadOnly,
		Category = "Weapon|Feedback|Sound|Bounce",
		meta = (ClampMin = "0.0", UIMin = "0.0")
	)
	float BounceVolume = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Feedback|Sound|Bounce")
	TObjectPtr<USoundAttenuation> BounceSoundAttenuation;


	// 폭발 사운드

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Feedback|Sound|Explosion")
	TArray<TObjectPtr<USoundBase>> ExplosionSounds;

	UPROPERTY(
		EditDefaultsOnly,
		BlueprintReadOnly,
		Category = "Weapon|Feedback|Sound|Explosion",
		meta = (ClampMin = "0.0", UIMin = "0.0")
	)
	float ExplosionVolume = 1.0f;


	// 공통 사운드 설정

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Feedback|Sound")
	TObjectPtr<USoundAttenuation> SoundAttenuation;


	// 사운드 선택 헬퍼

	USoundBase* GetRandomFireSound() const;

	USoundBase* GetRandomImpactSound() const;

	USoundBase* GetRandomExplosionSound() const;

	USoundBase* GetRandomBounceSound() const;

	USoundAttenuation* GetBounceSoundAttenuation() const;

	FWeaponResolvedSound ResolveFireSound() const;

	FWeaponResolvedSound ResolveImpactSound() const;

	FWeaponResolvedSound ResolveExplosionSound() const;

	FWeaponResolvedSound ResolveBounceSound() const;

private:
	static FWeaponResolvedSound MakeResolvedSound(
		USoundBase* Sound,
		float Volume,
		float Pitch,
		USoundAttenuation* Attenuation);

	static USoundBase* GetRandomSound(const TArray<TObjectPtr<USoundBase>>& Sounds);
};
