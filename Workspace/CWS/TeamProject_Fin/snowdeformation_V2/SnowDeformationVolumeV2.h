// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/TextureRenderTarget2D.h"
#include "SnowDeformationTypesV2.h"
#include "SnowDeformationVolumeV2.generated.h"

UENUM(BlueprintType)
enum class ESnowHazardLevelV2 : uint8
{
	Level1 UMETA(DisplayName = "Level 1 (얕은 눈 - 경고)"),
	Level2 UMETA(DisplayName = "Level 2 (깊은 눈 - 전투 제약)"),
	Level3 UMETA(DisplayName = "Level 3 (극한 눈 - 전력질주 불가)")
};

UCLASS()
class TEAMPROJECT_FIN_API ASnowDeformationVolumeV2 : public AActor
{
	GENERATED_BODY()

public:
	ASnowDeformationVolumeV2();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

protected:
	UFUNCTION()
	void OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnOverlapEnd(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

public:
	/** 외부에서 발자국 페이로드를 전달받아 대기열에 추가 */
	void RegisterDeformationPayload(const FSnowDeformationPayloadV2& InPayload);

	/** 특정 위치가 눈 영역 내부에 속하는지 검사 */
	bool IsInsideSnowArea(const FVector& WorldLocation) const;

	/** 3D 월드 좌표 -> 로컬 UV(0.0 ~ 1.0) 변환 */
	bool WorldLocationToUV(const FVector& WorldLocation, FVector2D& OutUV) const;

	/** 눈 표면 최고 높이 (Z 월드 좌표) */
	float GetSnowSurfaceZ() const;

	/** 눈 바닥 높이 (Z 월드 좌표) */
	float GetSnowBaseZ() const;

	/** 눈 두께 반환 (cm) */
	float GetSnowThickness() const { return SnowDepth; }

	/** 모인 페이로드들을 렌더 타깃에 일괄 드로우 (Subsystem에서 매 틱 호출됨) */
	void ProcessDeformationBatch(float DeltaTime);

	/** 현재 읽기용 활성 렌더 타깃 반환 */
	UTextureRenderTarget2D* GetCurrentReadRenderTarget() const { return bIsRTAPrimary ? SnowHeightRT_A : SnowHeightRT_B; }

	/** 현재 쓰기용 렌더 타깃 반환 */
	UTextureRenderTarget2D* GetCurrentWriteRenderTarget() const { return bIsRTAPrimary ? SnowHeightRT_B : SnowHeightRT_A; }

	/** 핑퐁 버퍼 교차 스왑 */
	void SwapPingPongBuffers() { bIsRTAPrimary = !bIsRTAPrimary; }

public:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Snow Deformation|Components")
	TObjectPtr<UBoxComponent> BoundsBox;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Snow Deformation|Components")
	TObjectPtr<UStaticMeshComponent> SnowPlaneMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Snow Deformation|Settings")
	ESnowHazardLevelV2 SnowHazardLevel = ESnowHazardLevelV2::Level1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Snow Deformation|Settings")
	float SnowDepth = 30.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Snow Deformation|Settings")
	float EdgeSoftness = 0.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Snow Deformation|Settings")
	bool bAutoFitBoundsToMesh = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Snow Deformation|Material")
	TObjectPtr<UMaterialInterface> SnowMaterialTemplate;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Snow Deformation|Material")
	TObjectPtr<UMaterialInterface> BrushMaterialTemplate;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Snow Deformation|Material")
	TObjectPtr<UMaterialInterface> ErosionMaterialTemplate;

private:
	void CreatePingPongResources();
	void UpdateMaterialParameters();

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> SnowMID;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> BrushMID;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> ErosionMID;

	UPROPERTY(VisibleAnywhere, Transient, Category = "Snow Deformation|Debug")
	TObjectPtr<UTextureRenderTarget2D> SnowHeightRT_A;

	UPROPERTY(VisibleAnywhere, Transient, Category = "Snow Deformation|Debug")
	TObjectPtr<UTextureRenderTarget2D> SnowHeightRT_B;

	UPROPERTY(VisibleAnywhere, Transient, Category = "Snow Deformation|Debug")
	TObjectPtr<UTextureRenderTarget2D> FinalDeformationRT;

	bool bIsRTAPrimary = true;
	TArray<FSnowDeformationPayloadV2> PendingPayloads;

};

