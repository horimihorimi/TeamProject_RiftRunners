// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AN_Footstep.generated.h"

class USoundBase;

/**
 * 발소리 전용 커스텀 애니메이션 노티파이
 * 1순위: 캐릭터가 'State.Environment.Snow' 태그를 가지고 있다면 눈(Snow) 사운드 재생
 * 2순위: 태그가 없다면, 라인트레이스를 통해 바닥 재질을 판별하여 금속(Metal) 등 특정 사운드 재생
 * 3순위: 그 외 일반 지형은 기본(Default) 사운드 재생
 */
UCLASS()
class TEAMPROJECT_FIN_API UAN_Footstep : public UAnimNotify
{
	GENERATED_BODY()

public:
	UAN_Footstep();

	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;

	/** 발 뼈 이름 (소켓 이름) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Footstep")
	FName FootSocketName = FName("foot_r");

	/** 기본 바닥(흙, 나무 등) 사운드 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Footstep")
	TObjectPtr<USoundBase> DefaultSound;

	/** 눈(Snow) 지형 사운드 (우선순위 높음) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Footstep")
	TObjectPtr<USoundBase> SnowSound;

	/** 금속(Metal) 바닥 사운드 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Footstep")
	TObjectPtr<USoundBase> MetalSound;

	/** 트레이스 검사 거리 (기본 50) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Footstep")
	float TraceDistance = 50.0f;

	/** 사운드 볼륨 배수 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Footstep")
	float VolumeMultiplier = 0.5f;

	/** 사운드 피치(음높이) 배수 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Footstep")
	float PitchMultiplier = 1.0f;

	/** 눈(Snow) 지형 파티클 (옵션) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Footstep|VFX")
	TObjectPtr<class UNiagaraSystem> SnowVFX;

	/** VFX를 스폰할 때 눈 표면을 정확히 찾기 위해 발에서 위로 올릴 높이 (무릎 이상으로 넉넉히 설정) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Footstep|VFX")
	float SurfaceTraceHeight = 80.0f;
};
