#include "WeaponGenerationSubsystem.h"

#include "GameCore/BasePlayerState.h"
#include "GameCore/Weapon/Core/WeaponComponent.h"
#include "GameCore/Weapon/Data/WeaponDataAsset.h"
#include "Engine/DataTable.h"
#include "GameFramework/PlayerController.h"

UWeaponGenerationSubsystem::UWeaponGenerationSubsystem()
{
	RandomStream.GenerateNewSeed();
	ShopLowRarityWeights.Add(EGunRarity::Common, 2.0f);
	ShopLowRarityWeights.Add(EGunRarity::Uncommon, 1.0f);
}

void UWeaponGenerationSubsystem::SetRandomSeed(int32 Seed)
{
	RandomStream.Initialize(Seed);
}

void UWeaponGenerationSubsystem::ClearRandomSeed()
{
	RandomStream.GenerateNewSeed();
}

EGunRarity UWeaponGenerationSubsystem::RollWeightedRarity(const TMap<EGunRarity, float>& Weights)
{
	constexpr EGunRarity OrderedRarities[] = {
		EGunRarity::Common,
		EGunRarity::Uncommon,
		EGunRarity::Epic,
		EGunRarity::Legendary
	};

	float TotalWeight = 0.0f;
	for (const EGunRarity Candidate : OrderedRarities)
	{
		TotalWeight += FMath::Max(Weights.FindRef(Candidate), 0.0f);
	}

	if (TotalWeight <= 0.0f)
	{
		return EGunRarity::Common;
	}

	const float Roll = RandomStream.FRand() * TotalWeight;
	float CumulativeWeight = 0.0f;
	for (const EGunRarity Candidate : OrderedRarities)
	{
		CumulativeWeight += FMath::Max(Weights.FindRef(Candidate), 0.0f);
		if (Roll < CumulativeWeight)
		{
			return Candidate;
		}
	}

	return EGunRarity::Legendary;
}

EGunRarity UWeaponGenerationSubsystem::RollRarity(
	const APlayerController* OwningPlayer,
	EWeaponGenerationSource Source,
	const TMap<EGunRarity, float>& EnemyDropRarityWeights)
{
	if (Source == EWeaponGenerationSource::EnemyDrop)
	{
		return RollWeightedRarity(EnemyDropRarityWeights);
	}

	const ABasePlayerState* PlayerState = OwningPlayer
		? OwningPlayer->GetPlayerState<ABasePlayerState>()
		: nullptr;
	const int32 LegendaryPull = (PlayerState ? PlayerState->GetShopPullsSinceLegendary() : 0) + 1;
	const int32 EpicPull = (PlayerState ? PlayerState->GetShopPullsSinceEpic() : 0) + 1;

	if (LegendaryPull >= FMath::Max(ShopPitySettings.LegendaryHardPity, 1))
	{
		return EGunRarity::Legendary;
	}

	float LegendaryRate = FMath::Clamp(ShopPitySettings.LegendaryBaseRate, 0.0f, 1.0f);
	const int32 SoftPityStart = FMath::Max(ShopPitySettings.LegendarySoftPityStart, 1);
	if (LegendaryPull >= SoftPityStart)
	{
		LegendaryRate += (LegendaryPull - SoftPityStart + 1)
			* FMath::Max(ShopPitySettings.LegendarySoftPityRateIncrease, 0.0f);
	}

	if (RandomStream.FRand() < FMath::Min(LegendaryRate, 1.0f))
	{
		return EGunRarity::Legendary;
	}

	if (EpicPull >= FMath::Max(ShopPitySettings.EpicHardPity, 1)
		|| RandomStream.FRand() < FMath::Clamp(ShopPitySettings.EpicBaseRate, 0.0f, 1.0f))
	{
		return EGunRarity::Epic;
	}

	return RollWeightedRarity(ShopLowRarityWeights);
}

