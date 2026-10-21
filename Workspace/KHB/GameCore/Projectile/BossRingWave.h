#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "GameFramework/Actor.h"
#include "BossRingWave.generated.h"

class UGameplayEffect;
class USceneComponent;
class UStaticMeshComponent;

UENUM(BlueprintType)
enum class EBossRingWavePlane : uint8 { Horizontal, Vertical };

UENUM(BlueprintType)
enum class EBossRingWaveOrientation : uint8
{
	WorldHorizontal,
	TargetFacingVertical,
	TargetFacingVerticalSideways UMETA(DisplayName = "Target Facing Vertical Sideways"),
	TargetFacingPlane
};

/** 보스 중심에 대기했다가 반지름만 커지는 원형 띠 위험 지대다. */
UCLASS()
class TEAMPROJECT_FIN_API ABossRingWave : public AActor
{
	GENERATED_BODY()
public:
	ABossRingWave();
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	void InitializeRing(EBossRingWavePlane InPlane, float InChargeRadius, float InMaxRadius,
		float InChargeExpansionSpeed, float InLaunchExpansionSpeed, float InBandHalfWidth, float InPlaneHalfThickness,
		float InFacingYaw, const FGameplayTag& InAttackTag, TSubclassOf<UGameplayEffect> InDamageEffectClass);
	void InitializeRingWithRotation(EBossRingWavePlane InPlane, const FRotator& InRotation,
		float InChargeRadius, float InMaxRadius, float InChargeExpansionSpeed, float InLaunchExpansionSpeed,
		float InBandHalfWidth, float InPlaneHalfThickness, const FGameplayTag& InAttackTag,
		TSubclassOf<UGameplayEffect> InDamageEffectClass);
	/** 강타용 수평 링: 즉시 바깥으로 확장한 뒤 Retract 호출까지 최대 크기에서 대기한다. */
	void InitializeGroundSlamRing(float InMaxRadius, float InExpandSpeed, float InRetractSpeed,
		float InBandHalfWidth, float InPlaneHalfThickness, const FGameplayTag& InAttackTag,
		TSubclassOf<UGameplayEffect> InDamageEffectClass);
	void LaunchRing();
	void BeginRetract();

	/** 판정 위치·회전만 보관한다. 시각 메시의 스케일은 이 루트에 전파되지 않는다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Ring") TObjectPtr<USceneComponent> RingRoot;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Ring") TObjectPtr<UStaticMeshComponent> RingMesh;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Ring", meta=(ClampMin="1.0")) float PlaneBaseSize = 100.0f;
	/** TraceAttack 후보 범위와 원형 띠의 실제 피격 판정을 화면에 표시한다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Debug") bool bDebugDrawDamageQuery = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Debug", meta=(ClampMin="0.0")) float DebugDrawDuration = 0.1f;

private:
	void UpdateVisual();
	void UpdatePlaneRotation();
	/** 이번 프레임에 반지름이 이동한 구간 전체를 검사한다. 빠른 링이 얇은 띠를 건너뛰지 않게 한다. */
	void ApplyRingDamage(float PreviousRadius, bool bRetractingPass);

	UPROPERTY(Replicated) bool bCharging = false;
	UPROPERTY(Replicated) bool bLaunched = false;
	UPROPERTY(Replicated) bool bHoldAtMaxRadius = false;
	UPROPERTY(Replicated) bool bRetracting = false;
	UPROPERTY(ReplicatedUsing=OnRep_RingState) EBossRingWavePlane Plane = EBossRingWavePlane::Horizontal;
	UPROPERTY(ReplicatedUsing=OnRep_RingState) float FacingYaw = 0.0f;
	UPROPERTY(ReplicatedUsing=OnRep_RingState) bool bUseExplicitRotation = false;
	UPROPERTY(ReplicatedUsing=OnRep_RingState) FRotator ExplicitRotation = FRotator::ZeroRotator;
	UPROPERTY(ReplicatedUsing=OnRep_RingState) float CurrentRadius = 0.0f;
	UPROPERTY(Replicated) float ChargeRadius = 0.0f;
	UPROPERTY(Replicated) float MaxRadius = 0.0f;
	UPROPERTY(Replicated) float ChargeExpansionSpeed = 0.0f;
	UPROPERTY(Replicated) float LaunchExpansionSpeed = 0.0f;
	UPROPERTY(Replicated) float RetractSpeed = 0.0f;
	UPROPERTY(Replicated) float BandHalfWidth = 30.0f;
	UPROPERTY(Replicated) float PlaneHalfThickness = 80.0f;

	UFUNCTION() void OnRep_RingState();
	FGameplayTag AttackTag;
	TSubclassOf<UGameplayEffect> DamageEffectClass;
	/** 확장·축소는 각각 한 번씩만 맞힐 수 있다. */
	TSet<TObjectPtr<AActor>> ExpandedDamagedActors;
	TSet<TObjectPtr<AActor>> RetractedDamagedActors;
};
