#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AN_BossP3AttackMirrorFire.generated.h"

/** P3Attack의 미러가 실제로 발사되는 프레임을 서버 GA에 알립니다. */
UCLASS(meta = (DisplayName = "Boss P3 Attack Mirror Fire"))
class TEAMPROJECT_FIN_API UAN_BossP3AttackMirrorFire : public UAnimNotify
{
	GENERATED_BODY()

public:
	virtual FString GetNotifyName_Implementation() const override;
	virtual void Notify(
		USkeletalMeshComponent* MeshComp,
		UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;
};
