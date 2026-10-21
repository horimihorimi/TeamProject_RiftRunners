#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "GameCore/Ability/DamageSystemLibrary.h"
#include "ANS_HitWindowBase.generated.h"

class UGameplayEffect;
class USkeletalMeshComponent;

/** 한 프레임에 처리할 스피어 스윕 구간. */
struct FHitTraceSegment
{
	FVector Start = FVector::ZeroVector;
	FVector End = FVector::ZeroVector;
};

/**
 * 타격 윈도우 하나가 유지하는 상태.
 *
 * AnimNotifyState 객체는 이 노티파이를 쓰는 모든 메시가 공유한다. 멤버 변수에
 * 직접 담으면 몬스터가 둘 이상일 때 서로 덮어쓰므로 메시별로 따로 보관한다.
 */
USTRUCT()
struct FHitWindowRuntime
{
	GENERATED_BODY()

	/*
	 * 대상별 마지막 타격 시각.
	 *
	 * 목록 전체를 주기적으로 비우는 방식이면 비우기 직전에 들어온 대상이 곧바로
	 * 두 번 연속 맞는다. 액터마다 따로 재야 진입 시점과 무관하게 간격이 지켜진다.
	 */
	TMap<TWeakObjectPtr<AActor>, float> LastHitTimes;

	/*
	 * 이 윈도우에 적용할 재타격 간격(초). 윈도우가 열릴 때 확정한다.
	 *
	 * HitCount로 지정하면 노티파이 길이에 따라 값이 달라지므로 프로퍼티가
	 * 아니라 여기에 담는다. 같은 노티파이 클래스를 길이가 다른 구간에 얹어도
	 * 서로 간섭하지 않는다.
	 */
	float ReHitInterval = 0.0f;

	/*
	 * 이 윈도우가 낼 데미지의 공격 태그. 윈도우가 열릴 때 확정한다.
	 *
	 * 타격마다 다시 물으면 몽타주 도중 다른 어빌리티가 끼어들 때 진행 중인
	 * 판정의 위력이 따라 바뀐다.
	 */
	FGameplayTag AttackTag;
};

/**
 * 몬스터 공격의 타격 윈도우 공통 절차를 담당하는 기반 NotifyState.
 *
 * 노티파이가 깔린 구간 동안 매 틱 스피어 스윕을 쏘고, 맞은 대상에게
 * GameplayEffect를 적용한다. 콜리전 컴포넌트를 켜고 끄는 방식이 아니라
 * 노티파이 구간 자체가 판정 구간이다.
 *
 * 하위 클래스는 "이번 틱에 어디를 훑을지"만 정의한다. 근접은 소켓이 지나간
 * 궤적을, 빔은 소켓에서 전방으로 뻗은 직선을 훑는 식이다.
 */
