// Copyright Epic Games, Inc. All Rights Reserved.

#include "ANS_MontagePlayRate.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"

void UANS_MontagePlayRate::NotifyBegin(
	USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation,
	float TotalDuration,
	const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);

	if (!IsValid(MeshComp))
	{
		return;
	}

	UAnimInstance* AnimInstance = MeshComp->GetAnimInstance();
	UAnimMontage* Montage = ResolveMontage(AnimInstance, Animation);

	if (!IsValid(AnimInstance) || !IsValid(Montage))
	{
		return;
	}

	FActivePlayRate& ActiveRate = ActivePlayRates.AddDefaulted_GetRef();
	ActiveRate.MeshComp = MeshComp;
	ActiveRate.AnimInstance = AnimInstance;
	ActiveRate.Montage = Montage;
	ActiveRate.PreviousPlayRate = AnimInstance->Montage_GetPlayRate(Montage);

	AnimInstance->Montage_SetPlayRate(Montage, FMath::Max(PlayRate, UE_SMALL_NUMBER));
}

void UANS_MontagePlayRate::NotifyEnd(
	USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);

	for (int32 Index = ActivePlayRates.Num() - 1; Index >= 0; --Index)
	{
		const FActivePlayRate& ActiveRate = ActivePlayRates[Index];

		if (ActiveRate.MeshComp.Get() != MeshComp)
		{
			continue;
		}

		if (UAnimInstance* AnimInstance = ActiveRate.AnimInstance.Get())
		{
			if (UAnimMontage* Montage = ActiveRate.Montage.Get())
			{
				AnimInstance->Montage_SetPlayRate(Montage, ActiveRate.PreviousPlayRate);
			}
		}

		ActivePlayRates.RemoveAt(Index);
		return;
	}
}

UAnimMontage* UANS_MontagePlayRate::ResolveMontage(
	UAnimInstance* AnimInstance,
	UAnimSequenceBase* Animation) const
{
	if (UAnimMontage* Montage = Cast<UAnimMontage>(Animation))
	{
		return Montage;
	}

	return IsValid(AnimInstance) ? AnimInstance->GetCurrentActiveMontage() : nullptr;
}
