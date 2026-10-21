#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SmartTrackingReticleWidget.generated.h"

UCLASS()
class TEAMPROJECT_FIN_API USmartTrackingReticleWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Smart Tracking")
	void ShowReticle();

	UFUNCTION(BlueprintCallable, Category = "Smart Tracking")
	void HideReticle();

	UFUNCTION(BlueprintCallable, Category = "Smart Tracking")
	void SetNeutralColor();

	UFUNCTION(BlueprintCallable, Category = "Smart Tracking")
	void SetLockedColor();

	UFUNCTION(BlueprintCallable, Category = "Smart Tracking")
	void SetFullyLockedColor();

protected:
	UFUNCTION(BlueprintImplementableEvent, Category = "Smart Tracking")
	void OnShowReticle();

	UFUNCTION(BlueprintImplementableEvent, Category = "Smart Tracking")
	void OnHideReticle();

	UFUNCTION(BlueprintImplementableEvent, Category = "Smart Tracking")
	void OnReticleColorChanged(FLinearColor NewColor);
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Smart Tracking|Color")
	FLinearColor NeutralColor = FLinearColor::White;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Smart Tracking|Color")
	FLinearColor LockedColor = FLinearColor(1.0f, 0.35f, 0.0f, 1.0f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Smart Tracking|Color")
	FLinearColor FullyLockedColor = FLinearColor::Red;
};
