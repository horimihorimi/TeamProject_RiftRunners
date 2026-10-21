#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "EnemyBeamAimReplicationComponent.generated.h"

/**
 * 서버가 확정한 적 빔의 현재 조준 방향을 클라이언트 연출에 전달한다.
 *
 * AIController와 TargetActor는 서버에만 있으므로, 클라이언트 Notify가 타깃을
 * 다시 조회해 빔을 계산하면 서버 판정과 다른 방향을 보게 된다.
 */
UCLASS(ClassGroup = (Enemy), meta = (BlueprintSpawnableComponent))
class TEAMPROJECT_FIN_API UEnemyBeamAimReplicationComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UEnemyBeamAimReplicationComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** 서버가 HitWindow에서 최종 보간된 방향을 기록한다. */
	void SetServerAimDirection(const FVector& InDirection);
	void StopServerAim();

	bool GetReplicatedAimDirection(FVector& OutDirection) const;

private:
	UPROPERTY(Replicated)
	bool bBeamAimActive = false;

	UPROPERTY(Replicated)
	FVector_NetQuantizeNormal BeamAimDirection = FVector::ForwardVector;
};
