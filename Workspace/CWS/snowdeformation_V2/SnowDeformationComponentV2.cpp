// Copyright Epic Games, Inc. All Rights Reserved.

#include "SnowDeformationComponentV2.h"
#include "SnowDeformationSubsystemV2.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "Kismet/GameplayStatics.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "SnowDeformationVolumeV2.h"
#include "../GameCore/Tags/GameplayTagSystem.h"
#include "AbilitySystemGlobals.h"
#include "AbilitySystemComponent.h"

USnowDeformationComponentV2::USnowDeformationComponentV2()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false; // [최적화] 눈 볼륨과 겹치기 전까지는 틱(매 프레임 연산)을 끕니다.
	PrimaryComponentTick.TickGroup = TG_PrePhysics;

	bDrawDebug = true;

	FSnowTrackedSocketV2 LeftFoot;
	LeftFoot.SocketName = TEXT("foot_l");
	LeftFoot.RadiusWorld = 150.0f;
	LeftFoot.DepthMultiplier = 0.7f;
	TrackedSockets.Add(LeftFoot);

	FSnowTrackedSocketV2 RightFoot;
	RightFoot.SocketName = TEXT("foot_r");
	RightFoot.RadiusWorld = 150.0f;
	RightFoot.DepthMultiplier = 0.7f;
	TrackedSockets.Add(RightFoot);

	FSnowTrackedSocketV2 LeftCalf;
	LeftCalf.SocketName = TEXT("calf_l");
	LeftCalf.RadiusWorld = 120.0f;
	LeftCalf.DepthMultiplier = 0.4f;
	TrackedSockets.Add(LeftCalf);

	FSnowTrackedSocketV2 RightCalf;
	RightCalf.SocketName = TEXT("calf_r");
	RightCalf.RadiusWorld = 120.0f;
	RightCalf.DepthMultiplier = 0.4f;
	TrackedSockets.Add(RightCalf);

	bDrawDebug = false;
	MinStrokeDistance = 3.0f;
}

void USnowDeformationComponentV2::BeginPlay()
{
	Super::BeginPlay();

	if (IsRunningDedicatedServer())
	{
		SetComponentTickEnabled(false);
		return;
	}

	CacheSkeletalMesh();

	if (UWorld* World = GetWorld())
	{
		CachedSubsystem = World->GetSubsystem<USnowDeformationSubsystemV2>();
	}

	ResetTrackedSockets();
}

void USnowDeformationComponentV2::CacheSkeletalMesh()
{
	if (AActor* OwnerActor = GetOwner())
	{
		if (ACharacter* Character = Cast<ACharacter>(OwnerActor))
		{
			CachedMeshComp = Character->GetMesh();
		}
		else
		{
			CachedMeshComp = OwnerActor->FindComponentByClass<USkeletalMeshComponent>();
		}
	}
}

void USnowDeformationComponentV2::ResetTrackedSockets()
{
	for (FSnowTrackedSocketV2& Socket : TrackedSockets)
	{
		Socket.bHasValidPrevLocation = false;
		Socket.PreviousLocation = FVector::ZeroVector;
	}
}

bool USnowDeformationComponentV2::IsWithinCullDistance() const
{
	UWorld* World = GetWorld();
	if (!World) return false;

	APlayerCameraManager* CameraManager = UGameplayStatics::GetPlayerCameraManager(World, 0);
	if (!CameraManager) return true;

	const FVector CameraLoc = CameraManager->GetCameraLocation();
	const FVector ActorLoc = GetOwner() ? GetOwner()->GetActorLocation() : FVector::ZeroVector;

	const float DistSq = FVector::DistSquared(CameraLoc, ActorLoc);
	return DistSq <= FMath::Square(MaxCameraDistance);
}

