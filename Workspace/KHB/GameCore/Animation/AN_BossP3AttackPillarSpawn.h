#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AN_BossP3AttackPillarSpawn.generated.h"

/** P3Attack의 고정 Pillar를 생성할 프레임을 서버 GA에 알립니다. */
UCLASS(meta = (DisplayName = "Boss P3 Attack Pillar Spawn"))
class TEAMPROJECT_FIN_API UAN_BossP3AttackPillarSpawn : public UAnimNotify
{
	GENERATED_BODY()

public:
	virtual FString GetNotifyName_Implementation() const override;
	virtual void Notify(
		USkeletalMeshComponent* MeshComp,
		UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;
};
