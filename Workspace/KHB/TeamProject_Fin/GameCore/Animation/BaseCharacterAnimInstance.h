#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "BaseCharacterAnimInstance.generated.h"

/**
 * 플레이어와 몬스터가 공통으로 사용하는 캐릭터 애니메이션 기능을 제공한다.
 */
UCLASS()
class TEAMPROJECT_FIN_API UBaseCharacterAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	virtual void NativeInitializeAnimation() override;
	virtual void NativePostEvaluateAnimation() override;

	/** 현재 애니메이션 프레임의 평가가 끝난 뒤 사망 Pose Snapshot을 저장하도록 요청한다. */
	void RequestDeathPoseCapture();

	/** 부활·풀 재사용 시 저장된 사망 자세 출력을 끄고 일반 애니메이션으로 되돌린다. */
	void ClearDeathPose();

protected:
	/** 저장된 사망 자세를 AnimGraph에서 출력할지 여부 */
	UPROPERTY(BlueprintReadOnly, Transient, Category = "Character|Death")
	bool bUseDeathPose = false;

	/** 사망 자세를 저장하고 조회할 때 사용하는 Pose Snapshot 이름 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Character|Death")
	FName DeathPoseSnapshotName = TEXT("DeathPose");

private:
	/** 노티파이가 발생한 프레임의 최종 평가 포즈를 저장하기 위한 지연 요청 */
	bool bPendingDeathPoseCapture = false;
};
