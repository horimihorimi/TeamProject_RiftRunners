#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameFramework/Actor.h"
#include "GameplayEffectTypes.h"
#include "GameplayTagContainer.h"
#include "GameCore/Ability/DamageSystemLibrary.h"
#include "DroneSwarm.generated.h"

class UAbilitySystemComponent;
class UCharacterAttributeSet;
class UGameplayEffect;
class UNiagaraComponent;
class UNiagaraSoundCueData;
class UNiagaraSystem;
class UStaticMesh;
class UStaticMeshComponent;

/** 무리 전체가 함께 밟는 단계. 개체별 진행도는 FSwarmDrone이 따로 갖는다. */
UENUM(BlueprintType)
enum class EDroneSwarmPhase : uint8
{
	/** 각자의 사출 소켓에서 배치 지점까지 곡선 비행. 판정 없음. */
	Deploy,

	/** 자리를 잡고 맴돈다. 이 시간이 플레이어가 벗어날 여유가 된다. */
	Hover,

	/**
	 * 맴돌기를 멈추고 그 자리에 굳는다.
	 *
	 * 자폭 직전의 예고다. 움직이던 것이 딱 멈추면 무언가 일어난다는 신호로
	 * 읽히고, 판정 위치도 이때 확정되어 플레이어가 거리를 가늠할 수 있다.
	 */
	Settle,

	/** 전기로 이어지며 자폭한다. 전깃줄과 폭발이 동시에 판정된다. */
	Discharge,

	/** 판정이 끝났다. 연출이 보일 시간을 준 뒤 스스로 파괴된다. */
	Finished,
};

/** 드론 한 기의 경로와 진행 상태. */
USTRUCT()
struct FSwarmDrone
{
	GENERATED_BODY()

	/**
	 * 3차 베지어의 네 점. 사출 순간 확정하고 이후 갱신하지 않는다.
	 *
	 * ControlA는 출발 방향을, ControlB는 도착 방향을 정한다. 둘 다 개체마다
	 * 흩어 놓으므로 소켓을 떠나는 순간부터 경로가 갈린다.
	 *
	 * 배치에 난수가 섞이므로 클라이언트가 같은 계산을 반복해도 결과가 다르다.
	 * 그래서 결과를 통째로 복제한다. 드론 여섯 기라 크기는 얼마 되지 않는다.
	 */
	UPROPERTY()
	FVector LaunchLocation = FVector::ZeroVector;

	UPROPERTY()
	FVector ControlA = FVector::ZeroVector;

	UPROPERTY()
	FVector ControlB = FVector::ZeroVector;

	/** 비행이 끝나는 지점. 맴돌 때도 이 좌표를 중심으로 흔들린다. */
	UPROPERTY()
	FVector HoverLocation = FVector::ZeroVector;

	/** 개체별 출발 지연. 소켓이 서로 떨어져 있으면 0이어도 겹쳐 보이지 않는다. */
	UPROPERTY()
	float LaunchDelay = 0.0f;

	/**
	 * 맴돌 때 쓰는 축별 사인파 위상.
	 *
	 * 개체마다 달라야 한다. 같으면 전부 한 몸처럼 움직여 오히려 더 기계적이다.
	 */
	UPROPERTY()
	FVector HoverPhase = FVector::ZeroVector;

	/** 개체별 속도 배율. 같은 속도로 돌면 무리가 한 덩어리로 보인다. */
	UPROPERTY()
	float HoverSpeedScale = 1.0f;

	/*
	 * 아래는 각 머신이 자기 것으로 굴린다. 복제 대상이 아니다.
	 * 위 값들이 같으면 진행은 결정적이라 굳이 보낼 이유가 없다.
	 */

	/**
	 * 이번 틱의 실제 위치.
	 *
	 * 연출이 아니라 로직이 소유한다. 메시를 지정하지 않아도 디버그 드로우로
	 * 전 과정을 확인할 수 있어야 하기 때문이다.
	 */
	FVector CurrentLocation = FVector::ZeroVector;

	/** 이 드론이 현재 단계에서 보낸 시간. 단계가 바뀔 때 0으로 돌아간다. */
	float PhaseElapsed = 0.0f;

	/**
	 * 플레이어가 쏴서 떨어뜨렸는지.
	 *
	 * 원작 소형 드론이 그렇듯 한 방이면 부서진다. 체력을 두지 않는 이유는
	 * 데미지 값이 의미가 없어서다. 맞았다는 사실만 있으면 된다.
	 */
	bool bDestroyed = false;

	/** 드론 본체. 런타임에 만들어 붙인다. 없으면 디버그 드로우로만 보인다. */
	UPROPERTY(NotReplicated)
	TObjectPtr<UStaticMeshComponent> Visual = nullptr;
};

