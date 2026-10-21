// Copyright Epic Games, Inc. All Rights Reserved.

#include "SnowDeformationVolumeV2.h"
#include "SnowDeformationComponentV2.h"
#include "SnowDeformationSubsystemV2.h"
#include "Engine/World.h"
#include "UObject/ConstructorHelpers.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "../GameCore/Tags/GameplayTagSystem.h"
#include "CanvasItem.h"
#include "CanvasTypes.h"
#include "Engine/Canvas.h"
#include "RenderingThread.h"
#include "RHICommandList.h"
#include "Kismet/KismetRenderingLibrary.h"

ASnowDeformationVolumeV2::ASnowDeformationVolumeV2()
{
	PrimaryActorTick.bCanEverTick = false;

	BoundsBox = CreateDefaultSubobject<UBoxComponent>(TEXT("BoundsBox"));
	RootComponent = BoundsBox;
	BoundsBox->SetBoxExtent(FVector(2500.0f, 2500.0f, 100.0f));
	BoundsBox->SetCollisionProfileName(TEXT("Trigger")); // [최적화] 캐릭터 오버랩 감지용
	BoundsBox->SetLineThickness(2.0f);
	BoundsBox->ShapeColor = FColor(135, 206, 250);

	SnowPlaneMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SnowPlaneMesh"));
	SnowPlaneMesh->SetupAttachment(BoundsBox);
	SnowPlaneMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	SnowPlaneMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
	SnowPlaneMesh->SetCollisionResponseToChannel(ECC_Camera, ECR_Block);
	SnowPlaneMesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	SnowPlaneMesh->SetCastShadow(true);
	SnowPlaneMesh->bEvaluateWorldPositionOffset = true;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CustomSnowMeshFinder(TEXT("/Game/snow_deformation/V1/SM_Testsnow.SM_Testsnow"));
	if (CustomSnowMeshFinder.Succeeded())
	{
		SnowPlaneMesh->SetStaticMesh(CustomSnowMeshFinder.Object);
	}
	else
	{	
		static ConstructorHelpers::FObjectFinder<UStaticMesh> DefaultPlaneFinder(TEXT("/Engine/BasicShapes/Plane.Plane"));
		if (DefaultPlaneFinder.Succeeded())
		{
			SnowPlaneMesh->SetStaticMesh(DefaultPlaneFinder.Object);
		}
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> GroundMatFinder(TEXT("/Game/snow_deformation/M_SnowGround_V2.M_SnowGround_V2"));
	if (GroundMatFinder.Succeeded())
	{
		SnowMaterialTemplate = GroundMatFinder.Object;
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> BrushMatFinder(TEXT("/Game/snow_deformation/M_SnowBrush_V2.M_SnowBrush_V2"));
	if (BrushMatFinder.Succeeded())
	{
		BrushMaterialTemplate = BrushMatFinder.Object;
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> ErosionMatFinder(TEXT("/Game/snow_deformation/M_SnowErosion.M_SnowErosion"));
	if (ErosionMatFinder.Succeeded())
	{
		ErosionMaterialTemplate = ErosionMatFinder.Object;
	}
}

void ASnowDeformationVolumeV2::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	if (!BoundsBox || !SnowPlaneMesh)
	{
		return;
	}

	SnowPlaneMesh->SetVisibility(true);

	if (bAutoFitBoundsToMesh && SnowPlaneMesh->GetStaticMesh())
	{
		// 커스텀 메시의 바운딩 크기를 가져와서 박스(BoundsBox)를 거기에 자동으로 맞춤
		const FBoxSphereBounds MeshBounds = SnowPlaneMesh->GetStaticMesh()->GetBounds();
		BoundsBox->SetBoxExtent(MeshBounds.BoxExtent);
		
		// 메시는 변형 없이 원본 스케일 유지
		SnowPlaneMesh->SetRelativeScale3D(FVector(1.0f, 1.0f, 1.0f));
		
		// Z축은 사용자가 뷰포트에서 배치한 높이를 유지 (X, Y만 중앙 정렬)
		const FVector CurrentLoc = SnowPlaneMesh->GetRelativeLocation();
		SnowPlaneMesh->SetRelativeLocation(FVector(0.0f, 0.0f, CurrentLoc.Z));
	}
	else if (SnowPlaneMesh->GetStaticMesh())
	{
		// 사용자가 바운드박스 크기를 수동으로 오버라이드하는 모드.
		// 메시는 강제로 스케일링되지 않고 원본 크기(1.0)를 유지합니다.
		SnowPlaneMesh->SetRelativeScale3D(FVector(1.0f, 1.0f, 1.0f));

		// Z축 스케일을 조절하더라도 메시가 박스 바닥으로 끌려가지 않도록 현재 뷰포트 높이(Z)를 보존
		const FVector CurrentLoc = SnowPlaneMesh->GetRelativeLocation();
		SnowPlaneMesh->SetRelativeLocation(FVector(0.0f, 0.0f, CurrentLoc.Z));
	}

	UpdateMaterialParameters();
}

void ASnowDeformationVolumeV2::BeginPlay()
{
	Super::BeginPlay();

	if (BoundsBox)
	{
		BoundsBox->OnComponentBeginOverlap.AddDynamic(this, &ASnowDeformationVolumeV2::OnOverlapBegin);
		BoundsBox->OnComponentEndOverlap.AddDynamic(this, &ASnowDeformationVolumeV2::OnOverlapEnd);
	}

	CreatePingPongResources();
	UpdateMaterialParameters();

	if (UWorld* World = GetWorld())
	{
		if (USnowDeformationSubsystemV2* Subsystem = World->GetSubsystem<USnowDeformationSubsystemV2>())
		{
			Subsystem->RegisterVolume(this);
		}
	}

	// [최적화] 첫 진입 시 셰이더 컴파일/메모리 할당으로 인한 버벅임(Hitch)을 방지하기 위한 워밍업(Warm-up)
	FSnowDeformationPayloadV2 WarmupPayload;
	WarmupPayload.HitLocationWorld = GetActorLocation(); // 볼륨 중앙
	WarmupPayload.RadiusWorld = 0.01f; // 눈에 안 보이게 아주 작게
	WarmupPayload.DepthIntensity = 0.0f; // 파임 없음
	RegisterDeformationPayload(WarmupPayload);
	
	// 가짜 렌더링 1회를 즉시 수행하여 GPU 캐싱 및 파이프라인 초기화를 끝냅니다.
	ProcessDeformationBatch(0.0f);
}

void ASnowDeformationVolumeV2::OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (!OtherActor || OtherActor == this) return;

	if (USnowDeformationComponentV2* SnowComp = OtherActor->FindComponentByClass<USnowDeformationComponentV2>())
	{
		SnowComp->EnableDeformationTick(true);
	}
}

void ASnowDeformationVolumeV2::OnOverlapEnd(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	if (!OtherActor || OtherActor == this) return;

	if (USnowDeformationComponentV2* SnowComp = OtherActor->FindComponentByClass<USnowDeformationComponentV2>())
	{
		SnowComp->EnableDeformationTick(false);
	}
}

void ASnowDeformationVolumeV2::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		if (USnowDeformationSubsystemV2* Subsystem = World->GetSubsystem<USnowDeformationSubsystemV2>())
		{
			Subsystem->UnregisterVolume(this);
		}
	}
	Super::EndPlay(EndPlayReason);
}

