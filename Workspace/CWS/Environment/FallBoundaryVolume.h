// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FallBoundaryVolume.generated.h"

class UBoxComponent;
class UMaterialInterface;
class UStaticMeshComponent;

UCLASS()
class TEAMPROJECT_FIN_API AFallBoundaryVolume : public AActor
{
	GENERATED_BODY()
	
public:	
	AFallBoundaryVolume();

protected:
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;

	UFUNCTION()
	void OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

public:
	// 낙사 판정 구역
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UBoxComponent* BoundaryBox;

private:
	// 낙하 한 번당 최대 체력에서 차감할 비율이다. 체력 하한선은 별도로 20%를 유지한다.
	UPROPERTY(EditAnywhere, Category = "Fall Boundary|Penalty", meta = (ClampMin = "0.0", ClampMax = "100.0", UIMin = "0.0", UIMax = "100.0", Units = "Percent"))
	float HealthPenaltyPercent = 20.0f;

	// 지정하지 않으면 경계 영역을 표시하지 않는다.
	UPROPERTY(EditAnywhere, Category = "Fall Boundary|Visual")
	TObjectPtr<UMaterialInterface> BoundaryMaterial;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UStaticMeshComponent> BoundaryVisual;

	void UpdateBoundaryVisual();
};
