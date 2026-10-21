#include "GunPickupUIComponent.h"

#include "GunObject.h"
#include "Blueprint/UserWidget.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/WidgetComponent.h"
#include "GameCore/Interact/InteractComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

namespace
{
	void SetText(UUserWidget* Widget, const FName Name, const FText& Text)
	{
		if (UTextBlock* Block = Cast<UTextBlock>(Widget->GetWidgetFromName(Name)))
		{
			Block->SetText(Text);
		}
	}

	FText Number(float Value)
	{
		FNumberFormattingOptions Options;
		Options.SetMaximumFractionalDigits(1);
		return FText::AsNumber(Value, &Options);
	}

	FText BarrelEffectText(const FGunBarrelEffect& Effect, int32 Level, bool bPenalty)
	{
		FText Label;
		bool bLowerIsBetter = false;
		switch (Effect.Stat)
		{
		case EGunBarrelStat::Damage:
			Label = NSLOCTEXT("GunUI", "EffectDamage", "피해량");
			break;
		case EGunBarrelStat::FireInterval:
			Label = NSLOCTEXT("GunUI", "EffectInterval", "발사 간격");
			bLowerIsBetter = true;
			break;
		case EGunBarrelStat::ReloadTime:
			Label = NSLOCTEXT("GunUI", "EffectReload", "재장전 시간");
			bLowerIsBetter = true;
			break;
		case EGunBarrelStat::MagazineSize:
			Label = NSLOCTEXT("GunUI", "EffectMagazine", "탄창 용량");
			break;
		case EGunBarrelStat::Range:
			Label = NSLOCTEXT("GunUI", "EffectRange", "사거리");
			break;
		default:
			return FText::GetEmpty();
		}
		if (Effect.Value <= 0.0f)
		{
			return FText::GetEmpty();
		}
		const bool bIncrease = bPenalty == bLowerIsBetter;
		return FText::Format(NSLOCTEXT("GunUI", "EffectPercent", "{0} {1}{2}%"), Label,
			FText::FromString(bIncrease ? TEXT("+") : TEXT("-")), Number(Effect.Value * Level * 20.0f));
	}
}

UGunPickupUIComponent::UGunPickupUIComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostUpdateWork;
	RarityBackgrounds.Add(EGunRarity::Common, nullptr);
	RarityBackgrounds.Add(EGunRarity::Uncommon, nullptr);
	RarityBackgrounds.Add(EGunRarity::Epic, nullptr);
	RarityBackgrounds.Add(EGunRarity::Legendary, nullptr);
}

void UGunPickupUIComponent::BeginPlay()
{
	Super::BeginPlay();
	if (GetNetMode() == NM_DedicatedServer)
	{
		SetComponentTickEnabled(false);
		return;
	}

	InitializePickupWidget();
	if (!PickupWidget)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s: Gun UI requires a Widget Component with WBP_GunUI (DamageText)."), *GetOwner()->GetName());
		SetComponentTickEnabled(false);
	}
}

void UGunPickupUIComponent::InitializePickupWidget()
{
	// Reuse the widget component already placed by the designer in BP_Gun.
	TArray<UWidgetComponent*> Widgets;
	GetOwner()->GetComponents(Widgets);
	for (UWidgetComponent* Component : Widgets)
	{
		Component->InitWidget();
		UUserWidget* Widget = Component->GetUserWidgetObject();
		if (Widget && Widget->GetWidgetFromName(TEXT("DamageText")))
		{
			PickupWidget = Component;
			PickupWidget->SetWidgetSpace(EWidgetSpace::Screen);
			PickupWidget->SetPivot(FVector2D(0.5f, 1.0f));
			PickupWidget->SetVisibility(false);
			return;
		}
	}
}

void UGunPickupUIComponent::RefreshData()
{
	bDataDirty = true;
}

void UGunPickupUIComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	AGunObject* Gun = Cast<AGunObject>(GetOwner());
	if (!Gun || !PickupWidget)
	{
		return;
	}

	const bool bCanInteract = IsSelectedForLocalPlayer(*Gun);
	PickupWidget->SetVisibility(bCanInteract);
	if (!bCanInteract)
	{
		return;
	}

	// Use a world offset so pickup physics/rotation cannot put the panel below the gun.
	PickupWidget->SetWorldLocation(Gun->GetPickupDisplayLocation()
		+ FVector::UpVector * HeightAboveGun);
	UUserWidget* Widget = PickupWidget->GetUserWidgetObject();
	if (!Widget || (!bDataDirty && FilledWidget == Widget))
	{
		return;
	}
	FilledWidget = Widget;
	bDataDirty = false;
	UpdateWidget(*Widget, *Gun);
}

bool UGunPickupUIComponent::IsSelectedForLocalPlayer(const AGunObject& Gun) const
{
	APlayerController* PC = Cast<APlayerController>(Gun.GetOwner());
	APawn* Pawn = PC && PC->IsLocalController() ? PC->GetPawn() : nullptr;
	const UInteractComponent* Interact = Pawn ? Pawn->FindComponentByClass<UInteractComponent>() : nullptr;
	return Interact && Interact->GetNearest() == &Gun && Gun.CanInteract(PC);
}