void ASnowDeformationVolumeV2::CreatePingPongResources()
{
	int32 Resolution = 512;
	if (BoundsBox)
	{
		// 해상도 자동 비례 스케일링 (황금 텍셀 밀도: 50m(5000cm)당 2048px => 1cm당 0.4096px)
		const float MaxLength = FMath::Max(BoundsBox->GetScaledBoxExtent().X, BoundsBox->GetScaledBoxExtent().Y) * 2.0f;
		float CalculatedRes = MaxLength * 0.4096f;
		
		CalculatedRes = FMath::Clamp(CalculatedRes, 128.0f, 4096.0f);
		Resolution = FMath::RoundUpToPowerOfTwo(FMath::RoundToInt(CalculatedRes));
	}

	SnowHeightRT_A = NewObject<UTextureRenderTarget2D>(this);
	
	// [NEW] 나나이트 테셀레이션 캐시 무효화(깜빡임/성능저하) 방지용 고정 3번째 RT
	FinalDeformationRT = NewObject<UTextureRenderTarget2D>(this);
	
	if (SnowHeightRT_A)
	{
		SnowHeightRT_A->RenderTargetFormat = RTF_RGBA16f;
		SnowHeightRT_A->ClearColor = FLinearColor::Black;
		SnowHeightRT_A->bAutoGenerateMips = false;
		SnowHeightRT_A->bCanCreateUAV = false;
		SnowHeightRT_A->Filter = TF_Bilinear;
		SnowHeightRT_A->AddressX = TA_Clamp;
		SnowHeightRT_A->AddressY = TA_Clamp;
		SnowHeightRT_A->InitAutoFormat(Resolution, Resolution);
		SnowHeightRT_A->UpdateResourceImmediate(true);
		UKismetRenderingLibrary::ClearRenderTarget2D(GetWorld(), SnowHeightRT_A, FLinearColor::Black);
	}

	SnowHeightRT_B = NewObject<UTextureRenderTarget2D>(this);
	if (SnowHeightRT_B)
	{
		SnowHeightRT_B->RenderTargetFormat = RTF_RGBA16f;
		SnowHeightRT_B->ClearColor = FLinearColor::Black;
		SnowHeightRT_B->bAutoGenerateMips = false;
		SnowHeightRT_B->bCanCreateUAV = false;
		SnowHeightRT_B->Filter = TF_Bilinear;
		SnowHeightRT_B->AddressX = TA_Clamp;
		SnowHeightRT_B->AddressY = TA_Clamp;
		SnowHeightRT_B->InitAutoFormat(Resolution, Resolution);
		SnowHeightRT_B->UpdateResourceImmediate(true);
		UKismetRenderingLibrary::ClearRenderTarget2D(GetWorld(), SnowHeightRT_B, FLinearColor::Black);
	}

	FinalDeformationRT = NewObject<UTextureRenderTarget2D>(this);
	if (FinalDeformationRT)
	{
		FinalDeformationRT->RenderTargetFormat = RTF_RGBA16f;
		FinalDeformationRT->ClearColor = FLinearColor::Black;
		FinalDeformationRT->bAutoGenerateMips = false;
		FinalDeformationRT->bCanCreateUAV = false;
		FinalDeformationRT->Filter = TF_Bilinear;
		FinalDeformationRT->AddressX = TA_Clamp;
		FinalDeformationRT->AddressY = TA_Clamp;
		FinalDeformationRT->InitAutoFormat(Resolution, Resolution);
		FinalDeformationRT->UpdateResourceImmediate(true);
		UKismetRenderingLibrary::ClearRenderTarget2D(GetWorld(), FinalDeformationRT, FLinearColor::Black);
	}

	if (BrushMaterialTemplate) BrushMID = UMaterialInstanceDynamic::Create(BrushMaterialTemplate, this);
	if (ErosionMaterialTemplate) ErosionMID = UMaterialInstanceDynamic::Create(ErosionMaterialTemplate, this);

	bIsRTAPrimary = true;
}

