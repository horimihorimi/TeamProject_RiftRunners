// Copyright Epic Games, Inc. All Rights Reserved.

#include "WindSwitchButton.h"
#include "Components/BoxComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameCore/Character/PlayerCharacter.h"
#include "Environment/WindVolumeActor.h"
#include "PCGMap/MapBuild/SectorBuild/Event/PlacedEvent/ChangeMaterialColorEvent.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

AWindSwitchButton::AWindSwitchButton()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;

	TriggerBox = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerBox"));
	RootComponent = TriggerBox;
	TriggerBox->SetBoxExtent(FVector(100.f, 100.f, 50.f));
	TriggerBox->SetCollisionProfileName(TEXT("Trigger"));

	SwitchBaseMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("SwitchBaseMesh"));
	SwitchBaseMesh->SetupAttachment(RootComponent);
	SwitchBaseMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	SwitchButtonMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("SwitchButtonMesh"));
	SwitchButtonMesh->SetupAttachment(SwitchBaseMesh); // 베이스 컴포넌트 하위에 부착
	SwitchButtonMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void AWindSwitchButton::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AWindSwitchButton, bIsPressed);
}

void AWindSwitchButton::BeginPlay()
{
	Super::BeginPlay();
	
	if (TriggerBox)
	{
		TriggerBox->OnComponentBeginOverlap.AddDynamic(this, &AWindSwitchButton::OnOverlapBegin);
		TriggerBox->OnComponentEndOverlap.AddDynamic(this, &AWindSwitchButton::OnOverlapEnd);
	}

	if (SwitchButtonMesh)
	{
		ButtonOriginalLocation = SwitchButtonMesh->GetRelativeLocation();
	}

	if (SwitchBaseMesh && SwitchBaseMesh->GetNumMaterials() > 0)
	{
		// 0번 머티리얼 슬롯을 가져와 다이내믹 머티리얼 인스턴스로 변경 (에미시브 조절용)
		BaseDynamicMaterial = SwitchBaseMesh->CreateAndSetMaterialInstanceDynamic(0);
	}
}

void AWindSwitchButton::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// 애니메이션: 목표 알파값 설정 (눌림=1, 떼짐=0)
	float TargetAlpha = bIsPressed ? 1.0f : 0.0f;
	
	// FInterpTo를 사용하여 부드럽게 전환
	CurrentPressAlpha = FMath::FInterpTo(CurrentPressAlpha, TargetAlpha, DeltaTime, AnimationSpeed);

	// 1. 물리적 버튼 높이 애니메이션 적용
	if (SwitchButtonMesh)
	{
		FVector NewLoc = ButtonOriginalLocation;
		NewLoc.Z += FMath::Lerp(0.0f, PressedZOffset, CurrentPressAlpha);
		SwitchButtonMesh->SetRelativeLocation(NewLoc);
	}

	// 2. 머티리얼 에미시브(발광) 애니메이션 적용
	if (BaseDynamicMaterial)
	{
		float CurrentEmissive = FMath::Lerp(EmissiveOffStrength, EmissiveOnStrength, CurrentPressAlpha);
		BaseDynamicMaterial->SetScalarParameterValue(EmissiveParamName, CurrentEmissive);
	}
}

void AWindSwitchButton::OnRep_IsPressed()
{
	AChangeMaterialColorEvent* MaterialColorEvent = bIsPressed
		? TargetMaterialColorEvent
		: ReleaseMaterialColorEvent;

	if (MaterialColorEvent)
	{
		MaterialColorEvent->ApplyMaterialColorChanges();
	}
}

void AWindSwitchButton::MulticastPlaySwitchSound_Implementation(bool bPressed)
{
	USoundBase* SwitchSound = bPressed ? PressSound : ReleaseSound;
	if (SwitchSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, SwitchSound, GetActorLocation());
	}
}

void AWindSwitchButton::OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (!HasAuthority())
	{
		return;
	}

	if (APlayerCharacter* PlayerCharacter = Cast<APlayerCharacter>(OtherActor))
	{
		// 처음 눌렸을 때만 실행 (중복 컴포넌트 충돌 방지)
		if (!bIsPressed)
		{
			bIsPressed = true;
			ForceNetUpdate();

			MulticastPlaySwitchSound(true);

			for (AWindVolumeActor* WindVolume : TargetWindVolumes)
			{
				if (WindVolume)
				{
					WindVolume->SetWindReversed(true); // 바람 역방향으로 전환
				}
			}

			if (TargetMaterialColorEvent)
			{
				TargetMaterialColorEvent->Execute(this);
			}

			// 체크포인트 등록
			PlayerCharacter->SetCheckpoint(GetActorLocation() + FVector(0.f, 0.f, 100.f)); // 살짝 위쪽으로 지정

			RunPressEvents();
		}
	}
}

void AWindSwitchButton::RunPressEvents()
{
	for (const FTimedEventGroup& Group : PressEvents)
	{
		if (Group.Delay <= 0.f)
		{
			ExecutePressEventGroup(Group.AllEvents());
			continue;
		}

		// 버튼에서 내려와도 예약된 이벤트는 실행한다. 버튼이 파괴되면 호출 대상은 무효화된다.
		FTimerHandle Handle;
		GetWorldTimerManager().SetTimer(Handle,
			FTimerDelegate::CreateUObject(this, &AWindSwitchButton::ExecutePressEventGroup, Group.AllEvents()),
			Group.Delay, false);
	}
}

void AWindSwitchButton::ExecutePressEventGroup(TArray<TScriptInterface<ISectorEvent>> Events)
{
	if (!HasAuthority())
	{
		return;
	}

	for (const TScriptInterface<ISectorEvent>& Event : Events)
	{
		ISectorEvent* Impl = Event.GetInterface();
		if (!IsValid(Event.GetObject()) || !Impl)
		{
			continue;
		}

		Impl->Execute(this);
	}
}

void AWindSwitchButton::OnOverlapEnd(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	if (!HasAuthority())
	{
		return;
	}

	if (APlayerCharacter* PlayerCharacter = Cast<APlayerCharacter>(OtherActor))
	{
		TArray<AActor*> OverlappingActors;
		TriggerBox->GetOverlappingActors(OverlappingActors, APlayerCharacter::StaticClass());

		// 더 이상 오버랩 중인 플레이어가 없을 때만 복구
		if (OverlappingActors.Num() == 0 && bIsPressed)
		{
			bIsPressed = false;
			ForceNetUpdate();

			MulticastPlaySwitchSound(false);

			// 마지막 플레이어가 내려갔을 때 바람 원래대로 복구
			for (AWindVolumeActor* WindVolume : TargetWindVolumes)
			{
				if (WindVolume)
				{
					WindVolume->SetWindReversed(false);
				}
			}

			if (ReleaseMaterialColorEvent)
			{
				ReleaseMaterialColorEvent->Execute(this);
			}
		}
	}
}
