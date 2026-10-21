#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "GameplayTagContainer.h"
#include "AN_SendGameplayEvent.generated.h"

/** 몽타주/시퀀스의 지정 프레임에서 소유 Actor의 ASC로 Gameplay Event를 보낸다. */
UCLASS(meta = (DisplayName = "Send Gameplay Event"))
class TEAMPROJECT_FIN_API UAN_SendGameplayEvent : public UAnimNotify
{
	GENERATED_BODY()

public:
	UAN_SendGameplayEvent();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gameplay Event")
	FGameplayTag EventTag;

	virtual void Notify(USkeletalMeshComponent* MeshComp,
		UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;
};