void ASnowDeformationVolumeV2::UpdateMaterialParameters()
{
	if (!SnowPlaneMesh) return;

	if (SnowMaterialTemplate)
	{
		if (!SnowMID || SnowMID->Parent != SnowMaterialTemplate)
		{
			SnowMID = UMaterialInstanceDynamic::Create(SnowMaterialTemplate, this);
			SnowPlaneMesh->SetMaterial(0, SnowMID);
		}
	}
	else
	{
		SnowMID = nullptr;
		SnowPlaneMesh->SetMaterial(0, nullptr);
	}

	if (SnowMID && BoundsBox)
	{
		const FVector BoxExtent = BoundsBox->GetScaledBoxExtent();
		SnowMID->SetScalarParameterValue(TEXT("SnowDepth"), SnowDepth);
		SnowMID->SetScalarParameterValue(TEXT("EdgeSoftness"), EdgeSoftness);
		SnowMID->SetScalarParameterValue(TEXT("SnowAreaSize"), BoxExtent.X * 2.0f);
		SnowMID->SetVectorParameterValue(TEXT("SnowAreaCenter"), GetActorLocation());
		SnowMID->SetVectorParameterValue(TEXT("SnowAreaExtent"), FVector(BoxExtent.X * 2.0f, BoxExtent.Y * 2.0f, BoxExtent.Z * 2.0f));

		if (FinalDeformationRT)
		{
			// 머티리얼에는 고정된 3번째 RT만 연결합니다 (나나이트 캐시 유지용)
			SnowMID->SetTextureParameterValue(TEXT("RT_SnowHeight"), FinalDeformationRT);
			SnowMID->SetTextureParameterValue(TEXT("snowHeight"), FinalDeformationRT);
			SnowMID->SetTextureParameterValue(TEXT("SnowHeight"), FinalDeformationRT);
			SnowMID->SetTextureParameterValue(TEXT("RT_Height"), FinalDeformationRT);
			SnowMID->SetTextureParameterValue(TEXT("RT_Snow"), FinalDeformationRT);
			SnowMID->SetTextureParameterValue(TEXT("DeformationRT"), FinalDeformationRT);
			SnowMID->SetTextureParameterValue(TEXT("Texture"), FinalDeformationRT);
		}
	}
}

