#pragma once

#include "CoreMinimal.h"
#include "GameCore/Animation/ANS_HitWindowBase.h"
#include "ANS_BeamHit.generated.h"

class UNiagaraComponent;
class UNiagaraSystem;
class USkeletalMeshComponent;

/** 빔이 어느 방향으로 뻗을지 정하는 기준. */
UENUM(BlueprintType)
enum class EBeamAimMode : uint8
{
	/** 소유자 전방. 몸이 도는 대로 빔도 돈다. ANS_FaceTarget과 함께 쓰는 방식. */
	OwnerForward UMETA(DisplayName = "Owner Forward"),

	/** 소켓 전방. 눈 본이 실제로 시선을 향하고 있을 때만 쓴다. */
	SocketForward UMETA(DisplayName = "Socket Forward"),

	/** 타깃 추적. 몸은 몽타주가 정한 대로 두고 빔 끝만 타깃을 따라간다. */
	Target UMETA(DisplayName = "Track Target"),
};

/**
 * 소켓에서 고정 길이로 뻗은 직선을 훑는 타격 윈도우.
 *
 * 타깃까지 이어주는 빔이 아니다. BeamLength까지만 뻗으므로 그 밖으로 벗어난
 * 대상은 맞지 않는다. 빔이 향하는 방향은 AimMode가 정한다.
 *
 * Track Target 모드는 몸을 돌리지 않고 빔만 타깃을 쫓는다. 몽타주가 정한
 * 자세를 유지한 채 조준만 움직이는 터렛 연출에 쓴다. 몸까지 같이 돌리려면
 * Owner Forward로 두고 ANS_FaceTarget을 같은 구간에 깔면 된다.
 *
 * 빔은 계속 닿아 있으므로 HitCount(또는 ReHitInterval)를 반드시 채운다.
 * 비워두면 윈도우 내내 한 대만 들어간다.
 * 0이면 처음 한 번만 맞고 나머지 시간이 무해해진다.
 */
UCLASS(meta = (DisplayName = "Beam Hit Window"))
class TEAMPROJECT_FIN_API UANS_BeamHit : public UANS_HitWindowBase
{
	GENERATED_BODY()

public:
	/** 빔 연출. 소켓에 부착되며 끝점은 BeamEndParameterName으로 매 틱 갱신한다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Beam")
	TObjectPtr<UNiagaraSystem> BeamSystem;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Beam")
	FName BeamSocketName = TEXT("EyeSocket");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Beam",
		meta = (ClampMin = "1.0"))
	float BeamLength = 3000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aim")
	EBeamAimMode AimMode = EBeamAimMode::Target;

	/**
	 * 빔이 방향을 트는 속도(초당 각도). 0이면 즉시 조준한다.
	 *
	 * 조준은 몬스터가 보고 있던 방향에서 출발해 타깃 쪽으로 이 속도만큼 좁혀
	 * 나간다. 그래서 빔이 켜진 뒤 훑고 지나가는 구간이 생긴다.
	 *
	 * 이 패턴의 난이도를 좌우하는 값이다. 빠르면 옆으로 뛰어도 계속 붙어서
	 * 사거리 밖으로 도망치는 수밖에 없고, 느리면 각을 벌려 떨궈낼 수 있다.
	 * Track Target 모드에서만 쓰인다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aim",
		meta = (ClampMin = "0.0"))
	float TurnSpeed = 90.0f;

	/**
	 * 타깃 추적 시 높이 차까지 따라갈지.
	 *
	 * 끄면 수평으로만 훑는다. 지상몹이라 기본은 꺼둔다. 높이 차는 TraceRadius가
	 * 어느 정도 흡수한다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aim")
	bool bAimPitch = false;

	/**
	 * 빔의 끝점을 받는 나이아가라 Vector 사용자 파라미터 이름.
	 *
	 * 트레이스가 구한 끝점을 그대로 넘기므로 연출과 판정이 항상 일치한다.
	 * 벽에 막히면 그 지점까지만 그린다. 비워두면 갱신하지 않는다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Beam")
	FName BeamEndParameterName = TEXT("Beam End");

	/**
	 * 위 파라미터가 월드 좌표를 기대하는지.
	 *
	 * 이미터가 Local Space면 꺼야 한다. 끄면 같은 월드 끝점을 빔 컴포넌트
	 * 기준 로컬로 변환해 넘기므로, 소켓 축이 어느 방향이든 결과는 같다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Beam")
	bool bBeamEndIsWorldSpace = true;

protected:
	virtual bool GetTraceSegment(
		USkeletalMeshComponent* MeshComp,
		float DeltaTime,
		FVector& OutStart,
		FVector& OutEnd) override;

	virtual void OnWindowBegin(USkeletalMeshComponent* MeshComp) override;
	virtual void OnWindowEnd(USkeletalMeshComponent* MeshComp) override;

	virtual void OnTraceResolved(
		USkeletalMeshComponent* MeshComp,
		const FVector& Start,
		const FVector& End,
		const TArray<FHitResult>& HitResults) override;

	virtual void DrawDebugTrace(
		const AActor* Owner,
		const FVector& Start,
		const FVector& End,
		const TArray<FHitResult>& HitResults,
		bool bHit) const override;

private:
	void DestroyBeam(USkeletalMeshComponent* MeshComp);

	/** AimMode에 따른 이번 틱의 목표 방향. 구하지 못하면 소유자 전방으로 떨어진다. */
	FVector GetDesiredDirection(
		const USkeletalMeshComponent* MeshComp,
		const FVector& Origin) const;

	/** 메시별 빔 컴포넌트. 한 변수에 담으면 엘리트가 둘일 때 서로 덮어쓴다. */
	TMap<TWeakObjectPtr<USkeletalMeshComponent>, TWeakObjectPtr<UNiagaraComponent>> ActiveBeams;

	/** 메시별 현재 조준 방향. 목표 방향으로 TurnSpeed만큼씩 좁혀 나간다. */
	TMap<TWeakObjectPtr<USkeletalMeshComponent>, FVector> AimDirections;
};
