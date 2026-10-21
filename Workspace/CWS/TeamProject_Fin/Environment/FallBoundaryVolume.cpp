// Copyright Epic Games, Inc. All Rights Reserved.

#include "FallBoundaryVolume.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameCore/Attribute/CharacterAttributeSet.h"
#include "GameCore/Character/PlayerCharacter.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

AFallBoundaryVolume::AFallBoundaryVolume()
{
	PrimaryActorTick.bCanEverTick = false;

	BoundaryBox = CreateDefaultSubobject<UBoxComponent>(TEXT("BoundaryBox"));
	RootComponent = BoundaryBox;
	BoundaryBox->SetBoxExtent(FVector(5000.f, 5000.f, 100.f)); // 기본적으로 넓고 얇게 생성
	BoundaryBox->SetCollisionProfileName(TEXT("Trigger"));

	BoundaryVisual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BoundaryVisual"));
	BoundaryVisual->SetupAttachment(BoundaryBox);
	BoundaryVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BoundaryVisual->SetGenerateOverlapEvents(false);
	BoundaryVisual->SetCanEverAffectNavigation(false);
	BoundaryVisual->SetCastShadow(false);
	BoundaryVisual->SetVisibility(false);
	BoundaryVisual->SetHiddenInGame(true);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		BoundaryVisual->SetStaticMesh(CubeMesh.Object);
	}
}

void AFallBoundaryVolume::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	UpdateBoundaryVisual();
}

void AFallBoundaryVolume::UpdateBoundaryVisual()
{
	if (!BoundaryBox || !BoundaryVisual)
	{
		return;
	}

	// 기본 큐브의 한 변은 100cm이므로 박스의 반쪽 크기를 50으로 나눠 전체 크기를 맞춘다.
	BoundaryVisual->SetRelativeLocation(FVector::ZeroVector);
	BoundaryVisual->SetRelativeRotation(FRotator::ZeroRotator);
	BoundaryVisual->SetRelativeScale3D(BoundaryBox->GetUnscaledBoxExtent() / 50.0f);
	BoundaryVisual->SetMaterial(0, BoundaryMaterial.Get());

	const bool bShowBoundary = BoundaryMaterial != nullptr;
	BoundaryVisual->SetVisibility(bShowBoundary);
	BoundaryVisual->SetHiddenInGame(!bShowBoundary);
}

void AFallBoundaryVolume::BeginPlay()
{
	Super::BeginPlay();
	UpdateBoundaryVisual();
	
	if (BoundaryBox)
	{
		BoundaryBox->OnComponentBeginOverlap.AddDynamic(this, &AFallBoundaryVolume::OnOverlapBegin);
	}
}

void AFallBoundaryVolume::OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	APlayerCharacter* PlayerCharacter = Cast<APlayerCharacter>(OtherActor);
	if (!PlayerCharacter || !PlayerCharacter->HasAuthority() || OtherComp != PlayerCharacter->GetCapsuleComponent())
	{
		return;
	}

	if (UCharacterAttributeSet* Attributes = PlayerCharacter->GetCharacterAttributeSet())
	{
		const float CurrentHealth = Attributes->GetHealth();
		const float MaxHealth = Attributes->GetMaxHealth();
		const float MinimumHealth = MaxHealth * 0.2f;
		if (MaxHealth > 0.0f && CurrentHealth > MinimumHealth)
		{
			// 액터별 설정 비율을 차감하되, 낙하 패널티로 체력이 20% 아래로 내려가지는 않는다.
			const float Penalty = MaxHealth * FMath::Clamp(HealthPenaltyPercent, 0.0f, 100.0f) / 100.0f;
			if (Penalty > 0.0f)
			{
				Attributes->SetHealth(FMath::Max(CurrentHealth - Penalty, MinimumHealth));
			}
		}
	}

	// 체력 차감 후 마지막 체크포인트로 즉시 이동한다.
	PlayerCharacter->RespawnAtCheckpoint();
}