bool ASnowDeformationVolumeV2::IsInsideSnowArea(const FVector& WorldLocation) const
{
	if (!BoundsBox) return false;
	const FVector BoxExtent = BoundsBox->GetScaledBoxExtent();
	if (BoxExtent.X <= 0.0f || BoxExtent.Y <= 0.0f) return false;

	const FVector LocalPos = GetActorTransform().InverseTransformPosition(WorldLocation);
	if (FMath::Abs(LocalPos.X) > BoxExtent.X || FMath::Abs(LocalPos.Y) > BoxExtent.Y) return false;

	const float MaxLocalZ = BoxExtent.Z + 150.0f;
	const float MinLocalZ = -BoxExtent.Z - 100.0f;
	return (LocalPos.Z >= MinLocalZ && LocalPos.Z <= MaxLocalZ);
}

bool ASnowDeformationVolumeV2::WorldLocationToUV(const FVector& WorldLocation, FVector2D& OutUV) const
{
	if (!BoundsBox) return false;
	const FVector LocalExtent = BoundsBox->GetScaledBoxExtent();
	if (LocalExtent.X <= 0.0f || LocalExtent.Y <= 0.0f) return false;

	const FVector LocalPos = GetActorTransform().InverseTransformPosition(WorldLocation);
	if (FMath::Abs(LocalPos.X) > LocalExtent.X || FMath::Abs(LocalPos.Y) > LocalExtent.Y) return false;

	const float MaxLocalZ = LocalExtent.Z + 150.0f;
	const float MinLocalZ = -LocalExtent.Z - 100.0f;
	if (LocalPos.Z < MinLocalZ || LocalPos.Z > MaxLocalZ) return false;

	OutUV.X = FMath::Clamp((LocalPos.X + LocalExtent.X) / (LocalExtent.X * 2.0f), 0.0f, 1.0f);
	OutUV.Y = FMath::Clamp((LocalPos.Y + LocalExtent.Y) / (LocalExtent.Y * 2.0f), 0.0f, 1.0f);
	return true;
}

float ASnowDeformationVolumeV2::GetSnowSurfaceZ() const
{
	if (!BoundsBox) return 0.0f;
	return GetActorLocation().Z - BoundsBox->GetScaledBoxExtent().Z + SnowDepth;
}

float ASnowDeformationVolumeV2::GetSnowBaseZ() const
{
	if (!BoundsBox) return 0.0f;
	return GetActorLocation().Z - BoundsBox->GetScaledBoxExtent().Z;
}

void ASnowDeformationVolumeV2::RegisterDeformationPayload(const FSnowDeformationPayloadV2& InPayload)
{
	PendingPayloads.Add(InPayload);
}

