#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameCore/Weapon/Data/WeaponTypes.h"
#include "GunPickupUIComponent.generated.h"

class UWidgetComponent;
class UUserWidget;
class UTexture2D;
class AGunObject;

/** Drives the existing pickup Blueprint's widget locally, without changing its layout. */
UCLASS(ClassGroup=(UI), meta=(BlueprintSpawnableComponent))
class TEAMPROJECT_FIN_API UGunPickupUIComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGunPickupUIComponent();
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;
	void RefreshData();

protected:
	/** World-space height above the visible gun mesh, in centimeters. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Gun UI", meta=(Units="cm"))
	float HeightAboveGun = 60.0f;

	/** Background textures for the WBP_GunUI Image named BG. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gun UI")
	TMap<EGunRarity, TObjectPtr<UTexture2D>> RarityBackgrounds;

private:
	void InitializePickupWidget();
	bool IsSelectedForLocalPlayer(const AGunObject& Gun) const;
	void UpdateWidget(UUserWidget& Widget, const AGunObject& Gun);
	void UpdateBackground(UUserWidget& Widget, EGunRarity Rarity) const;
	void UpdateWeaponSummary(UUserWidget& Widget, const FGeneratedWeaponData& Data) const;
	void UpdateBarrelEffects(UUserWidget& Widget, const AGunObject& Gun) const;
	void UpdateModifier(UUserWidget& Widget, const AGunObject& Gun) const;

	UPROPERTY(Transient)
	TObjectPtr<UWidgetComponent> PickupWidget;

	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> FilledWidget;

	bool bDataDirty = true;
};
