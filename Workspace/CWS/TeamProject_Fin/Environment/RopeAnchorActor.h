#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RopeAnchorActor.generated.h"

class UBoxComponent;
class UProceduralMeshComponent;
class ACharacter;

UCLASS()
class TEAMPROJECT_FIN_API ARopeAnchorActor : public AActor
{
	GENERATED_BODY()
	
public:	
	ARopeAnchorActor();

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	virtual void OnConstruction(const FTransform& Transform) override;

	UFUNCTION()
	void OnOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, 
						UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, 
						bool bFromSweep, const FHitResult& SweepResult);

public:	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USceneComponent* DefaultRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UBoxComponent* TriggerBox;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UProceduralMeshComponent* ProceduralMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rope")
	float RopeLength = 500.0f;

	// 로프 트리거 영역의 두께 (X, Y 크기)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rope")
	FVector2D TriggerThickness = FVector2D(100.f, 100.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rope")
	int32 NumSegments = 30;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rope")
	int32 ConstraintIterations = 8;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rope")
	float RopeThickness = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rope")
	int32 Sides = 6;

	// 캐릭터 몸통 중심에서 밧줄을 잡는 손 위치 오프셋 (X:앞뒤, Y:좌우, Z:높이)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rope")
	FVector HandOffset = FVector(30.0f, 0.0f, 90.0f);

	// 로프 외형 머티리얼 (색상이나 질감 변경용)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rope|Visual")
	UMaterialInterface* RopeMaterial;

	// Interface for CCMC
	void GrabRope(ACharacter* InCharacter, float InGrabRatio);
	void ReleaseRope(ACharacter* InCharacter);

	// AnimBP IK 연동용
	UFUNCTION(BlueprintPure, Category = "Rope")
	FVector GetGrabbedParticleLocation() const;

private:
	// Verlet State
	TArray<FVector> Particles;
	TArray<FVector> OldParticles;
	float SegmentLength;
	
	// Grab State
	ACharacter* GrabbingCharacter;
	int32 GrabIndexK;

	// PMC Buffers
	TArray<FVector> Vertices;
	TArray<int32> Triangles;
	TArray<FVector> Normals;
	TArray<FVector2D> UVs;

	void SimulateVerletPhysics(float DeltaTime);
	void UpdateMeshExtrusion();
};