void UGunPickupUIComponent::UpdateWidget(UUserWidget& Widget, const AGunObject& Gun)
{
	const FGeneratedWeaponData& Data = Gun.GetGeneratedData();
	UpdateBackground(Widget, Data.Rarity);
	UpdateWeaponSummary(Widget, Data);
	UpdateBarrelEffects(Widget, Gun);
	UpdateModifier(Widget, Gun);
}

void UGunPickupUIComponent::UpdateBackground(UUserWidget& Widget, EGunRarity Rarity) const
{
	if (UImage* Background = Cast<UImage>(Widget.GetWidgetFromName(TEXT("BG"))))
	{
		const TObjectPtr<UTexture2D>* Texture = RarityBackgrounds.Find(Rarity);
		if (Texture && *Texture)
		{
			Background->SetBrushFromTexture(*Texture, false);
		}
	}
}

void UGunPickupUIComponent::UpdateWeaponSummary(UUserWidget& Widget, const FGeneratedWeaponData& Data) const
{
	const FWeaponRuntimeStats& Stats = Data.RuntimeStats;
	FText DisplayName = Data.GunName;
	if (DisplayName.IsEmptyOrWhitespace())
	{
		switch (Data.WeaponCategory)
		{
		case EWeaponCategory::Rifle:
			DisplayName = NSLOCTEXT("GunUI", "DefaultRifleName", "돌격 소총");
			break;
		case EWeaponCategory::SniperRifle:
			DisplayName = NSLOCTEXT("GunUI", "DefaultSniperName", "저격 소총");
			break;
		case EWeaponCategory::GrenadeLauncher:
			DisplayName = NSLOCTEXT("GunUI", "DefaultLauncherName", "유탄발사기");
			break;
		}
	}
	SetText(&Widget, TEXT("NameText"), DisplayName);
	SetText(&Widget, TEXT("DamageText"), Number(Stats.FireInterval > 0.0f ? Stats.Damage / Stats.FireInterval : 0.0f));
	SetText(&Widget, TEXT("LevelText_Barrel"), FText::Format(
		NSLOCTEXT("GunUI", "BarrelLevel", "LV.{0}"), FText::AsNumber(Data.BarrelLevel)));
	SetText(&Widget, TEXT("LevelText_Magazine"), FText::Format(
		NSLOCTEXT("GunUI", "MagazineLevel", "LV.{0}"), FText::AsNumber(Data.MagazineLevel)));
}

void UGunPickupUIComponent::UpdateBarrelEffects(UUserWidget& Widget, const AGunObject& Gun) const
{
	const FGeneratedWeaponData& Data = Gun.GetGeneratedData();
	FText FirstEffect;
	FText SecondEffect;
	if (const UDataTable* BarrelTable = Gun.GetBarrelTable())
	{
		TArray<FGunBarrelRow*> Rows;
		BarrelTable->GetAllRows(TEXT("GunPickupUI"), Rows);
		for (const FGunBarrelRow* Row : Rows)
		{
			if (Row && Row->PartName == Data.BarrelName && Row->Mesh == Data.BarrelMesh)
			{
				FirstEffect = BarrelEffectText(Row->BonusPerLevel, Data.BarrelLevel, false);
				const FText Secondary = BarrelEffectText(Row->SecondaryBonusPerLevel, Data.BarrelLevel, false);
				if (FirstEffect.IsEmpty())
				{
					FirstEffect = Secondary;
				}
				else
				{
					SecondEffect = Secondary;
				}
				if (SecondEffect.IsEmpty())
				{
					SecondEffect = BarrelEffectText(Row->PenaltyPerLevel, Data.BarrelLevel, true);
				}
				break;
			}
		}
	}
	SetText(&Widget, TEXT("StatText_First"), FirstEffect);
	SetText(&Widget, TEXT("StatText_Second"), SecondEffect);
}

void UGunPickupUIComponent::UpdateModifier(UUserWidget& Widget, const AGunObject& Gun) const
{
	const FWeaponRuntimeStats& Stats = Gun.GetResolvedStats();
	FText Description;
	UTexture2D* Icon = nullptr;
	if (const UDataTable* ModifierTable = Gun.GetModifierTable())
	{
		TArray<FGunModifierRow*> Rows;
		ModifierTable->GetAllRows(TEXT("GunPickupUI"), Rows);
		for (const FGunModifierRow* Row : Rows)
		{
			if (Row && Row->Modifier != EWeaponModiferType::None && Stats.Modifiers.Contains(Row->Modifier))
			{
				Description = Description.IsEmpty() ? Row->Description : FText::Format(
					NSLOCTEXT("GunUI", "JoinDescriptions", "{0}\n{1}"), Description, Row->Description);
				if (!Icon)
				{
					Icon = Row->Icon;
				}
			}
		}
	}
	SetText(&Widget, TEXT("ModifierText"), Description);
	if (UImage* Image = Cast<UImage>(Widget.GetWidgetFromName(TEXT("ModifierImage"))))
	{
		Image->SetBrushFromTexture(Icon);
		Image->SetVisibility(Icon ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	}
}
