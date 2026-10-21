#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "BossWaveProjectile.generated.h"

class UBoxComponent;
class UGameplayEffect;
class UStaticMeshComponent;

/**
 * 보스의 판이 전개되어 날아가는 판형 에너지 벽입니다.
 *
 * 일반 투사체와 두 가지가 다르다.
 *
 * 첫째, 무언가에 맞아도 사라지지 않는다. 아레나를 가로지르며 지나가는 압박이라
 * 첫 명중에서 멈추면 패턴의 의미가 없어진다. 대신 같은 대상을 여러 번 때리지
 * 않도록 이미 맞춘 액터를 기록해 둔다.
 *
 * 둘째, 판정이 구가 아니라 박스다. 판의 폭과 높이가 그대로 회피 가능 범위라
 * 구로 근사하면 옆으로 빠져나갈 여지가 사라진다.
 */
UCLASS()
class TEAMPROJECT_FIN_API ABossWaveProjectile : public AActor
{
	GENERATED_BODY()

public:
	ABossWaveProjectile();

	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/**
	 * 스폰 직후 발사 정보를 확정합니다.
	 *
	 * 생성자에서 크기를 정할 수 없어 스폰한 쪽이 알려줘야 한다. 판마다 치수가
	 * 달라서 클래스 기본값 하나로는 표현되지 않는다.
	 */
	void InitializeWave(
		const FVector& InBoxExtent,
		const FVector& InDirection,
		float InSpeed,
		const FGameplayTag& InAttackTag,
		TSubclassOf<UGameplayEffect> InDamageEffectClass);

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wave")
	TObjectPtr<UBoxComponent> HitBox;

	/** 연출용 메시입니다. 나이아가라로 대체하면 비워 둡니다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wave")
	TObjectPtr<UStaticMeshComponent> WaveMesh;

	/** 메시 원본 크기를 BoxExtent에 맞추기 위한 기준 치수(cm)입니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Wave")
	FVector MeshBaseSize = FVector(100.0f, 100.0f, 100.0f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Wave")
	TSubclassOf<UGameplayEffect> DamageEffectClass;

private:
	/** 서버가 결정한 판정/시각 크기를 클라이언트 사본에도 적용한다. */
	void ApplyWaveExtent();

	UFUNCTION()
	void OnRep_WaveExtent();

	/**
	 * 이동 구간을 ECC_TraceAttack 박스로 쓸어 얻은 단일 피격을 처리한다.
	 * 벽은 관통해야 하므로 ComponentHit/Overlap 이벤트에 맡기지 않는다.
	 */
	void ApplyWaveDamage(const FHitResult& Hit);

	/** 위력표에서 계수를 찾을 공격 태그입니다. */
	FGameplayTag AttackTag;

	/**
	 * SpawnActor 뒤 서버에서만 InitializeWave가 호출된다. UBoxComponent와 Mesh의
	 * 런타임 변경은 자동 복제되지 않으므로, 이 값으로 클라이언트도 같은 크기를 만든다.
	 */
	UPROPERTY(ReplicatedUsing = OnRep_WaveExtent)
	FVector WaveExtent = FVector(50.0f);

	/** 진입 시 확정된 진행 방향. 벽은 유도하지 않으므로 갱신하지 않는다. */
	FVector Direction = FVector::ForwardVector;

	float Speed = 0.0f;

	/** 관통하면서 같은 대상을 반복해서 때리지 않도록 기록해 둔다. */
	UPROPERTY(Transient)
	TSet<TObjectPtr<AActor>> DamagedActors;
};
