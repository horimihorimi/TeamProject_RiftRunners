#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AN_BossP3AttackMissileFire.generated.h"

/** P3Attack의 유도탄을 발사할 프레임을 서버 GA에 알립니다. */
UCLASS(meta = (DisplayName = "Boss P3 Attack Missile Fire"))
class TEAMPROJECT_FIN_API UAN_BossP3AttackMissileFire : public UAnimNotify
{
	GENERATED_BODY()

public:
	virtual FString GetNotifyName_Implementation() const override;
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;
};
