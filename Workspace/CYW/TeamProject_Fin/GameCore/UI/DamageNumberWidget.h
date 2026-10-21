#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DamageNumberWidget.generated.h"

class UTextBlock;
class UScreenVisualFeedbackComponent;
class UWidgetAnimation;

UCLASS()
class TEAMPROJECT_FIN_API UDamageNumberWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void InitializeDamageNumber(float InDamageAmount, const FVector2D& InScreenPosition, bool bInWeakpoint);
	void SetPoolOwner(UScreenVisualFeedbackComponent* InPoolOwner);

	UFUNCTION(BlueprintCallable, Category = "Damage")
	void ReleaseToPool();

protected:
	virtual void OnAnimationFinished_Implementation(const UWidgetAnimation* Animation) override;

	UFUNCTION(BlueprintImplementableEvent, Category = "Damage")
	void OnDamageNumberInitialized(bool bWeakpoint);

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Damage")
	TObjectPtr<UTextBlock> DamageText;

private:
	UPROPERTY(EditDefaultsOnly, Category = "Damage|Pool", meta = (ClampMin = "0.1"))
	float SafetyLifetimeSeconds = 2.0f;

	TWeakObjectPtr<UScreenVisualFeedbackComponent> PoolOwner;
	FTimerHandle SafetyReleaseTimerHandle;
	bool bIsInPool = true;
};
