// Copyright Epic Games, Inc. All Rights Reserved.

#include "AN_Footstep.h"
#include "Kismet/GameplayStatics.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "../Tags/GameplayTagSystem.h"
#include "UObject/ConstructorHelpers.h"


UAN_Footstep::UAN_Footstep()
{
#if WITH_EDITORONLY_DATA
	NotifyColor = FColor(255, 200, 100, 255);
#endif
static ConstructorHelpers::FObjectFinder<UNiagaraSystem> DefaultSnowVFX(TEXT("/Script/Niagara.NiagaraSystem'/Game/A_Surface_Footstep/Niagara_FX/ParticleSystems/PSN_SnowHeavy_Surface.PSN_SnowHeavy_Surface'"));
	
	if (DefaultSnowVFX.Succeeded())
	{
		SnowVFX = DefaultSnowVFX.Object;
	}

}

void UAN_Footstep::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	if (!MeshComp) return;

	AActor* OwnerActor = MeshComp->GetOwner();
	if (!OwnerActor) return;

	const ACharacter* Character = Cast<ACharacter>(OwnerActor);
	const UCharacterMovementComponent* MovementComponent = Character ? Character->GetCharacterMovement() : nullptr;
	if (!MovementComponent || !MovementComponent->IsMovingOnGround())
	{
		return;
	}

	// 최종적으로 재생할 사운드 포인터
	USoundBase* SoundToPlay = DefaultSound;
	
	bool bFoundSpecialTerrain = false;

	// 1. 눈 지형 검사 (GAS 태그 우선)
	UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(OwnerActor);
	if (ASC && (ASC->HasMatchingGameplayTag(GameplayTagSystem::State_Environment_Snow_Level1) ||
				ASC->HasMatchingGameplayTag(GameplayTagSystem::State_Environment_Snow_Level2) ||
				ASC->HasMatchingGameplayTag(GameplayTagSystem::State_Environment_Snow_Level3)))
	{
		if (SnowSound)
		{
			SoundToPlay = SnowSound;
			bFoundSpecialTerrain = true;
		}

		// 눈 전용 VFX 스폰 처리
		if (SnowVFX)
		{
			FVector FootLocation = MeshComp->GetSocketLocation(FootSocketName);
			
			// 발이 깊은 눈에 묻혀있을 수 있으므로, 무릎 이상 높이(SurfaceTraceHeight)에서 아래로 레이캐스트하여 눈 표면을 정확히 찾음
			FVector TraceStart = FootLocation + FVector(0.f, 0.f, SurfaceTraceHeight);
			FVector TraceEnd = FootLocation - FVector(0.f, 0.f, TraceDistance);
			
			FHitResult SurfaceHit;
			FCollisionQueryParams QueryParams;
			QueryParams.AddIgnoredActor(OwnerActor);

			FVector SpawnLocation = FootLocation; // 기본값
			// 하향 레이캐스트로 지형 표면(Hit)을 찾은 경우 해당 좌표 사용
			if (MeshComp->GetWorld()->LineTraceSingleByChannel(SurfaceHit, TraceStart, TraceEnd, ECC_Visibility, QueryParams))
			{
				SpawnLocation = SurfaceHit.ImpactPoint;
			}

			// 캐릭터가 바라보는 방향을 기준으로 스폰 (뒤로 튀는 눈보라 방향을 맞추기 위함)
			FRotator SpawnRotation = OwnerActor->GetActorRotation();

			UNiagaraFunctionLibrary::SpawnSystemAtLocation(
				MeshComp->GetWorld(),
				SnowVFX,
				SpawnLocation,
				SpawnRotation
			);
		}
	}

	// 2. 바닥 재질 검사 (눈 지형이 아닐 경우)
	if (!bFoundSpecialTerrain && MetalSound)
	{
		FVector StartLocation = MeshComp->GetSocketLocation(FootSocketName);
		// 발 위치에서 아래로 레이캐스트 (약간 위에서 시작)
		FVector TraceStart = StartLocation + FVector(0.f, 0.f, 10.f);
		FVector TraceEnd = TraceStart - FVector(0.f, 0.f, TraceDistance);

		FHitResult HitResult;
		FCollisionQueryParams QueryParams;
		QueryParams.AddIgnoredActor(OwnerActor);
		QueryParams.bReturnPhysicalMaterial = true; // 물리 재질 반환 활성화

		bool bHit = MeshComp->GetWorld()->LineTraceSingleByChannel(HitResult, TraceStart, TraceEnd, ECC_Visibility, QueryParams);
		if (bHit && HitResult.PhysMaterial.IsValid())
		{
			EPhysicalSurface SurfaceType = UPhysicalMaterial::DetermineSurfaceType(HitResult.PhysMaterial.Get());

			// [안내] 프로젝트 세팅에서 Metal이 SurfaceType1 인지 확인하세요
			if (SurfaceType == SurfaceType1) 
			{
				SoundToPlay = MetalSound;
			}
		}
	}

	// 3. 사운드 재생
	if (SoundToPlay)
	{
		UGameplayStatics::PlaySoundAtLocation(
			MeshComp->GetWorld(), 
			SoundToPlay, 
			MeshComp->GetSocketLocation(FootSocketName),
			VolumeMultiplier,
			PitchMultiplier
		);
	}

	// 4. 틱(Tick) 연속 스윕 방식을 사용하기 위해 노티파이 발자국 코드는 제거함 (롤백)
}