void USnowDeformationComponentV2::ProcessContinuousSweeps(float DeltaTime)
{
	if (!CachedMeshComp)
	{
		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 0.0f, FColor::Red, TEXT("[Snow Error] CachedMeshComp is NULL!"));
		return;
	}

	if (!IsWithinCullDistance())
	{
		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 0.0f, FColor::Red, TEXT("[Snow Error] Out of Cull Distance!"));
		return;
	}

	if (!CachedSubsystem)
	{
		if (UWorld* World = GetWorld())
		{
			CachedSubsystem = World->GetSubsystem<USnowDeformationSubsystemV2>();
		}
	}

	if (!CachedSubsystem) return;

	for (FSnowTrackedSocketV2& Socket : TrackedSockets)
	{
		if (!CachedMeshComp->DoesSocketExist(Socket.SocketName))
		{
			if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 0.0f, FColor::Red, FString::Printf(TEXT("[Snow Error] Socket Missing: %s"), *Socket.SocketName.ToString()));
			continue;
		}

		const FVector CurrentLoc = CachedMeshComp->GetSocketLocation(Socket.SocketName);

		if (!Socket.bHasValidPrevLocation)
		{
			Socket.PreviousLocation = CurrentLoc;
			Socket.bHasValidPrevLocation = true;
			continue;
		}

		const FVector PrevLoc = Socket.PreviousLocation;
		const float MoveDist = FVector::Dist(PrevLoc, CurrentLoc);

		if (MoveDist > MaxTeleportDistance)
		{
			Socket.PreviousLocation = CurrentLoc;
			continue;
		}

		if (MoveDist < MinStrokeDistance) continue;

		const float StepSize = FMath::Max(8.0f, Socket.RadiusWorld * 0.25f);
		const int32 NumSubSteps = FMath::Clamp(FMath::CeilToInt(MoveDist / StepSize), 1, 6);
		const FVector Velocity = (DeltaTime > 0.0f) ? ((CurrentLoc - PrevLoc) / DeltaTime) : FVector::ZeroVector;

		for (int32 StepIdx = 0; StepIdx < NumSubSteps; ++StepIdx)
		{
			const float Alpha = (float)(StepIdx + 1) / (float)NumSubSteps;
			const FVector SamplePos = FMath::Lerp(PrevLoc, CurrentLoc, Alpha);

			FHitResult HitResult;
			FCollisionQueryParams QueryParams;
			QueryParams.AddIgnoredActor(GetOwner());
			QueryParams.bReturnPhysicalMaterial = true;
			QueryParams.bTraceComplex = true;

			const FVector TraceStart = SamplePos + FVector(0.0f, 0.0f, 200.0f);
			const FVector TraceEnd = SamplePos - FVector(0.0f, 0.0f, 150.0f);
			
			bool bHit = GetWorld()->LineTraceSingleByChannel(HitResult, TraceStart, TraceEnd, ECC_Visibility, QueryParams);
			
			ASnowDeformationVolumeV2* HitVolume = Cast<ASnowDeformationVolumeV2>(HitResult.GetActor());
			if (!bHit || !HitVolume) continue;

			// [경사로 보완 로직]
			// 고정된 평면 높이(GetSnowSurfaceZ)를 구하는 대신, 트레이스로 닿은 '실제 눈 메쉬 표면'과 '발바닥'의 거리를 계산합니다.
			const float DistanceFromFootToGround = HitResult.Location.Z - SamplePos.Z;

			// 캐릭터 캡슐 충돌로 발이 허공에 살짝(최대 15cm) 떠 있더라도 밟힌 것으로 인정
			if (DistanceFromFootToGround < -15.0f) continue;

			float DepthRatio = 0.5f; // 기본 파임 강도
			if (DistanceFromFootToGround > 0.0f)
			{
				DepthRatio = FMath::Clamp(0.5f + (DistanceFromFootToGround / 20.0f), 0.5f, 1.0f); 
			}

			float SpeedMultiplier = 1.0f;
			if (Velocity.SizeSquared2D() > 28000.0f) SpeedMultiplier = 2.5f;

			DepthRatio = FMath::Clamp(DepthRatio * SpeedMultiplier * Socket.DepthMultiplier, 0.0f, 1.0f);
			if (DepthRatio <= 0.01f) continue;

			float FootAngle = GetOwner() ? GetOwner()->GetActorRotation().Yaw : 0.0f;
			if (Velocity.SizeSquared2D() > 100.0f)
			{
				FootAngle = FMath::RadiansToDegrees(FMath::Atan2(Velocity.Y, Velocity.X));
			}

			FSnowDeformationPayloadV2 Payload;
			Payload.HitLocationWorld = SamplePos;
			Payload.RadiusWorld = Socket.RadiusWorld;
			Payload.StrokeLength = 0.0f;
			Payload.RotationAngle = FootAngle;
			Payload.DepthIntensity = DepthRatio;
			Payload.Velocity = Velocity;
			Payload.bIsContinuousStroke = false;

			HitVolume->RegisterDeformationPayload(Payload);
		}
		Socket.PreviousLocation = CurrentLoc;
	}
}

