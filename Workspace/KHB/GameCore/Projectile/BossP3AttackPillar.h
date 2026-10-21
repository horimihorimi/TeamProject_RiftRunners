#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayEffectTypes.h"
#include "BossP3AttackPillar.generated.h"

class UCapsuleComponent;
class UStaticMeshComponent;
class UGameplayEffect;
class UPrimitiveComponent;

/** P3Attack에서 미러와 바닥을 잇는 고정 사선 Pillar입니다. */
UCLASS()
class TEAMPROJECT_FIN_API ABossP3AttackPillar : public AActor
{
	GENERATED_BODY()

public:
	ABossP3AttackPillar();
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Top과 Bottom을 잇도록 Pillar의 위치·회전·길이를 한 번만 확정합니다. */
	void InitializePillar(const FVector& Top, const FVector& Bottom, float Radius);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pillar")
	TObjectPtr<UCapsuleComponent> PillarCollision;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pillar")
	TObjectPtr<UStaticMeshComponent> PillarMesh;

	/** Pillar 영역에 들어온 플레이어에게 적용할 Infinite 슬로우 GE입니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pillar|Effect")
	TSubclassOf<UGameplayEffect> SlowEffectClass;

	/** BP에 넣은 원기둥 메시의 반경(cm)입니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pillar|Visual", meta = (ClampMin = "0.01"))
	float MeshBaseRadius = 50.0f;

	/** BP에 넣은 원기둥 메시의 Z축 전체 길이(cm)입니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pillar|Visual", meta = (ClampMin = "0.01"))
	float MeshBaseLength = 100.0f;

private:
	/** 서버에서 확정한 기둥 반경/길이를 서버와 클라이언트 모두에 반영한다. */
	void ApplyPillarDimensions();

	UFUNCTION()
	void OnRep_PillarDimensions();

	/** TraceAttack 조회 결과를 기준으로 슬로우 진입/이탈을 동기화한다. */
	void RefreshSlowTargets();
	void ApplySlowToPlayer(AActor* Target);
	void RemoveSlowFromPlayer(AActor* Target);

	/** 이 Pillar가 직접 적용한 GE만 제거하기 위한 대상별 Handle입니다. */
	TMap<TObjectPtr<AActor>, FActiveGameplayEffectHandle> AppliedSlowEffects;

	/** 컴포넌트 런타임 크기 변경은 자동 복제되지 않으므로 별도로 전달한다. */
	UPROPERTY(ReplicatedUsing = OnRep_PillarDimensions)
	float PillarRadius = 50.0f;

	UPROPERTY(ReplicatedUsing = OnRep_PillarDimensions)
	float PillarLength = 100.0f;
};
