#pragma once

#include "CoreMinimal.h"
#include "GameCore/Animation/ANS_HitWindowBase.h"
#include "ANS_MeleeAttackHit.generated.h"

class USkeletalMeshComponent;

/**
 * 소켓이 지나간 궤적을 훑는 타격 윈도우.
 *
 * 직전 프레임 소켓 위치에서 현재 위치까지를 스윕하므로, 팔이 휘두르는 궤적이나
 * 루트모션으로 전진한 경로가 빠짐없이 판정된다.
 */
UCLASS(meta = (DisplayName = "Melee Attack Hit Window"))
class TEAMPROJECT_FIN_API UANS_MeleeAttackHit : public UANS_HitWindowBase
{
	GENERATED_BODY()

public:
	/**
	 * 동시에 판정할 명시적 타격 소켓 목록. 하위 본을 자동 순회하지 않는다.
	 * 비어 있으면 판정하지 않는다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hit")
	TArray<FName> HitSocketNames;

protected:
	virtual bool GetTraceSegment(
		USkeletalMeshComponent* MeshComp,
		float DeltaTime,
		FVector& OutStart,
		FVector& OutEnd) override;

	virtual void GetTraceSegments(
		USkeletalMeshComponent* MeshComp,
		float DeltaTime,
		TArray<FHitTraceSegment>& OutSegments) override;

	virtual void OnWindowBegin(USkeletalMeshComponent* MeshComp) override;
	virtual void OnWindowEnd(USkeletalMeshComponent* MeshComp) override;

private:
	/** 메시별·소켓별 직전 프레임 위치. 노티파이 객체가 공유되므로 메시로 구분한다. */
	TMap<TWeakObjectPtr<USkeletalMeshComponent>, TMap<FName, FVector>> PreviousSocketLocations;
};
