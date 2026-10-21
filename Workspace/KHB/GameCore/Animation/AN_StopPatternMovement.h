#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AN_StopPatternMovement.generated.h"

/** 몽타주의 지정 프레임에서 DA가 시작한 병행 패턴 이동을 종료한다. */
UCLASS(meta=(DisplayName="Stop Pattern Movement"))
class TEAMPROJECT_FIN_API UAN_StopPatternMovement : public UAnimNotify
{
	GENERATED_BODY()

public:
	virtual FString GetNotifyName_Implementation() const override;
	virtual void Notify(USkeletalMeshComponent* MeshComp,
		UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;
};
