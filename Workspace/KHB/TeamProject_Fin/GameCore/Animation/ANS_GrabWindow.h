#pragma once

#include "CoreMinimal.h"
#include "GameCore/Animation/ANS_MeleeAttackHit.h"
#include "ANS_GrabWindow.generated.h"

/**
 * 잡기 판정 윈도우.
 *
 * 궤적을 훑는 방식은 근접 타격과 같지만, 걸린 대상에게 데미지를 넣지 않고
 * 이벤트만 보낸다. 잡힌 순간과 데미지가 들어가는 순간이 다르기 때문이다
 * (잡기 성립 -> 잠시 뒤 손을 뽑을 때 데미지).
 *
 * 그래서 이 노티파이에는 DamageEffectClass를 지정하지 않는다. 지정하면
 * 잡히는 순간에도 데미지가 들어가 두 번 맞는다.
 *
 * 같은 대상에게 이벤트가 여러 번 갈 수 있다. 첫 번째만 처리하는 책임은
 * 받는 쪽(UGA_SoulRip)에 있다. 노티파이가 상태를 들면 이 노티파이를 쓰는
 * 모든 메시가 그 상태를 공유해서 몬스터가 둘 이상일 때 서로 덮어쓴다.
 */
UCLASS(meta = (DisplayName = "Grab Window"))
class TEAMPROJECT_FIN_API UANS_GrabWindow : public UANS_MeleeAttackHit
{
	GENERATED_BODY()

public:
	UANS_GrabWindow();

	/** 판정에 걸렸을 때 시전자에게 보낼 이벤트. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grab",
		meta = (Categories = "Event.AI"))
	FGameplayTag CaughtEventTag;

protected:
	virtual void OnTraceResolved(
		USkeletalMeshComponent* MeshComp,
		const FVector& Start,
		const FVector& End,
		const TArray<FHitResult>& HitResults) override;
};
