#pragma once

#include "CoreMinimal.h"
#include "WeaponTypes.generated.h"

class UStaticMesh;

UENUM(BlueprintType)
enum class EWeaponCategory : uint8
{
	Rifle,
	GrenadeLauncher,
	SniperRifle
};

UENUM(BlueprintType)
enum class EWeaponFireType : uint8
{
	HitScan,
	Projectile
};

UENUM(BlueprintType)
enum class EWeaponReloadSoundPhase : uint8
{
	Start,
	Insert,
	Complete
};

UENUM(BlueprintType)
enum class EWeaponModiferType : uint8
{
	None,
	SmartTracking,
	RocketBarrage,
	HealingNova,
	DeployTurret
};

USTRUCT(BlueprintType)
struct TEAMPROJECT_FIN_API FWeaponRecoilStats
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Recoil", meta = (ClampMin = "0.0", UIMin = "0.0", ToolTip = "Camera pitch kick in degrees applied each time this weapon fires."))
	float Pitch = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Recoil", meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "1.0", ToolTip = "Multiplier applied to recoil while aiming."))
	float AimingMultiplier = 0.65f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Recoil", meta = (ClampMin = "0.0", UIMin = "0.0", ToolTip = "Random yaw kick ratio relative to Pitch."))
	float YawRatio = 0.35f;
};

USTRUCT(BlueprintType)
struct TEAMPROJECT_FIN_API FWeaponSpreadStats
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Spread", meta = (ClampMin = "0.0", UIMin = "0.0", ToolTip = "Half-angle in degrees used to randomize the actual fire direction."))
	float Angle = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Spread", meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "1.0", ToolTip = "Multiplier applied to spread while aiming."))
	float AimingMultiplier = 0.5f;
};

USTRUCT(BlueprintType)
struct TEAMPROJECT_FIN_API FWeaponDamageFalloffStats
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Damage|Falloff", meta = (ClampMin = "0.0", UIMin = "0.0", Units = "cm"))
	float StartDistance = 3000.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Damage|Falloff", meta = (ClampMin = "0.0", UIMin = "0.0", Units = "cm"))
	float EndDistance = 10000.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Damage|Falloff", meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float MinimumDamageMultiplier = 0.5f;
};

USTRUCT(BlueprintType)
struct TEAMPROJECT_FIN_API FWeaponRuntimeStats
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
	float Damage = 10.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Damage", meta = (ClampMin = "1.0", UIMin = "1.0"))
	float WeakpointMultiplier = 1.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Damage")
	FWeaponDamageFalloffStats DamageFalloff;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
	float FireInterval = 0.1f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
	float ReloadTime = 1.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
	int32 MagazineSize = 30;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
	float Range = 10000.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Recoil")
	FWeaponRecoilStats Recoil;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Spread")
	FWeaponSpreadStats Spread;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
	EWeaponFireType FireType = EWeaponFireType::HitScan;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Projectile", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float ExplosionRadius = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Modifier")
	TArray<EWeaponModiferType> Modifiers;

	void Sanitize()
	{
		Damage = FMath::Max(Damage, 0.0f);
		WeakpointMultiplier = FMath::Max(WeakpointMultiplier, 1.0f);
		DamageFalloff.StartDistance = FMath::Max(DamageFalloff.StartDistance, 0.0f);
		DamageFalloff.EndDistance = FMath::Max(DamageFalloff.EndDistance, DamageFalloff.StartDistance);
		DamageFalloff.MinimumDamageMultiplier = FMath::Clamp(DamageFalloff.MinimumDamageMultiplier, 0.0f, 1.0f);
		FireInterval = FMath::Max(FireInterval, 0.0f);
		ReloadTime = FMath::Max(ReloadTime, 0.0f);
		MagazineSize = FMath::Max(MagazineSize, 1);
		Range = FMath::Max(Range, 0.0f);
		Recoil.Pitch = FMath::Max(Recoil.Pitch, 0.0f);
		Recoil.AimingMultiplier = FMath::Max(Recoil.AimingMultiplier, 0.0f);
		Recoil.YawRatio = FMath::Max(Recoil.YawRatio, 0.0f);
		Spread.Angle = FMath::Max(Spread.Angle, 0.0f);
		Spread.AimingMultiplier = FMath::Max(Spread.AimingMultiplier, 0.0f);
		ExplosionRadius = FMath::Max(ExplosionRadius, 0.0f);
	}
};

UENUM(BlueprintType)
enum class EGunRarity : uint8
{
	Common,
	Uncommon,
	Epic,
	Legendary
};

UENUM(BlueprintType)
enum class EWeaponGenerationSource : uint8
{
	EnemyDrop,
	Shop
};

/** Runtime-tunable gacha rates. Rate values use 0..1, so 0.006 means 0.6%. */
USTRUCT(BlueprintType)
struct TEAMPROJECT_FIN_API FWeaponPitySettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Pity", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float LegendaryBaseRate = 0.006f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Pity", meta = (ClampMin = "1"))
	int32 LegendarySoftPityStart = 74;

	/** Absolute rate added per pull from soft pity onward. 0.06 means +6 percentage points. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Pity", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float LegendarySoftPityRateIncrease = 0.06f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Pity", meta = (ClampMin = "1"))
	int32 LegendaryHardPity = 90;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Pity", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float EpicBaseRate = 0.051f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Pity", meta = (ClampMin = "1"))
	int32 EpicHardPity = 10;
};

/** Replicated, per-instance result of PCG weapon generation. Static behavior remains in the template DataAsset. */
USTRUCT(BlueprintType)
struct TEAMPROJECT_FIN_API FGeneratedWeaponData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Weapon|Generated")
	bool bIsGeneratedWeapon = false;

	UPROPERTY(BlueprintReadOnly, Category = "Weapon|Generated")
	EWeaponCategory WeaponCategory = EWeaponCategory::Rifle;

	UPROPERTY(BlueprintReadOnly, Category = "Weapon|Generated")
	EGunRarity Rarity = EGunRarity::Common;

	UPROPERTY(BlueprintReadOnly, Category = "Weapon|Generated")
	EWeaponGenerationSource GenerationSource = EWeaponGenerationSource::Shop;

	UPROPERTY(BlueprintReadOnly, Category = "Weapon|Generated")
	int32 BarrelLevel = 1;

	UPROPERTY(BlueprintReadOnly, Category = "Weapon|Generated")
	int32 MagazineLevel = 1;

	UPROPERTY(BlueprintReadOnly, Category = "Weapon|Generated")
	FName FrameName;

	UPROPERTY(BlueprintReadOnly, Category = "Weapon|Generated")
	FName BarrelName;

	UPROPERTY(BlueprintReadOnly, Category = "Weapon|Generated")
	FText GunName;

	UPROPERTY(BlueprintReadOnly, Category = "Weapon|Generated")
	FWeaponRuntimeStats RuntimeStats;

	UPROPERTY(BlueprintReadOnly, Category = "Weapon|Generated|Assembly")
	TObjectPtr<UStaticMesh> FrameMesh;

	UPROPERTY(BlueprintReadOnly, Category = "Weapon|Generated|Assembly")
	TObjectPtr<UStaticMesh> BarrelMesh;

	UPROPERTY(BlueprintReadOnly, Category = "Weapon|Generated|Assembly")
	FName BarrelSocketName = TEXT("BarrelSocket");

	UPROPERTY(BlueprintReadOnly, Category = "Weapon|Generated|Assembly")
	FTransform BarrelRelativeTransform = FTransform::Identity;
};
