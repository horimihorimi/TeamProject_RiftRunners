#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SmartTrackingSplineVisualActor.generated.h"

class USplineComponent;
class UNiagaraComponent;
class UNiagaraSystem;

UCLASS()
class TEAMPROJECT_FIN_API ASmartTrackingSplineVisualActor : public AActor
{
	GENERATED_BODY()

public:
	ASmartTrackingSplineVisualActor();

	void InitializeVisual(AActor* InSourceActor, AActor* InTargetActor);
	void UpdateVisual(
		const FVector& StartLocation,
		const FVector& EndLocation,
		float CurveHeight,
		float CurveSideOffset);
	void HideVisual();

	AActor* GetTargetActor() const { return TrackedTargetActor.Get(); }

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Smart Tracking")
	TObjectPtr<USplineComponent> SplineComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Smart Tracking")
	TObjectPtr<UNiagaraComponent> NiagaraComponent;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Smart Tracking")
	TObjectPtr<UNiagaraSystem> NiagaraSystem;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Smart Tracking")
	FLinearColor LineColor = FLinearColor(1.0f, 0.45f, 0.0f, 1.0f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Smart Tracking", meta = (ClampMin = "0.1"))
	float LineThickness = 1.5f;

	UFUNCTION(BlueprintImplementableEvent, Category = "Smart Tracking")
	void OnVisualShown(AActor* VisualSourceActor, AActor* VisualTargetActor);

	UFUNCTION(BlueprintImplementableEvent, Category = "Smart Tracking")
	void OnVisualHidden(AActor* VisualSourceActor, AActor* VisualTargetActor);

private:
	void UpdateNiagaraParameters();

	TWeakObjectPtr<AActor> TrackedSourceActor;
	TWeakObjectPtr<AActor> TrackedTargetActor;
	float CurveSideDirection = 1.0f;
};
