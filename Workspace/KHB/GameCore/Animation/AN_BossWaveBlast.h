#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AN_BossWaveBlast.generated.h"

/**
 * Shattered Mirror가 에너지 벽으로 발사되는 프레임을 알립니다.
 *
 * 노티파이가 직접 벽을 스폰하지 않는 이유는, 노티파이가 클라이언트에서도
 * 실행되어 같은 벽이 두 번 생기기 때문이다. 여기서는 "언제"만 알리고
 * "무엇을"은 서버 권한을 가진 GA가 결정한다.
 */
UCLASS(meta = (DisplayName = "Boss Shattered Mirror Fire"))
class TEAMPROJECT_FIN_API UAN_BossWaveBlast : public UAnimNotify
{
	GENERATED_BODY()

public:
	virtual FString GetNotifyName_Implementation() const override;

	virtual void Notify(
		USkeletalMeshComponent* MeshComp,
		UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;
};
