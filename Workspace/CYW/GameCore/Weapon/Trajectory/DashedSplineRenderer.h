#pragma once

#include "CoreMinimal.h"
#include "Components/SplineMeshComponent.h"
#include "UObject/Object.h"
#include "DashedSplineRenderer.generated.h"

class AActor;
class UMaterialInterface;
class USplineComponent;
class UStaticMesh;

// Per-update inputs; the preview component retains its serialized editor settings.
struct FDashedSplineStyle
{
	UStaticMesh* Mesh = nullptr;
	UMaterialInterface* Material = nullptr;
	float Length = 18.0f;
	float Gap = 22.0f;
	float MoveSpeed = 180.0f;
	FVector2D Scale = FVector2D(0.08f, 0.08f);
	ESplineMeshAxis::Type ForwardAxis = ESplineMeshAxis::Z;
};

// Owns rendering resources only; accepts world positions without weapon dependencies.
UCLASS()
class TEAMPROJECT_FIN_API UDashedSplineRenderer : public UObject
{
	GENERATED_BODY()

public:
	void Initialize(AActor* Owner);
	void Update(const TArray<FVector>& Positions, const FDashedSplineStyle& Style, float WorldTime);
	void Hide() const;
	void Shutdown();

private:
	void EnsureSpline();
	void UpdateSplinePath(const TArray<FVector>& Positions) const;
	void UpdateDashSegments(const FDashedSplineStyle& Style, float WorldTime);
	void EnsureDashSegments(int32 RequiredCount);
	void HideUnusedDashSegments(int32 UsedSegmentCount) const;

	TWeakObjectPtr<AActor> OwnerActor;
	UPROPERTY(Transient)
	TObjectPtr<USplineComponent> PreviewSplineComponent;
	UPROPERTY(Transient)
	TArray<TObjectPtr<USplineMeshComponent>> DashSegments;
};
