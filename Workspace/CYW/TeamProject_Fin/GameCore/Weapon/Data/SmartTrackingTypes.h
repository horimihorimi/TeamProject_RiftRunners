#pragma once

#include "CoreMinimal.h"
#include "WeaponTypes.h"
#include "SmartTrackingTypes.generated.h"

UENUM(BlueprintType)
enum class ESmartTrackingLockState : uint8
{
	Inactive,
	Tracking,
	FullyLocked
};

USTRUCT(BlueprintType)
struct TEAMPROJECT_FIN_API FSmartTrackingPathSettings
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Smart Tracking|Path", meta = (ClampMin = "0.0", Units = "cm"))
	float CurveHeight = 120.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Smart Tracking|Path", meta = (ClampMin = "0.0", Units = "cm"))
	float CurveSideOffset = 40.0f;
};

USTRUCT(BlueprintType)
struct TEAMPROJECT_FIN_API FSmartTrackingTargetingSettings
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Smart Tracking|Targeting", meta = (ClampMin = "0.0", Units = "cm"))
	float Range = 3000.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Smart Tracking|Targeting", meta = (ClampMin = "0.0", ClampMax = "180.0", Units = "deg"))
	float ConeHalfAngleDegrees = 22.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Smart Tracking|Targeting", meta = (ClampMin = "0.01", ClampMax = "1.0"))
	FVector2D ReticleLockBoxRatio = FVector2D(0.5f, 0.5f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Smart Tracking|Targeting", meta = (ClampMin = "0.01", Units = "s"))
	float ScanInterval = 0.1f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Smart Tracking|Targeting")
	bool bRequireLineOfSight = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Smart Tracking|Lock", meta = (ClampMin = "0.01"))
	float LockStackIntervalMultiplier = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Smart Tracking|Lock", meta = (ClampMin = "0.0", Units = "s"))
	float LockPreparationTime = 0.0f;
};

USTRUCT(BlueprintType)
struct TEAMPROJECT_FIN_API FSmartTrackingFireSettings
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Smart Tracking|Fire", meta = (ClampMin = "0.01", Units = "s"))
	float RifleVolleyInterval = 0.1f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Smart Tracking|Fire", meta = (ClampMin = "0.01", Units = "s"))
	float SniperRifleVolleyInterval = 0.1f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Smart Tracking|Fire", meta = (ClampMin = "0.01", Units = "s"))
	float GrenadeLauncherVolleyInterval = 0.1f;

	float GetVolleyInterval(EWeaponCategory WeaponCategory) const
	{
		switch (WeaponCategory)
		{
		case EWeaponCategory::SniperRifle:
			return SniperRifleVolleyInterval;
		case EWeaponCategory::GrenadeLauncher:
			return GrenadeLauncherVolleyInterval;
		case EWeaponCategory::Rifle:
		default:
			return RifleVolleyInterval;
		}
	}
};

USTRUCT()
struct TEAMPROJECT_FIN_API FSmartTrackingFireRequest
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<AActor> TargetActor;

	UPROPERTY()
	int32 BulletCount = 0;
};

struct FSmartTrackingShotResult
{
	TWeakObjectPtr<AActor> TargetActor;
	int32 ShotIndex = INDEX_NONE;
};

DECLARE_MULTICAST_DELEGATE_OneParam(
	FOnSmartTrackingShotFired,
	const FSmartTrackingShotResult&);

namespace SmartTrackingConstants
{
	constexpr int32 MaxTargetRequests = 32;
}