/**
 * 캐리어가 방출하는 드론 무리. 자리를 잡고 맴돌다 전기로 이어지며 자폭한다.
 *
 * 드론은 적이 아니라 이 패턴의 일부다. AI도 체력도 없고 정해진 순서만 밟는다.
 * 그래서 개체마다 액터를 만들지 않고 이 액터 하나가 전부를 관리한다.
 *
 * 자체 AbilitySystemComponent를 갖는 이유는 캐리어의 수명 때문이다. 몬스터는
 * 사망 0.1초 뒤 파괴되므로 캐리어를 데미지 출처로 삼으면 그 뒤에 터지는 드론이
 * 무해해진다. 사출 시점에 공격력을 넘겨받아 두면 캐리어가 죽어도 이미 나간
 * 드론은 제 몫을 한다. 게임플레이 큐도 이 ASC로 실행한다.
 *
 * 판정은 자폭 순간 한 번뿐이다. 그 전까지는 전부 예고이며, 맴도는 시간이
 * 그대로 플레이어가 사이를 빠져나갈 여유가 된다.
 */
UCLASS()
class TEAMPROJECT_FIN_API ADroneSwarm : public AActor, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	ADroneSwarm();

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	virtual void GetLifetimeReplicatedProps(
		TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/**
	 * 사출 시점에 한 번 호출한다. 여기서 배치를 전부 확정하고 이후 추적하지 않는다.
	 *
	 * 계속 따라가면 회피가 불가능해진다. 고정해야 "예측하고 미리 움직여라"가 된다.
	 *
	 * 드론 수는 InLaunchLocations의 개수가 정한다. 몸에 달린 것보다 많이 쏠 수는
	 * 없으므로 개수를 따로 두지 않는다.
	 */
	UFUNCTION(BlueprintCallable, Category = "Swarm")
	void Deploy(
		AActor* InInstigator,
		const TArray<FVector>& InLaunchLocations,
		const AActor* InTarget,
		const FGameplayTag& InAttackTag);

protected:
	virtual void Tick(float DeltaSeconds) override;

	// --- 배치 -------------------------------------------------------------

	/** 드론 사이 좌우 간격. 이 값과 폭발 반경이 벽에 틈이 생기는지를 정한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Placement",
		meta = (ClampMin = "0.0"))
	float Spacing = 220.0f;

	/**
	 * 캐리어에서 타깃까지의 거리 중 몇 지점에 벽을 세울지.
	 *
	 * 고정 거리로 두면 플레이어가 가까울 때 벽이 등 뒤에 생긴다. 비율로 잡아야
	 * 어디에 있든 항상 둘 사이를 막는다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Placement",
		meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float WallRatio = 0.6f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Placement",
		meta = (ClampMin = "0.0"))
	float MinWallDistance = 250.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Placement",
		meta = (ClampMin = "0.0"))
	float MaxWallDistance = 900.0f;

	/** 타깃에서 최소 이만큼은 떨어뜨린다. 발밑에 벽이 생기면 피할 수 없다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Placement",
		meta = (ClampMin = "0.0"))
	float MinTargetClearance = 200.0f;

	/** 벽을 축 방향으로 흔드는 폭. 일직선을 지그재그로 만든다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Placement",
		meta = (ClampMin = "0.0"))
	float DepthJitter = 60.0f;

	/** Scale=1에서 지면 기준 부유 높이. 시전자 Z scale을 반영하고 천장 아래로 제한한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Placement",
		meta = (ClampMin = "0.0"))
	float MinHoverHeight = 120.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Placement",
		meta = (ClampMin = "0.0"))
	float MaxHoverHeight = 300.0f;

	// --- 비행 -------------------------------------------------------------

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Flight",
		meta = (ClampMin = "0.01"))
	float FlightDuration = 0.9f;

	/**
	 * 개체별 출발 지연 간격. 0이면 전부 동시에 나간다.
	 *
	 * 한 지점에서 몰아 쏠 때는 시간차가 필요하지만, 소켓이 몸 여기저기에 흩어져
	 * 있으면 동시에 나가도 겹쳐 보이지 않는다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Flight",
		meta = (ClampMin = "0.0"))
	float StaggerInterval = 0.0f;

	/** 0이면 직선, 1이면 최대 호. 제어점을 직선 위에서 얼마나 밀어낼지 정한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Flight",
		meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float CurveAmount = 0.4f;

	/**
	 * 출발 쪽 제어점이 흩어지는 반경.
	 *
	 * 소켓을 떠나는 순간부터 개체마다 다른 방향으로 휘게 만든다. 0이면 전부
	 * 같은 각도로 나가서 경로가 나란히 겹친다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Flight",
		meta = (ClampMin = "0.0"))
	float DepartureRadius = 200.0f;

	/** 도착 쪽 제어점이 흩어지는 반경. 개체마다 다른 방향에서 파고들게 한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Flight",
		meta = (ClampMin = "0.0"))
	float ApproachRadius = 250.0f;

	// --- 맴돌기 -----------------------------------------------------------

	/**
	 * 자리를 잡고 자폭까지 맴도는 시간.
	 *
	 * 이 시간이 그대로 플레이어가 드론 사이를 빠져나갈 여유가 된다. 짧으면
	 * 피할 수 없는 벽이 되고, 길면 위험을 감수하고 통과하는 통로가 된다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hover",
		meta = (ClampMin = "0.0"))
	float HoverDuration = 2.5f;

	/**
	 * 배치 지점 주변을 수평으로 오가는 반경.
	 *
	 * 판정은 자폭 순간의 실제 위치로 하므로, 이 값이 크면 어디서 터질지
	 * 예측하기 어려워진다. Spacing의 1/4을 넘기지 않는 편이 좋다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hover",
		meta = (ClampMin = "0.0"))
	float HoverRadius = 60.0f;

	/** 수직으로 오가는 폭. 수평보다 작아야 떠 있는 느낌이 산다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hover",
		meta = (ClampMin = "0.0"))
	float HoverHeightRadius = 20.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hover",
		meta = (ClampMin = "0.0"))
	float HoverSpeed = 2.0f;

	/** 개체별 속도가 흔들리는 폭(비율). 0이면 전부 같은 속도로 돈다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hover",
		meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float HoverSpeedVariance = 0.3f;

	/**
	 * 맴돌기를 멈추고 자폭할 때까지 굳어 있는 시간.
	 *
	 * 이 정지가 유일한 예고다. 0으로 두면 아무 신호 없이 터진다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hover",
		meta = (ClampMin = "0.0"))
	float SettleDuration = 0.5f;

	// --- 자폭 -------------------------------------------------------------

	/**
	 * 자폭 연출이 보이는 시간. 끝나면 액터가 사라진다.
	 *
	 * 판정은 자폭 시작 프레임에 이미 끝나 있으므로 이 값은 연출 전용이다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Discharge",
		meta = (ClampMin = "0.0"))
	float DischargeDuration = 0.5f;

	/** 인접한 두 드론을 잇는 전기의 굵기. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Discharge",
		meta = (ClampMin = "1.0"))
	float LinkDamageRadius = 80.0f;

	/**
	 * 드론 각자의 폭발 반경. Spacing보다 크면 벽에 틈이 없어진다.
	 *
	 * 데미지에만 쓴다. 이펙트 크기는 큐 데이터 에셋의 Scale Parameter Value가
	 * 따로 정하며 이 값과 연동되지 않는다.
	 *
	 * 기준: 같은 나이아가라가 배율 1.0에서 반경 300. 배율 0.7이면 210이다.
	 * bDebugDrawSequence로 확인하고 조정한다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Discharge",
		meta = (ClampMin = "1.0"))
	float ExplosionRadius = 210.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Discharge")
	TSubclassOf<UGameplayEffect> DamageEffectClass;

	/**
	 * 자폭 판정이 어떤 공격인지. 위력표에서 계수를 찾는 열쇠다.
	 *
	 * BP에서 지정하지 않는다. Deploy가 시전자에게서 값으로 복사해 온다. 자폭할
	 * 무렵이면 캐리어가 이미 파괴돼 있을 수 있어 그때 물어볼 수는 없다.
	 *
	 * 전깃줄과 폭발이 이 태그를 함께 쓴다. 둘은 대상을 한 번에 모아 중복을
	 * 제거한 뒤 한 번만 때리는 구조라, 계수를 나누려면 그 구조부터 갈라야 한다.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Discharge")
	FGameplayTag AttackTag;

	// --- 연출 -------------------------------------------------------------

	/** 드론 본체 메시. 개체마다 하나씩 만든다. 비워두면 디버그 드로우로만 보인다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Effects")
	TObjectPtr<UStaticMesh> DroneMesh;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Effects")
	FVector DroneMeshScale = FVector(1.0f);

	/** 진행 방향을 바라보게 할지. 끄면 항상 같은 방향을 본다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Effects")
	bool bOrientToVelocity = true;

	/** 인접한 드론을 잇는 전기. 끝점을 LinkEndParameterName으로 넘긴다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Effects")
	TObjectPtr<UNiagaraSystem> LinkEffect;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Effects")
	FName LinkEndParameterName = TEXT("Beam End");

	/**
	 * 드론이 터질 때 실행할 게임플레이 큐.
	 *
	 * GCN_SpawnNiagaraAndSoundScaled 계열을 붙이면 이펙트와 사운드를 한곳에서
	 * 관리할 수 있고, 서버가 실행하면 클라이언트로 자동 복제된다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Effects")
	FGameplayTag DischargeCueTag;

	/**
	 * 큐에 넘길 이펙트·사운드 묶음.
	 *
	 * 큐 파라미터의 SourceObject로 전달되므로, 같은 큐 태그를 쓰는 다른
	 * 몬스터와 연출을 다르게 가져갈 수 있다. 비워두면 큐 에셋의 기본값이 쓰인다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Effects")
	TObjectPtr<UNiagaraSoundCueData> DischargeCueData;

	/**
	 * 메시나 나이아가라 없이 전 과정을 그린다.
	 *
	 * 이펙트를 만들기 전에 배치와 경로부터 검증하려는 것이다. 층을 다 쌓은 뒤에
	 * 문제를 찾으면 어디가 틀렸는지 구분이 안 된다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Debug")
	bool bDebugDrawSequence = false;

	/**
	 * 자폭 순간의 판정 반경을 이 시간만큼 남긴다.
	 *
	 * 나이아가라 폭발 크기를 판정 반경에 맞출 때 쓴다. 한 프레임만 그리면
	 * 이펙트와 겹쳐볼 수가 없어서 따로 수명을 준다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Debug",
		meta = (ClampMin = "0.0"))
	float DebugExplosionDrawDuration = 3.0f;

private:
	void BuildPlacement(const TArray<FVector>& InLaunchLocations, const AActor* InTarget);
	bool ResolveGroundZ(const FVector& Location, float& OutGroundZ) const;
	bool IsFlightPathClear(const FSwarmDrone& Drone) const;
	float GetDroneClearance() const;
	FVector ConstrainMovement(const FVector& Start, const FVector& End) const;

	/** Scale=1 기준 높이와 곡선을 시전자 크기에 맞춘다. */
	UPROPERTY(Replicated)
	float PlacementScale = 1.0f;

	void TickDeploy(float DeltaSeconds);
	void TickHover(float DeltaSeconds);

	void SpawnVisuals();
	void SyncVisuals();
	void DestroyLinks();
	void DrawSequence() const;

	/** 전기를 켜고 판정과 연출을 한 번에 끝낸다. */
	void BeginDischarge();

	/**
	 * 자폭 연출. 서버가 실행하면 클라이언트로 복제된다.
	 *
	 * 판정과 분리해 둔다. 데미지 설정이 비었다고 폭발이 안 보이면 원인을
	 * 짚기 어렵고, 연출은 밸런스와 무관하게 늘 나가야 한다.
	 */
	void ExecuteDischargeCues();

	/** 자폭 순간의 판정. 서버에서만 부른다. */
	void ApplyDischargeDamage();

	/**
	 * 스웜이 피격당했을 때. 맞은 컴포넌트로 어느 드론인지 가려낸다.
	 *
	 * 드론은 액터가 아니라 컴포넌트라 히트 액터만으로는 개체를 알 수 없다.
	 * 데미지 경로가 이펙트 컨텍스트에 히트 결과를 실어 보내므로 그걸 꺼내 쓴다.
	 */
	void OnDamageReceived(
		UAbilitySystemComponent* TargetASC,
		const FGameplayEffectSpec& Spec,
		FActiveGameplayEffectHandle Handle);

	/** 해당 드론을 무리에서 떨궈낸다. 모든 머신에서 같이 일어나야 한다. */
	UFUNCTION(NetMulticast, Reliable)
	void MulticastDestroyDrone(int32 DroneIndex);

	UFUNCTION()
	void OnRep_Drones();

	static FVector EvaluateBezier(
		const FVector& P0, const FVector& P1,
		const FVector& P2, const FVector& P3, float T);

	UPROPERTY(ReplicatedUsing = OnRep_Drones)
	TArray<FSwarmDrone> Drones;

	/** 인접 쌍을 잇는 전기. 드론이 N기면 N-1개다. */
	UPROPERTY()
	TArray<TObjectPtr<UNiagaraComponent>> Links;

	UPROPERTY(VisibleAnywhere, Category = "Abilities")
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

	/** 공격력을 담는다. 사출 시점에 캐리어 값을 복사해 온다. */
	UPROPERTY()
	TObjectPtr<UCharacterAttributeSet> AttributeSet;

	EDroneSwarmPhase Phase = EDroneSwarmPhase::Deploy;
	float PhaseElapsed = 0.0f;
};
