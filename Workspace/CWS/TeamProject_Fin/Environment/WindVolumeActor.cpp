#include "WindVolumeActor.h"
#include "Environment/WindAudioManagerSubsystem.h"
#include "EnvironmentCharacterMovementComponent.h"
#include "GameFramework/Character.h"
#include "Components/BoxComponent.h"
#include "Components/AudioComponent.h"
#include "Net/UnrealNetwork.h"
#include "NiagaraComponent.h"

AWindVolumeActor::AWindVolumeActor()
{
	bReplicates = true;

	WindVolume = CreateDefaultSubobject<UBoxComponent>(TEXT("WindVolume"));
	RootComponent = WindVolume;
	WindVolume->SetBoxExtent(VolumeSize);
	WindVolume->SetCollisionProfileName(TEXT("Trigger"));

	WindParticle = CreateDefaultSubobject<UNiagaraComponent>(TEXT("WindParticle"));
	WindParticle->SetupAttachment(RootComponent);

	WindAudioComponent = CreateDefaultSubobject<UAudioComponent>(TEXT("WindAudioComponent"));
	WindAudioComponent->SetupAttachment(RootComponent);
	WindAudioComponent->bAutoActivate = false;
}

void AWindVolumeActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AWindVolumeActor, bIsWindReversed);
}

void AWindVolumeActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	// [원상복구] 컴포넌트의 Box Extent를 직접 수정하면 언리얼 엔진 구조상 액터의 OnConstruction이 호출되지 않아서 수명이 안 변합니다!
	// 따라서 다시 VolumeSize 변수를 통해 크기를 조절하도록 강제합니다. (그래야 수명 계산 코드가 실행됨)
	if (WindVolume)
	{
		WindVolume->SetBoxExtent(VolumeSize);
	}

	UpdateWindParticleDirection();
}

void AWindVolumeActor::UpdateWindParticleDirection()
{
	if (WindParticle && !GetEffectiveWindVelocity().IsNearlyZero() && WindVolume)
	{
		// [핵심 변경] 기획자님 의도대로 액터를 회전시키면 바람 방향도 같이 회전하도록(Local Space) 변환
		FVector EffectiveWind = GetEffectiveWindVelocity();
		FVector WorldWindVelocity = GetActorTransform().TransformVectorNoScale(EffectiveWind);
		WindParticle->SetWorldRotation(WorldWindVelocity.Rotation());
		
		float Speed = EffectiveWind.Size();
		FVector LocalWindDir = EffectiveWind.GetSafeNormal();

		FVector ScaledVolume = VolumeSize * GetActorScale3D(); // 스케일 툴(R) 크기 반영

		float ExtentAlongWind = FMath::Abs(LocalWindDir.X) * ScaledVolume.X + 
								FMath::Abs(LocalWindDir.Y) * ScaledVolume.Y + 
								FMath::Abs(LocalWindDir.Z) * ScaledVolume.Z;
		
		float Distance = ExtentAlongWind * 2.0f;
		
		// 기획자님 요청사항: 기본 수명이 3배 정도 기니까 나누기 3 적용!
		float CalculatedLifetime = (Distance / Speed) / 4.0f;

		WindParticle->SetFloatParameter(FName("CalculatedLifetime"), CalculatedLifetime);
	}
}

void AWindVolumeActor::OnRep_WindReversed()
{
	UpdateWindParticleDirection();
}

void AWindVolumeActor::BeginPlay()
{
	Super::BeginPlay();

	if (WindAudioComponent)
	{
		WindAudioComponent->Stop();
	}

	if (UWorld* World = GetWorld())
	{
		if (UWindAudioManagerSubsystem* WindAudioManager = World->GetSubsystem<UWindAudioManagerSubsystem>())
		{
			WindAudioManager->RegisterWindVolume(this);
		}
	}
	
	if (WindVolume)
	{
		WindVolume->OnComponentBeginOverlap.AddDynamic(this, &AWindVolumeActor::OnOverlapBegin);
		WindVolume->OnComponentEndOverlap.AddDynamic(this, &AWindVolumeActor::OnOverlapEnd);
	}
}

void AWindVolumeActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		if (UWindAudioManagerSubsystem* WindAudioManager = World->GetSubsystem<UWindAudioManagerSubsystem>())
		{
			WindAudioManager->UnregisterWindVolume(this);
		}
	}

	Super::EndPlay(EndPlayReason);
}

void AWindVolumeActor::OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (ACharacter* Character = Cast<ACharacter>(OtherActor))
	{
		if (UEnvironmentCharacterMovementComponent* EnvMovement = Cast<UEnvironmentCharacterMovementComponent>(Character->GetCharacterMovement()))
		{
			FVector WorldWindVelocity = GetActorTransform().TransformVectorNoScale(GetEffectiveWindVelocity());
			EnvMovement->AddActiveWindForce(WorldWindVelocity, TerminalVelocity);
		}
	}
}

void AWindVolumeActor::OnOverlapEnd(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	if (ACharacter* Character = Cast<ACharacter>(OtherActor))
	{
		if (UEnvironmentCharacterMovementComponent* EnvMovement = Cast<UEnvironmentCharacterMovementComponent>(Character->GetCharacterMovement()))
		{
			FVector WorldWindVelocity = GetActorTransform().TransformVectorNoScale(GetEffectiveWindVelocity());
			EnvMovement->RemoveActiveWindForce(WorldWindVelocity, TerminalVelocity);
		}
	}
}

FVector AWindVolumeActor::GetEffectiveWindVelocity() const
{
	return bIsWindReversed ? (WindVelocity * -1.f) : WindVelocity;
}

void AWindVolumeActor::SetWindReversed(bool bReversed)
{
	if (!HasAuthority() || bIsWindReversed == bReversed)
	{
		return;
	}

	// 1. 역방향 상태 변경
	bIsWindReversed = bReversed;
	ForceNetUpdate();

	// 2. 이미 안에 있는 플레이어들의 기존 힘을 제거하고, 새로운 힘으로 교체
	FVector OldWorldWind = GetActorTransform().TransformVectorNoScale(bIsWindReversed ? WindVelocity : (WindVelocity * -1.f));
	FVector NewWorldWind = GetActorTransform().TransformVectorNoScale(GetEffectiveWindVelocity());

	if (WindVolume)
	{
		TArray<AActor*> OverlappingActors;
		WindVolume->GetOverlappingActors(OverlappingActors, ACharacter::StaticClass());

		for (AActor* Actor : OverlappingActors)
		{
			if (ACharacter* Character = Cast<ACharacter>(Actor))
			{
				if (UEnvironmentCharacterMovementComponent* EnvMovement = Cast<UEnvironmentCharacterMovementComponent>(Character->GetCharacterMovement()))
				{
					EnvMovement->RemoveActiveWindForce(OldWorldWind, TerminalVelocity);
					EnvMovement->AddActiveWindForce(NewWorldWind, TerminalVelocity);
				}
			}
		}
	}

	// 3. 서버의 나이아가라 방향을 즉시 갱신합니다. 클라이언트는 OnRep에서 갱신합니다.
	UpdateWindParticleDirection();
}
