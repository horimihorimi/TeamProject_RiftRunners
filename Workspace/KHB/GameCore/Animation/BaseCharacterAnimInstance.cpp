#include "GameCore/Animation/BaseCharacterAnimInstance.h"

void UBaseCharacterAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();

	// 새 AnimInstance가 초기화될 때는 일반 애니메이션 출력을 사용한다.
	bUseDeathPose = false;
	bPendingDeathPoseCapture = false;
}

void UBaseCharacterAnimInstance::RequestDeathPoseCapture()
{
	// 노티파이 처리 시점이 아니라 현재 프레임의 애니메이션 평가가 끝난 뒤 저장한다.
	bPendingDeathPoseCapture = true;
}

void UBaseCharacterAnimInstance::ClearDeathPose()
{
	// 캡처 대기 중에 되살아나면 다음 평가에서 다시 사망 자세로 굳지 않도록 요청도 함께 버린다.
	bUseDeathPose = false;
	bPendingDeathPoseCapture = false;
}

void UBaseCharacterAnimInstance::NativePostEvaluateAnimation()
{
	Super::NativePostEvaluateAnimation();

	if (!bPendingDeathPoseCapture)
	{
		return;
	}

	bPendingDeathPoseCapture = false;

	// 노티파이가 발생한 프레임에 최종 평가된 모든 본 Transform을 저장한다.
	SavePoseSnapshot(DeathPoseSnapshotName);

	// 다음 AnimGraph 평가부터 저장한 사망 자세를 출력한다.
	bUseDeathPose = true;
}
