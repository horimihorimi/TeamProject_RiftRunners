#include "GameCore/Animation/AN_CaptureDeathPose.h"

#include "Components/SkeletalMeshComponent.h"
#include "GameCore/Animation/BaseCharacterAnimInstance.h"

UAN_CaptureDeathPose::UAN_CaptureDeathPose()
{
	// 몽타주에서 지정한 프레임에 동기적으로 실행한다.
	bIsNativeBranchingPoint = true;
}

void UAN_CaptureDeathPose::Notify(
	USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	if (!IsValid(MeshComp))
	{
		return;
	}

	UBaseCharacterAnimInstance* AnimInstance =
		Cast<UBaseCharacterAnimInstance>(MeshComp->GetAnimInstance());

	if (!IsValid(AnimInstance))
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("[CaptureDeathPose] %s의 AnimInstance가 UBaseCharacterAnimInstance를 상속하지 않았습니다."),
			*GetNameSafe(MeshComp->GetOwner()));
		return;
	}

	AnimInstance->RequestDeathPoseCapture();
}
