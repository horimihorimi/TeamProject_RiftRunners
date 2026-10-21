#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AN_BossP3AttackShout.generated.h"

/** P3Attack 샤우트가 실제로 경직을 적용할 프레임을 서버 GA에 알립니다. */
UCLASS(meta = (DisplayName = "Boss P3 Attack Shout"))
class TEAMPROJECT_FIN_API UAN_BossP3AttackShout : public UAnimNotify
{
	GENERATED_BODY()

public:
	virtual FString GetNotifyName_Implementation() const override;
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;
};
