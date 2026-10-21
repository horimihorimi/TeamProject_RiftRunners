#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "ANS_FaceTarget.generated.h"

/**
 * 노티파이 구간 동안만 소유 액터를 AMonsterAIController가 인지한 타깃 쪽으로 회전시킨다.
 *
 * Motion Warping의 회전 워프와 달리 이동 워프와 완전히 분리된 시스템이라
 * 같은 구간에 이동 워프가 걸려 있어도 서로 간섭해 궤적이 휘지 않는다.
 */
UCLASS(meta = (DisplayName = "Face Target"))
class TEAMPROJECT_FIN_API UANS_FaceTarget : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	// 타깃 방향으로 회전할 때의 보간 속도.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Face")
	float RotationSpeed = 10.0f;

	// 켜면 Yaw뿐 아니라 Pitch도 타깃 방향으로 맞춘다. 부유/비행형 몹용.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Face")
	bool bIncludePitch = false;

	virtual void NotifyTick(
		USkeletalMeshComponent* MeshComp,
		UAnimSequenceBase* Animation,
		float FrameDeltaTime,
		const FAnimNotifyEventReference& EventReference) override;
};
