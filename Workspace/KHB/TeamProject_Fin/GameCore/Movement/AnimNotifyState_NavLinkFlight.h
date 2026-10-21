#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "AnimNotifyState_NavLinkFlight.generated.h"

// Motion Warping의 이륙부터 착지까지 구간에 함께 배치합니다.
UCLASS(meta = (DisplayName = "Nav Link Flight"))
class TEAMPROJECT_FIN_API UAnimNotifyState_NavLinkFlight : public UAnimNotifyState
{
	GENERATED_BODY()

  public:
	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration,
							 const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
						   const FAnimNotifyEventReference& EventReference) override;
};