void ASnowDeformationVolumeV2::ProcessDeformationBatch(float DeltaTime)
{
	bool bEnableErosion = false;
	if (UWorld* World = GetWorld())
	{
		if (USnowDeformationSubsystemV2* Subsystem = World->GetSubsystem<USnowDeformationSubsystemV2>())
		{
			bEnableErosion = Subsystem->bEnableErosion;
		}
	}

	if (PendingPayloads.Num() == 0 && !bEnableErosion) return;

	UTextureRenderTarget2D* ReadRT = GetCurrentReadRenderTarget();
	UTextureRenderTarget2D* WriteRT = GetCurrentWriteRenderTarget();

	if (!ReadRT || !WriteRT || !BrushMID) return;

	FTextureRenderTargetResource* ReadResource = ReadRT->GameThread_GetRenderTargetResource();
	FTextureRenderTargetResource* WriteResource = WriteRT->GameThread_GetRenderTargetResource();

	if (!ReadResource || !WriteResource) return;

	ENQUEUE_RENDER_COMMAND(SnowCopyRTCommand)(
		[ReadResource, WriteResource](FRHICommandListImmediate& RHICmdList)
		{
			FTextureRHIRef SourceRHI = ReadResource->GetRenderTargetTexture();
			FTextureRHIRef TargetRHI = WriteResource->GetRenderTargetTexture();
			if (SourceRHI && TargetRHI)
			{
				RHICmdList.CopyTexture(SourceRHI, TargetRHI, FRHICopyTextureInfo());
			}
		}
	);

	FCanvas DrawCanvas(WriteResource, nullptr, GetWorld(), GetWorld()->FeatureLevel);
	UCanvas* CanvasObj = NewObject<UCanvas>(GetTransientPackage());
	if (CanvasObj)
	{
		CanvasObj->Canvas = &DrawCanvas;
		CanvasObj->SizeX = WriteRT->SizeX;
		CanvasObj->SizeY = WriteRT->SizeY;
		const FVector2D CanvasSize(WriteRT->SizeX, WriteRT->SizeY);

		for (const FSnowDeformationPayloadV2& Payload : PendingPayloads)
		{
			FVector2D HitUV;
			if (!WorldLocationToUV(Payload.HitLocationWorld, HitUV)) continue;

			// World Radius to UV Radius
			const FVector BoxExtent = BoundsBox->GetScaledBoxExtent();
			const float TotalVolumeWidth = BoxExtent.X * 2.0f;
			const float RadiusUV = (TotalVolumeWidth > 0.0f) ? (Payload.RadiusWorld / TotalVolumeWidth) : 0.015f;

			const float PixelX = HitUV.X * CanvasSize.X;
			const float PixelY = HitUV.Y * CanvasSize.Y;
			const float RadiusPx = RadiusUV * CanvasSize.X;
			const float Size = RadiusPx * 2.0f;

			BrushMID->SetScalarParameterValue(TEXT("DepthIntensity"), Payload.DepthIntensity);
			BrushMID->SetTextureParameterValue(TEXT("PreviousRT"), ReadRT);

			const FVector2D ScreenPos(PixelX - RadiusPx, PixelY - RadiusPx);
			const FVector2D ScreenSize(Size, Size);
			
			CanvasObj->K2_DrawMaterial(BrushMID, ScreenPos, ScreenSize, FVector2D::ZeroVector, FVector2D::UnitVector, Payload.RotationAngle, FVector2D(0.5f, 0.5f));
		}
	}

	DrawCanvas.Flush_GameThread(false);
	PendingPayloads.Reset();
	
	// [NEW] 핑퐁 버퍼 스왑 전에, 방금 완성된 WriteRT의 결과물을 3번째 고정 RT로 복사합니다.
	if (FinalDeformationRT)
	{
		FTextureRenderTargetResource* FinalResource = FinalDeformationRT->GameThread_GetRenderTargetResource();
		if (FinalResource)
		{
			ENQUEUE_RENDER_COMMAND(SnowCopyToFinalRTCommand)(
				[WriteResource, FinalResource](FRHICommandListImmediate& RHICmdList)
				{
					FTextureRHIRef SourceRHI = WriteResource->GetRenderTargetTexture();
					FTextureRHIRef TargetRHI = FinalResource->GetRenderTargetTexture();
					if (SourceRHI && TargetRHI)
					{
						RHICmdList.CopyTexture(SourceRHI, TargetRHI, FRHICopyTextureInfo());
					}
				}
			);
		}
	}

	SwapPingPongBuffers();
	
	// 매 프레임마다 머티리얼의 파라미터를 교체하는 코드는 삭제합니다!
	// (UpdateMaterialParameters에서 FinalDeformationRT를 한 번만 연결해 두면 나나이트가 안정적으로 유지됩니다)
}

#if WITH_EDITOR
void ASnowDeformationVolumeV2::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	UpdateMaterialParameters();
}
#endif

