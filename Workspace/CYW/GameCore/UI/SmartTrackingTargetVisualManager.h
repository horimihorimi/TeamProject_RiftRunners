#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "SmartTrackingTargetVisualManager.generated.h"

class AActor;
class APlayerController;
class ASmartTrackingSplineVisualActor;
class UCanvasPanel;
class USmartTrackingLockIndicatorWidget;
class USmartTrackingVisualComponent;
struct FSmartTrackingVisualSettings;

USTRUCT()
struct FSmartTrackingTargetVisual
{
	GENERATED_BODY()

	UPROPERTY(Transient)
	TObjectPtr<AActor> TargetActor;
	UPROPERTY(Transient)
	TObjectPtr<USmartTrackingLockIndicatorWidget> LockWidget;
	UPROPERTY(Transient)
	TObjectPtr<ASmartTrackingSplineVisualActor> SplineActor;
	bool bPendingRemoval = false;
};

// Owned by the visual component; all target widgets and spline actors live here.
UCLASS()
class TEAMPROJECT_FIN_API USmartTrackingTargetVisualManager : public UObject
{
	GENERATED_BODY()

public:
	virtual UWorld* GetWorld() const override;
	void Initialize(USmartTrackingVisualComponent* InOwnerComponent, APlayerController* PlayerController);
	void SetWidgetHost(UCanvasPanel* Host);
	void Shutdown();
	void AddTarget(AActor* TargetActor, const FSmartTrackingVisualSettings& VisualSettings);
	void RemoveTarget(AActor* TargetActor);
	bool SetLockedBulletCount(AActor* TargetActor, int32 Count);
	void NotifyLockLostAnimationFinished(USmartTrackingLockIndicatorWidget* LockWidget);
	void Clear();
	void Update();

private:
	void FinishRemove(int32 TargetIndex);
	void RefreshTickEnabled();
	static void DestroyVisual(FSmartTrackingTargetVisual& Visual, bool bPlayLockLost);
	bool ProjectWorldToHostPosition(const FVector& WorldLocation, FVector2D& OutHostPosition) const;
	FVector GetSmartTrackingFirePoint() const;
	FVector GetSmartTrackingTargetPoint(const AActor* TargetActor) const;
	int32 FindTargetIndex(const AActor* TargetActor) const;

	TWeakObjectPtr<APlayerController> LocalPlayerController;
	TWeakObjectPtr<USmartTrackingVisualComponent> OwnerComponent;
	TWeakObjectPtr<UCanvasPanel> WidgetHost;
	UPROPERTY(Transient)
	TArray<FSmartTrackingTargetVisual> TargetVisuals;
};