void UWeaponGenerationSubsystem::ApplyBarrelEffect(
	FWeaponRuntimeStats& Stats,
	EGunBarrelStat Stat,
	float Value,
	float LevelScale,
	bool bPenalty) const
{
	const float Multiplier = 1.0f + Value * LevelScale;
	const float InverseMultiplier = 2.0f - Multiplier;

	switch (Stat)
	{
	case EGunBarrelStat::Damage:
		Stats.Damage = FMath::Max(0.0f, Stats.Damage * (bPenalty ? InverseMultiplier : Multiplier));
		break;
	case EGunBarrelStat::FireInterval:
		Stats.FireInterval = FMath::Max(0.01f, Stats.FireInterval * (bPenalty ? Multiplier : InverseMultiplier));
		break;
	case EGunBarrelStat::ReloadTime:
		Stats.ReloadTime = FMath::Max(0.0f, Stats.ReloadTime * (bPenalty ? Multiplier : InverseMultiplier));
		break;
	case EGunBarrelStat::MagazineSize:
		Stats.MagazineSize = FMath::Max(1, FMath::RoundToInt(Stats.MagazineSize * (bPenalty ? InverseMultiplier : Multiplier)));
		break;
	case EGunBarrelStat::Range:
	{
		const float RangeMultiplier = bPenalty ? InverseMultiplier : Multiplier;
		Stats.Range = FMath::Max(0.0f, Stats.Range * RangeMultiplier);
		Stats.DamageFalloff.StartDistance = FMath::Max(0.0f, Stats.DamageFalloff.StartDistance * RangeMultiplier);
		Stats.DamageFalloff.EndDistance = FMath::Max(
			Stats.DamageFalloff.StartDistance,
			Stats.DamageFalloff.EndDistance * RangeMultiplier);
		break;
	}
	default:
		break;
	}
}

