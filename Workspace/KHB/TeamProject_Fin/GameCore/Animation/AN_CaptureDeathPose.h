#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AN_CaptureDeathPose.generated.h"

/** 실행 시점의 캐릭터 자세를 사망 Pose Snapshot으로 저장한다. */
UCLASS(meta = (DisplayName = "Capture Death Pose"))
class TEAMPROJECT_FIN_API UAN_CaptureDeathPose : public UAnimNotify
{
	GENERATED_BODY()

public:
	UAN_CaptureDeathPose();

	virtual void Notify(
		USkeletalMeshComponent* MeshComp,
		UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;
};
