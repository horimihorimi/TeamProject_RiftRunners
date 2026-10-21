#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameCore/Combat/CombatFeedbackTypes.h"
#include "ScreenVisualFeedbackComponent.generated.h"

class UCombatFeedbackComponent;
class UDamageNumberWidget;
class UCanvasPanel;
class APlayerController;

UCLASS(ClassGroup = (UI), meta = (BlueprintSpawnableComponent))
class TEAMPROJECT_FIN_API UScreenVisualFeedbackComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UScreenVisualFeedbackComponent();
	void SetWidgetHost(UCanvasPanel* InWidgetHost);
	void ReturnDamageNumberToPool(UDamageNumberWidget* Widget);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(EditAnywhere, Category = "Damage Number")
	TSubclassOf<UDamageNumberWidget> DamageNumberWidgetClass;

	UPROPERTY(EditAnywhere, Category = "Damage Number|Pool", meta = (ClampMin = "0", ClampMax = "128"))
	int32 InitialPoolSize = 16;

	UPROPERTY(EditAnywhere, Category = "Damage Number|Pool", meta = (ClampMin = "1", ClampMax = "256"))
	int32 MaxPoolSize = 64;

private:
	void BindFeedbackSource();
	void OnDamageFeedbackBatch(const TArray<FDamageFeedbackData>& FeedbackBatch);
	void ShowDamageFeedback(const FDamageFeedbackData& Feedback);
	void PrewarmDamageNumberPool();
	UDamageNumberWidget* CreateDamageNumberWidget();
	UDamageNumberWidget* AcquireDamageNumberWidget();

	TWeakObjectPtr<UCombatFeedbackComponent> FeedbackSource;
	TWeakObjectPtr<UCanvasPanel> WidgetHost;
	TWeakObjectPtr<APlayerController> LocalPlayerController;
	FDelegateHandle FeedbackDelegateHandle;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UDamageNumberWidget>> AllDamageNumberWidgets;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UDamageNumberWidget>> AvailableDamageNumberWidgets;

};
