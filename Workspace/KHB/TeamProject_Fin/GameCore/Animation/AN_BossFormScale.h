#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "AN_BossFormScale.generated.h"

class UCurveFloat;

/**
 * 몽타주의 노티파이 구간 전체에 맞춰 보스 Form Scale Curve를 적용한다.
 * Curve 값은 기본 Form 대비 절대 배율이다. 예: 1 → 12 → 10.
 */
UCLASS(meta = (DisplayName = "Boss Form Scale"))
class TEAMPROJECT_FIN_API UAN_BossFormScale : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Form")
	TObjectPtr<UCurveFloat> ScaleCurve;

	virtual FString GetNotifyName_Implementation() const override;
	virtual void NotifyBegin(
		USkeletalMeshComponent* MeshComp,
		UAnimSequenceBase* Animation,
		float TotalDuration,
		const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyTick(
		USkeletalMeshComponent* MeshComp,
		UAnimSequenceBase* Animation,
		float FrameDeltaTime,
		const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyEnd(
		USkeletalMeshComponent* MeshComp,
		UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;
};
