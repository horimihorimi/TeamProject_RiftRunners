#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameCore/Weapon/Data/SmartTrackingTypes.h"
#include "SmartTrackingVisualComponent.generated.h"

class APlayerController;
class APawn;
class AActor;
class ASmartTrackingSplineVisualActor;
class UAbilitySystemComponent;
class UCanvasPanel;
class USmartTrackingReticleWidget;
class USmartTrackingTargetVisualManager;
class USmartTrackingLockIndicatorWidget;
class USoundBase;
struct FGameplayEventData;
struct FGameplayTag;

USTRUCT()
struct FSmartTrackingVisualSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Tracking Reticle")
	TSubclassOf<USmartTrackingReticleWidget> SmartTrackingReticleWidgetClass;
	UPROPERTY(EditAnywhere, Category = "Tracking Reticle")
	TSubclassOf<USmartTrackingLockIndicatorWidget> SmartTrackingLockIndicatorWidgetClass;
	UPROPERTY(EditAnywhere, Category = "Tracking Reticle")
	TSubclassOf<ASmartTrackingSplineVisualActor> SmartTrackingSplineVisualActorClass;
	UPROPERTY(EditAnywhere, Category = "Sound")
	TObjectPtr<USoundBase> SmartTrackingStartSound;
	UPROPERTY(EditAnywhere, Category = "Sound")
	TObjectPtr<USoundBase> SmartTrackingTargetLockedSound;
	UPROPERTY(EditAnywhere, Category = "Sound")
	TObjectPtr<USoundBase> SmartTrackingTargetLostSound;
	UPROPERTY(EditAnywhere, Category = "Sound")
	TObjectPtr<USoundBase> SmartTrackingBulletStackSound;
	UPROPERTY(EditAnywhere, Category = "Sound")
	TObjectPtr<USoundBase> SmartTrackingEndSound;
	UPROPERTY(EditAnywhere, Category = "Sound", meta = (ClampMin = "0.0"))
	float SmartTrackingSoundVolume = 1.0f;
};

UCLASS(ClassGroup = (UI))
class TEAMPROJECT_FIN_API USmartTrackingVisualComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USmartTrackingVisualComponent();
	void SetWidgetHost(UCanvasPanel* InWidgetHost);
	void NotifyLockLostAnimationFinished(USmartTrackingLockIndicatorWidget* LockWidget);
	void Shutdown();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

	UPROPERTY(EditAnywhere, Category = "Smart Tracking")
	FSmartTrackingVisualSettings VisualSettings;

private:
	void RebindToPawn(APawn* NewPawn);
	void Unbind();
	void OnSmartTrackingTagChanged(const FGameplayTag Tag, int32 NewCount);
	void OnLockStateChanged(const FGameplayEventData* Payload);
	void OnTargetLocked(const FGameplayEventData* Payload);
	void OnTargetLost(const FGameplayEventData* Payload);
	void OnTargetStackChanged(const FGameplayEventData* Payload);
	USmartTrackingReticleWidget* CreateSmartTrackingReticleWidget();
	void RemoveSmartTrackingReticleWidget();
	void PlaySmartTrackingSound(USoundBase* Sound) const;

	TWeakObjectPtr<APlayerController> LocalPlayerController;
	TWeakObjectPtr<UCanvasPanel> WidgetHost;
	TWeakObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;
	UPROPERTY(Transient) TObjectPtr<USmartTrackingReticleWidget> SmartTrackingReticleWidget;
	UPROPERTY(Transient)
	TObjectPtr<USmartTrackingTargetVisualManager> TargetVisualManager;
	bool bSmartTrackingActive = false;
	FDelegateHandle PawnChangedHandle;
	FDelegateHandle TagHandle;
	FDelegateHandle LockStateChangedHandle;
	FDelegateHandle TargetLockedHandle;
	FDelegateHandle TargetLostHandle;
	FDelegateHandle TargetStackChangedHandle;
};
