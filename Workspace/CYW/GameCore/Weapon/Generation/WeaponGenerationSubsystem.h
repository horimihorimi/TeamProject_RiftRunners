#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GameCore/Weapon/Data/WeaponTypes.h"
#include "WeaponGenerationTypes.h"
#include "WeaponGenerationSubsystem.generated.h"

class APlayerController;
class UDataTable;
class UWeaponDataAsset;

/** Central authority for weapon eligibility, rarity policy, pity, part rolls, and final stat assembly. */
UCLASS(BlueprintType)
class TEAMPROJECT_FIN_API UWeaponGenerationSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	UWeaponGenerationSubsystem();

	bool GenerateWeapon(
		APlayerController* OwningPlayer,
		EWeaponGenerationSource Source,
		const UDataTable* FrameTable,
		const UDataTable* BarrelTable,
		const UDataTable* ModifierTable,
		const UWeaponDataAsset* WeaponTemplate,
		FGeneratedWeaponData& OutGeneratedData,
		const TMap<EGunRarity, float>& EnemyDropRarityWeights);

	void CommitGeneration(APlayerController* OwningPlayer, const FGeneratedWeaponData& GeneratedData) const;

	UFUNCTION(BlueprintPure, Category = "Weapon Generation|Shop")
	FWeaponPitySettings GetShopPitySettings() const { return ShopPitySettings; }

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Weapon Generation|Shop")
	void SetShopPitySettings(const FWeaponPitySettings& NewSettings) { ShopPitySettings = NewSettings; }

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Weapon Generation|Debug")
	void SetRandomSeed(int32 Seed);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Weapon Generation|Debug")
	void ClearRandomSeed();

private:
	UPROPERTY(EditAnywhere, Category = "Weapon Generation|Shop")
	FWeaponPitySettings ShopPitySettings;

	UPROPERTY(EditAnywhere, Category = "Weapon Generation|Shop")
	TMap<EGunRarity, float> ShopLowRarityWeights;

	FRandomStream RandomStream;

	EGunRarity RollRarity(const APlayerController* OwningPlayer, EWeaponGenerationSource Source, const TMap<EGunRarity, float>& EnemyDropRarityWeights);
	EGunRarity RollWeightedRarity(const TMap<EGunRarity, float>& Weights);
	void ApplyBarrelEffect(FWeaponRuntimeStats& Stats, EGunBarrelStat Stat, float Value, float LevelScale, bool bPenalty) const;
};