void USnowDeformationComponentV2::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	ProcessContinuousSweeps(DeltaTime);
	UpdateHazardState();
}

void USnowDeformationComponentV2::TriggerFootstep(FName SocketName)
{
	if (!IsWithinCullDistance() || !CachedMeshComp) return;

	if (!CachedSubsystem)
	{
		if (UWorld* World = GetWorld()) CachedSubsystem = World->GetSubsystem<USnowDeformationSubsystemV2>();
	}

	if (!CachedSubsystem) return;
	if (!CachedMeshComp->DoesSocketExist(SocketName)) return;

	const FVector SocketLoc = CachedMeshComp->GetSocketLocation(SocketName);

	// --- PM 기반 바닥 검사 복구 ---
	FHitResult HitResult;
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(GetOwner());
	QueryParams.bReturnPhysicalMaterial = true;
	QueryParams.bTraceComplex = true; // 실제 폴리곤 타격 강제

	// 눈 깊이를 고려해 넉넉한 높이에서 라인트레이스를 쏩니다.
	const FVector TraceStart = SocketLoc + FVector(0.0f, 0.0f, 200.0f);
	const FVector TraceEnd = SocketLoc - FVector(0.0f, 0.0f, 150.0f);
	
	bool bHit = GetWorld()->LineTraceSingleByChannel(HitResult, TraceStart, TraceEnd, ECC_Visibility, QueryParams);
	
	ASnowDeformationVolumeV2* HitVolume = Cast<ASnowDeformationVolumeV2>(HitResult.GetActor());
	if (!bHit || !HitVolume)
	{
		return; 
	}
	// ---------------------------------
	// [경사로 보완 로직]
	// 블루프린트 컴포넌트 세팅에 등록된 소켓 크기 가져오기
	float FootRadius = 30.0f;
	float DepthMult = 1.0f;
	for (const FSnowTrackedSocketV2& TrackedSocket : TrackedSockets)
	{
		if (TrackedSocket.SocketName == SocketName)
		{
			FootRadius = TrackedSocket.RadiusWorld;
			DepthMult = TrackedSocket.DepthMultiplier;
			break;
		}
	}

	const float DistanceFromFootToGround = HitResult.Location.Z - SocketLoc.Z;

	// 발이 15cm 이상 떠 있으면 무시
	if (DistanceFromFootToGround < -15.0f) return;

	float DepthRatio = 0.5f; 
	if (DistanceFromFootToGround > 0.0f)
	{
		DepthRatio = FMath::Clamp(0.5f + (DistanceFromFootToGround / 20.0f), 0.5f, 1.0f); 
	}

	DepthRatio = FMath::Clamp(DepthRatio * DepthMult, 0.0f, 1.0f);
	if (DepthRatio <= 0.01f) return;

	const float FootAngle = GetOwner() ? GetOwner()->GetActorRotation().Yaw : 0.0f;

	FSnowDeformationPayloadV2 Payload;
	Payload.HitLocationWorld = SocketLoc;
	Payload.RadiusWorld = FootRadius; 
	Payload.StrokeLength = 0.0f;
	Payload.RotationAngle = FootAngle;
	Payload.DepthIntensity = DepthRatio;
	Payload.Velocity = GetOwner() ? GetOwner()->GetVelocity() : FVector::ZeroVector;
	Payload.bIsContinuousStroke = false;

	HitVolume->RegisterDeformationPayload(Payload);
}

void USnowDeformationComponentV2::ApplyDeformationAtLocation(const FVector& WorldLocation, float InRadiusWorld, float InIntensity, const FVector& InVelocity)
{
	if (!CachedSubsystem)
	{
		if (UWorld* World = GetWorld()) CachedSubsystem = World->GetSubsystem<USnowDeformationSubsystemV2>();
	}

	if (!CachedSubsystem) return;

	float SnowSurfaceZ = 0.0f;
	float SnowThickness = 1.0f;
	if (CachedSubsystem->GetSnowSurfaceZAtLocation(WorldLocation, SnowSurfaceZ, SnowThickness))
	{
		FSnowDeformationPayloadV2 Payload;
		Payload.HitLocationWorld = WorldLocation;
		Payload.RadiusWorld = InRadiusWorld;
		Payload.StrokeLength = 0.0f;
		Payload.RotationAngle = 0.0f;
		Payload.DepthIntensity = InIntensity;
		Payload.Velocity = InVelocity;
		Payload.bIsContinuousStroke = false;

		CachedSubsystem->RegisterDeformationPayload(Payload);
	}
}

