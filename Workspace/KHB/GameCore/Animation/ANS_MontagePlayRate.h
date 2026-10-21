// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "ANS_MontagePlayRate.generated.h"

class UAnimInstance;
class UAnimMontage;

/** Changes the active montage play rate only while this notify state is active. */
UCLASS(meta = (DisplayName = "Montage Play Rate"))
class TEAMPROJECT_FIN_API UANS_MontagePlayRate : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	/** Play rate applied during the notify interval. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Montage", meta = (ClampMin = "0.01", UIMin = "0.01"))
	float PlayRate = 0.5f;

	virtual void NotifyBegin(
		USkeletalMeshComponent* MeshComp,
		UAnimSequenceBase* Animation,
		float TotalDuration,
		const FAnimNotifyEventReference& EventReference) override;

	virtual void NotifyEnd(
		USkeletalMeshComponent* MeshComp,
		UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;

private:
	struct FActivePlayRate
	{
		TWeakObjectPtr<USkeletalMeshComponent> MeshComp;
		TWeakObjectPtr<UAnimInstance> AnimInstance;
		TWeakObjectPtr<UAnimMontage> Montage;
		float PreviousPlayRate = 1.0f;
	};

	UAnimMontage* ResolveMontage(UAnimInstance* AnimInstance, UAnimSequenceBase* Animation) const;

	TArray<FActivePlayRate> ActivePlayRates;
};
