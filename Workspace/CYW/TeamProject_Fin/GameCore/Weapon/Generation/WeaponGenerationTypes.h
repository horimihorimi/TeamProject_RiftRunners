#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "GameCore/Weapon/Data/WeaponTypes.h"
#include "WeaponGenerationTypes.generated.h"

class UStaticMesh;
class UTexture2D;

UENUM(BlueprintType)
enum class EGunBarrelStat : uint8
{
	None,
	Damage,
	FireInterval,
	ReloadTime,
	MagazineSize,
	Range
};

USTRUCT(BlueprintType)
struct TEAMPROJECT_FIN_API FGunBarrelEffect
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gun Barrel Effect")
	EGunBarrelStat Stat = EGunBarrelStat::None;

	/** Percentage at max level. 0.10 means 2% at Barrel Level 1 and 10% at Level 5. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gun Barrel Effect", meta=(ClampMin="0.0", UIMin="0.0"))
	float Value = 0.0f;
};

USTRUCT(BlueprintType)
struct TEAMPROJECT_FIN_API FGunFrameRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gun Part")
	FName PartName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TObjectPtr<UStaticMesh> Mesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FWeaponRuntimeStats BaseStats;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gun Frame")
	EWeaponCategory WeaponCategory = EWeaponCategory::Rifle;
};

USTRUCT(BlueprintType)
struct TEAMPROJECT_FIN_API FGunBarrelRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gun Part")
	FName PartName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TObjectPtr<UStaticMesh> Mesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FGunBarrelEffect BonusPerLevel;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gun Part|Bonus")
	FGunBarrelEffect SecondaryBonusPerLevel;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gun Part|Penalty")
	FGunBarrelEffect PenaltyPerLevel;
};

USTRUCT(BlueprintType)
struct TEAMPROJECT_FIN_API FGunModifierRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gun Modifier")
	EWeaponModiferType Modifier = EWeaponModiferType::None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gun Modifier")
	FName PrefixName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gun Modifier")
	TObjectPtr<UTexture2D> Icon = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gun Modifier", meta=(MultiLine="true"))
	FText Description;
};