void USnowDeformationComponentV2::UpdateHazardState()
{
	if (!GetOwner()) return;

	ASnowDeformationVolumeV2* FoundVolume = nullptr;

	FHitResult HitResult;
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(GetOwner());
	QueryParams.bReturnPhysicalMaterial = true;
	QueryParams.bTraceComplex = true;

	const FVector TraceStart = GetOwner()->GetActorLocation();
	const FVector TraceEnd = TraceStart - FVector(0.0f, 0.0f, 200.0f); // 캡슐 중앙에서 바닥으로 2m 검사
	
	if (GetWorld()->LineTraceSingleByChannel(HitResult, TraceStart, TraceEnd, ECC_Visibility, QueryParams))
	{
		FoundVolume = Cast<ASnowDeformationVolumeV2>(HitResult.GetActor());
	}

	if (FoundVolume == CurrentHazardVolume) return;

	UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetOwner());
	if (!ASC) return;

	// 기존 디버프 태그 제거
	if (CurrentHazardVolume)
	{
		FGameplayTag TagToRemove;
		switch (CurrentHazardVolume->SnowHazardLevel)
		{
		case ESnowHazardLevelV2::Level1: TagToRemove = GameplayTagSystem::State_Environment_Snow_Level1; break;
		case ESnowHazardLevelV2::Level2: TagToRemove = GameplayTagSystem::State_Environment_Snow_Level2; break;
		case ESnowHazardLevelV2::Level3: TagToRemove = GameplayTagSystem::State_Environment_Snow_Level3; break;
		}
		ASC->RemoveLooseGameplayTag(TagToRemove);
	}

	// 새 디버프 태그 부여
	if (FoundVolume)
	{
		FGameplayTag TagToAdd;
		switch (FoundVolume->SnowHazardLevel)
		{
		case ESnowHazardLevelV2::Level1: TagToAdd = GameplayTagSystem::State_Environment_Snow_Level1; break;
		case ESnowHazardLevelV2::Level2: TagToAdd = GameplayTagSystem::State_Environment_Snow_Level2; break;
		case ESnowHazardLevelV2::Level3: TagToAdd = GameplayTagSystem::State_Environment_Snow_Level3; break;
		}
		ASC->AddLooseGameplayTag(TagToAdd);
	}

	CurrentHazardVolume = FoundVolume;
}

void USnowDeformationComponentV2::EnableDeformationTick(bool bEnable)
{
	if (bEnable)
	{
		OverlappedVolumeCount++;
	}
	else
	{
		OverlappedVolumeCount = FMath::Max(0, OverlappedVolumeCount - 1);
	}

	const bool bShouldTick = (OverlappedVolumeCount > 0);
	if (PrimaryComponentTick.bStartWithTickEnabled != bShouldTick)
	{
		SetComponentTickEnabled(bShouldTick);
		
		// 틱이 꺼질 때 현재 소켓의 위치 캐시 초기화 (다시 틱이 켜질 때 텔레포트 판정 방지)
		if (!bShouldTick)
		{
			for (FSnowTrackedSocketV2& Socket : TrackedSockets)
			{
				Socket.PreviousLocation = FVector::ZeroVector;
				Socket.bHasValidPrevLocation = false;
			}
			
			// 틱이 꺼질 때는 디버프도 즉시 제거
			if (CurrentHazardVolume && GetOwner())
			{
				if (UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetOwner()))
				{
					FGameplayTag TagToRemove;
					switch (CurrentHazardVolume->SnowHazardLevel)
					{
					case ESnowHazardLevelV2::Level1: TagToRemove = GameplayTagSystem::State_Environment_Snow_Level1; break;
					case ESnowHazardLevelV2::Level2: TagToRemove = GameplayTagSystem::State_Environment_Snow_Level2; break;
					case ESnowHazardLevelV2::Level3: TagToRemove = GameplayTagSystem::State_Environment_Snow_Level3; break;
					}
					ASC->RemoveLooseGameplayTag(TagToRemove);
				}
				CurrentHazardVolume = nullptr;
			}
		}
	}
}