bool UWeaponGenerationSubsystem::GenerateWeapon(
	APlayerController* OwningPlayer,
	EWeaponGenerationSource Source,
	const UDataTable* FrameTable,
	const UDataTable* BarrelTable,
	const UDataTable* ModifierTable,
	const UWeaponDataAsset* WeaponTemplate,
	FGeneratedWeaponData& OutGeneratedData,
	const TMap<EGunRarity, float>& EnemyDropRarityWeights)
{
	OutGeneratedData = FGeneratedWeaponData();
	if (!OwningPlayer || !OwningPlayer->HasAuthority() || !FrameTable || !BarrelTable || !WeaponTemplate)
	{
		return false;
	}

	const APawn* Pawn = OwningPlayer->GetPawn();
	const UWeaponComponent* WeaponComponent = Pawn ? Pawn->FindComponentByClass<UWeaponComponent>() : nullptr;
	if (!WeaponComponent)
	{
		return false;
	}

	TArray<FGunFrameRow*> EligibleFrames;
	FrameTable->GetAllRows(TEXT("WeaponGeneration"), EligibleFrames);
	EligibleFrames.RemoveAll([WeaponComponent](const FGunFrameRow* Row)
	{
		return !WeaponComponent->IsWeaponCategoryAllowed(Row->WeaponCategory);
	});

	TArray<FGunBarrelRow*> Barrels;
	BarrelTable->GetAllRows(TEXT("WeaponGeneration"), Barrels);

	if (EligibleFrames.IsEmpty() || Barrels.IsEmpty())
	{
		return false;
	}

	const FGunFrameRow* Frame = EligibleFrames[RandomStream.RandRange(0, EligibleFrames.Num() - 1)];
	const FGunBarrelRow* Barrel = Barrels[RandomStream.RandRange(0, Barrels.Num() - 1)];
	const EGunRarity Rarity = RollRarity(OwningPlayer, Source, EnemyDropRarityWeights);
	const int32 TotalLevels = Rarity == EGunRarity::Common
		? RandomStream.RandRange(3, 5)
		: Rarity == EGunRarity::Uncommon
			? RandomStream.RandRange(5, 7)
			: RandomStream.RandRange(7, 10);
	const int32 BarrelLevel = FMath::Clamp(RandomStream.RandRange(1, TotalLevels - 1), 1, 5);
	const int32 MagazineLevel = FMath::Clamp(TotalLevels - BarrelLevel, 1, 5);

	FWeaponRuntimeStats Stats = Frame->BaseStats;
	// Generated weapon modifiers come exclusively from the modifier table.
	Stats.Modifiers.Reset();
	Stats.MagazineSize = FMath::RoundToInt(Frame->BaseStats.MagazineSize * (1.0f + MagazineLevel * 0.2f));
	const float BarrelLevelScale = BarrelLevel * 0.2f;
	ApplyBarrelEffect(Stats, Barrel->BonusPerLevel.Stat, Barrel->BonusPerLevel.Value, BarrelLevelScale, false);
	ApplyBarrelEffect(Stats, Barrel->SecondaryBonusPerLevel.Stat, Barrel->SecondaryBonusPerLevel.Value, BarrelLevelScale, false);
	ApplyBarrelEffect(Stats, Barrel->PenaltyPerLevel.Stat, Barrel->PenaltyPerLevel.Value, BarrelLevelScale, true);

	FName PrefixName = NAME_None;
	if (Rarity == EGunRarity::Legendary && ModifierTable)
	{
		TArray<FGunModifierRow*> Rows;
		ModifierTable->GetAllRows(TEXT("WeaponGeneration"), Rows);

		if (!Rows.IsEmpty())
		{
			const FGunModifierRow* Modifier = Rows[RandomStream.RandRange(0, Rows.Num() - 1)];
			Stats.Modifiers.Add(Modifier->Modifier);
			PrefixName = Modifier->PrefixName;
		}
	}
	Stats.Sanitize();

	OutGeneratedData.bIsGeneratedWeapon = true;
	OutGeneratedData.WeaponCategory = Frame->WeaponCategory;
	OutGeneratedData.Rarity = Rarity;
	OutGeneratedData.GenerationSource = Source;
	OutGeneratedData.BarrelLevel = BarrelLevel;
	OutGeneratedData.MagazineLevel = MagazineLevel;
	OutGeneratedData.FrameName = Frame->PartName;
	OutGeneratedData.BarrelName = Barrel->PartName;
	OutGeneratedData.RuntimeStats = MoveTemp(Stats);
	OutGeneratedData.FrameMesh = Frame->Mesh;
	OutGeneratedData.BarrelMesh = Barrel->Mesh;
	OutGeneratedData.BarrelSocketName = WeaponTemplate->AssemblyData.BarrelSocketName;
	OutGeneratedData.BarrelRelativeTransform = WeaponTemplate->AssemblyData.BarrelRelativeTransform;
	OutGeneratedData.GunName = PrefixName.IsNone()
		? FText::Format(NSLOCTEXT("GunObject", "GeneratedGunNameWithoutModifier", "{0} {1}"),
			FText::FromName(OutGeneratedData.BarrelName), FText::FromName(OutGeneratedData.FrameName))
		: FText::Format(NSLOCTEXT("GunObject", "GeneratedGunNameWithModifier", "{0} {1} {2}"),
			FText::FromName(PrefixName), FText::FromName(OutGeneratedData.BarrelName), FText::FromName(OutGeneratedData.FrameName));
	return true;
}

void UWeaponGenerationSubsystem::CommitGeneration(
	APlayerController* OwningPlayer,
	const FGeneratedWeaponData& GeneratedData) const
{
	ABasePlayerState* PlayerState = OwningPlayer
		? OwningPlayer->GetPlayerState<ABasePlayerState>()
		: nullptr;
	if (PlayerState && GeneratedData.bIsGeneratedWeapon)
	{
		PlayerState->RecordGeneratedWeapon(
			GeneratedData.Rarity,
			GeneratedData.WeaponCategory,
			GeneratedData.GenerationSource);
	}
}