UCLASS(Abstract)
class TEAMPROJECT_FIN_API UANS_HitWindowBase : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hit",
		meta = (ClampMin = "1.0"))
	float TraceRadius = 45.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Damage")
	TSubclassOf<UGameplayEffect> DamageEffectClass;

	/**
	 * 이 윈도우 동안 같은 대상을 때릴 횟수.
	 *
	 * 몽타주에서 프레임을 세어 초로 환산할 필요가 없다. 회전 3바퀴에 3대면
	 * 3을 넣는다. 구간 길이를 늘리거나 줄여도 타수는 그대로 유지된다.
	 *
	 * 1 이하면 이 값을 무시하고 ReHitInterval을 쓴다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Damage",
		meta = (ClampMin = "0"))
	int32 HitCount = 0;

	/**
	 * 같은 대상을 다시 때리기까지 필요한 간격(초). HitCount가 1 이하일 때만 쓴다.
	 *
	 * 0이면 노티파이 구간당 한 번만 판정한다. 타수보다 리듬이 중요한 패턴,
	 * 또는 구간 길이와 무관하게 일정 간격을 유지하고 싶을 때 쓴다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Damage",
		meta = (EditCondition = "HitCount <= 1", ClampMin = "0.0"))
	float ReHitInterval = 0.0f;

	/**
	 * 공격 태그를 손으로 덮어쓴다. 비워두면 시전 중인 어빌리티의 태그를 따른다.
	 *
	 * 평소에는 비워둔다. 태그는 어빌리티가 Asset Tags로 이미 갖고 있고, 같은
	 * 사실을 두 곳에 적으면 언젠가 어긋난다.
	 *
	 * 어빌리티를 거치지 않고 몽타주만 재생하는 경우, 또는 한 몽타주 안에서
	 * 구간마다 위력을 다르게 주고 싶은 경우에만 채운다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Damage",
		meta = (Categories = "Action.AI"))
	FGameplayTag AttackTagOverride;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Debug")
	bool bDebugDrawTrace = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Debug",
		meta = (EditCondition = "bDebugDrawTrace", ClampMin = "0.0"))
	float DebugDrawDuration = 1.0f;

	virtual void NotifyBegin(
		USkeletalMeshComponent* MeshComp,
		UAnimSequenceBase* Animation,
		float TotalDuration,
		const FAnimNotifyEventReference& EventReference) override;

	virtual void NotifyTick(
		USkeletalMeshComponent* MeshComp,
		UAnimSequenceBase* Animation,
		float FrameDeltaTime,
		const FAnimNotifyEventReference& EventReference) override;

	virtual void NotifyEnd(
		USkeletalMeshComponent* MeshComp,
		UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;

protected:
	/**
	 * 이번 틱에 훑을 구간을 채운다. false를 반환하면 이번 틱은 판정하지 않는다.
	 *
	 * 매 틱 호출되므로 하위 클래스가 여기서 자기 기준점을 갱신해도 된다.
	 */
	virtual bool GetTraceSegment(
		USkeletalMeshComponent* MeshComp,
		float DeltaTime,
		FVector& OutStart,
		FVector& OutEnd)
		PURE_VIRTUAL(UANS_HitWindowBase::GetTraceSegment, return false;);

	/** 기본은 단일 구간이다. 다중 부위 공격만 이 함수를 오버라이드한다. */
	virtual void GetTraceSegments(
		USkeletalMeshComponent* MeshComp,
		float DeltaTime,
		TArray<FHitTraceSegment>& OutSegments);

	/** 윈도우가 열릴 때. 하위가 자기 상태를 준비한다. */
	virtual void OnWindowBegin(USkeletalMeshComponent* MeshComp) {}

	/** 윈도우가 닫힐 때. 하위가 스폰한 것은 반드시 여기서 정리한다. */
	virtual void OnWindowEnd(USkeletalMeshComponent* MeshComp) {}

	/** 트레이스 직후. 히트 지점으로 연출을 갱신하고 싶을 때 쓴다. */
	virtual void OnTraceResolved(
		USkeletalMeshComponent* MeshComp,
		const FVector& Start,
		const FVector& End,
		const TArray<FHitResult>& HitResults) {}

	/**
	 * 이번 틱에 훑은 구간을 그린다.
	 *
	 * 기본은 구간을 감싸는 캡슐이다. 짧고 굵은 근접 궤적에는 맞지만 길고 얇은
	 * 빔에는 축이 묻히므로, 그런 하위는 자기에게 맞는 그림으로 대체한다.
	 */
	virtual void DrawDebugTrace(
		const AActor* Owner,
		const FVector& Start,
		const FVector& End,
		const TArray<FHitResult>& HitResults,
		bool bHit) const;

private:
	void ApplyDamage(
		AActor* Owner,
		FHitWindowRuntime& Runtime,
		const TArray<FHitResult>& HitResults) const;

	/** 오버라이드가 있으면 그것을, 없으면 시전 중인 어빌리티의 태그를 쓴다. */
	FGameplayTag ResolveAttackTag(const AActor* Owner) const;

	TMap<TWeakObjectPtr<USkeletalMeshComponent>, FHitWindowRuntime> ActiveWindows;
};
