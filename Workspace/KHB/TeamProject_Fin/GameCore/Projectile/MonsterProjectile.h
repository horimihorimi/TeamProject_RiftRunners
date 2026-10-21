#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "GameFramework/Actor.h"
#include "MonsterProjectile.generated.h"

class USphereComponent;
class UNiagaraComponent;
class UNiagaraSystem;
class UMonsterProjectileMovementComponent;
class UGameplayEffect;
class USoundAttenuation;
class USoundBase;
class ABaseCharacter;

USTRUCT(BlueprintType)
struct FNiagaraUserFloatParameter
{
	GENERATED_BODY()

	// "User." 접두사를 포함한 전체 이름. 대소문자는 무시되지만 공백은 구분된다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cue|VFX")
	FName ParameterName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cue|VFX")
	float Value = 1.0f;
};

/**
 * 큐가 재생할 나이아가라와 사운드 한 세트.
 *
 * 여러 몬스터가 같은 큐 태그를 공유하면서 연출만 다르게 하려고 만든다.
 * 요청하는 쪽이 자기 데이터에셋을 큐 파라미터의 SourceObject로 실어 보내면,
 * 큐 노티파이가 그 값으로 재생한다. 몬스터를 추가할 때 태그를 새로 만들거나
 * 코드를 고칠 필요가 없다.
 *
 * 투사체 자신을 SourceObject로 넘기지 않는 이유는, 분열 직후 파괴되기 때문에
 * 복제된 클라이언트에서 큐가 도착했을 때 이미 사라져 있을 수 있어서다.
 * 데이터에셋은 영구 에셋이라 그런 문제가 없다.
 */
UCLASS(BlueprintType)
class TEAMPROJECT_FIN_API UNiagaraSoundCueData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cue|VFX")
	TObjectPtr<UNiagaraSystem> Effect;

	// 크기용 사용자 파라미터. 비워두면 에셋 기본 크기로 나간다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cue|VFX")
	FName ScaleParameterName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cue|VFX", meta = (ClampMin = "0.0"))
	float ScaleParameterValue = 1.0f;

	// 크기 외에 더 넘길 float 파라미터가 있으면 여기에 추가한다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cue|VFX")
	TArray<FNiagaraUserFloatParameter> FloatParameters;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cue|Sound")
	TObjectPtr<USoundBase> Sound;

	// 비어 있지 않으면 이쪽에서 무작위로 하나 고르고, 비면 Sound를 쓴다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cue|Sound")
	TArray<TObjectPtr<USoundBase>> RandomSounds;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cue|Sound", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float SoundVolume = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cue|Sound", meta = (ClampMin = "0.1", UIMin = "0.1"))
	float SoundPitch = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cue|Sound")
	TObjectPtr<USoundAttenuation> SoundAttenuation;
};

/**
 * 몬스터 원거리 공격용 투사체.
 *
 * HomingDuration 초 동안 타깃을 추적하다가, 시간이 다하면 유도를 끄고
 * 그 순간의 속도로 직진한다. 끝까지 쫓지 않으므로 플레이어가 피할 수 있다.
 * Character.Player 대상에 명중하면 데미지를 적용하고 파괴된다. 발사자, 다른
 * 몬스터, 월드는 관통하며 아무것도 안 맞으면 수명이 다할 때 사라진다.
 *
	 * 분열 패턴이 켜지면 발사 위치에서 정지 -> 대기 -> 횡열 분열 순으로 진행한다.
	 * 분열 시점의 타깃 좌우를 기준으로 자탄을 한 줄로 놓고, 전부 모탄이 처음
	 * 향하던 방향과 평행하게 직진시킨다. 자탄은 타깃을 다시 조준하거나 추적하지
	 * 않으므로, 분열 뒤의 플레이어 이동에는 반응하지 않는다.
 *
 * 플레이어 무기 시스템(WeaponComponent/WeaponBase)과는 무관한 별도 경로다.
 */
UCLASS()
class TEAMPROJECT_FIN_API AMonsterProjectile : public AActor
{
	GENERATED_BODY()

public:
	AMonsterProjectile();

	/*
	 * 스폰 직후 발사자가 호출한다.
	 * 정해진 시간 동안 InTarget을 추적하다가, 시간이 다하면 직진한다.
	 */
	void LaunchAtTarget(AActor* InTarget, AActor* InInstigatorActor);
	/** 유도 없이 InDirection 방향으로 직진한다. 횡열 분열 자탄 전용이다. */
	void LaunchStraight(const FVector& InDirection, AActor* InInstigatorActor);
	void SetUseSplitPattern(bool bInUseSplitPattern);
	/** 분열 모탄의 정지 시간, 생성 자탄 수, 좌우 자탄 간격을 발사 GA에서 지정한다. */
	void SetSplitPatternSettings(float InHoverDuration, int32 InProjectileCount, float InLateralSpacing,
		float InChildScale, FName InChildScaleParameterName)
	{
		SplitHoverDuration = FMath::Max(InHoverDuration, 0.0f);
		SplitProjectileCount = FMath::Max(InProjectileCount, 1);
		SplitLateralSpacing = FMath::Max(InLateralSpacing, 0.0f);
		SplitChildScale = FMath::Max(InChildScale, KINDA_SMALL_NUMBER);
		SplitChildScaleParameterName = InChildScaleParameterName;
	}

	/*
	 * 분열로 태어난 자탄임을 표시한다. 자탄은 모탄과 같은 클래스로 스폰되므로
	 * SplitChild~ 값을 자기 것에서 그대로 읽는다.
	 * LaunchAtTarget보다 먼저 호출해야 스케일이 반영된다.
	 */
	void SetupAsSplitChild();

	void SetDamageEffectClass(TSubclassOf<UGameplayEffect> InDamageEffectClass);

	/** 발사 직후 시전자의 공격 태그를 넘겨받는다. 자탄에도 그대로 물려준다. */
	void SetAttackTag(const FGameplayTag& InAttackTag) { AttackTag = InAttackTag; }
	/** 활성 유도탄들 사이의 겹침을 줄이기 위해 무브먼트에 더할 분리 가속도다. */
	FVector GetHomingSeparationAcceleration() const;
	bool ShouldPassThrough(AActor* OtherActor) const;
protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/*
	 * 클라이언트는 무브먼트를 끄고 복제 위치로만 움직이므로 컴포넌트 속도가 0으로 남는다.
	 * 속도를 읽는 연출(나이아가라 속도 정렬·속도 상속)이 서버와 같은 방향을 보도록
	 * 서버가 보낸 속도를 그대로 넣는다.
	 */
	virtual void PostNetReceiveVelocity(const FVector& NewVelocity) override;

	virtual void GetLifetimeReplicatedProps(
		TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION()
	void OnRep_UseSplitPattern();

	/** 강화탄/분열 자탄의 런타임 시각·충돌 크기를 클라이언트에 재적용한다. */
	UFUNCTION()
	void OnRep_ProjectileVisualState();

	UFUNCTION()
	void OnProjectileHit(
		UPrimitiveComponent* HitComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		FVector NormalImpulse,
		const FHitResult& Hit);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Projectile")
	TObjectPtr<USphereComponent> CollisionComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Projectile")
	TObjectPtr<UNiagaraComponent> NiagaraComponent;

	// Owner가 이 태그를 보유한 경우에만 강화 탄환 설정을 적용한다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Enhanced")
	FGameplayTag EnhancedEffectRequiredTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Enhanced")
	TObjectPtr<UNiagaraSystem> EnhancedNiagaraEffect;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Enhanced", meta = (ClampMin = "0.0"))
	float EnhancedDamageMultiplier = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Enhanced", meta = (ClampMin = "0.0"))
	float EnhancedCollisionRadius = 0.0f;

	/*
	 * 나이아가라 에셋에 노출된 사용자 파라미터 이름.
	 * 투사체 크기는 컴포넌트 스케일이 아니라 이 파라미터로만 조절한다.
	 *
	 * 이름은 에셋에 적힌 그대로여야 한다. 대소문자는 무시되지만 공백은
	 * 다른 이름으로 취급되고, 없는 이름을 넘기면 아무 에러 없이 무시된다.
	 * 에셋에서 리네임하면 시스템 내부 링크가 끊기므로, 에셋을 고치지 말고
	 * 이 값을 맞추는 쪽이 안전하다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|VFX")
	FName BulletScaleParameterName = TEXT("User.BulletScale");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Projectile|VFX", meta = (ClampMin = "0.0"))
	float NormalNiagaraScale = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Projectile|VFX", meta = (ClampMin = "0.0"))
	float EnhancedNiagaraScale = 0.7f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Projectile")
	TObjectPtr<UMonsterProjectileMovementComponent> ProjectileMovement;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Damage")
	TSubclassOf<UGameplayEffect> DamageEffectClass;

	/**
	 * 이 투사체가 어떤 공격인지. 위력표에서 계수를 찾는 열쇠다.
	 *
	 * BP에서 지정하지 않는다. 발사한 어빌리티가 값으로 넘겨준다. 투사체는
	 * 어빌리티보다 오래 날아가므로 명중 시점에 물어볼 수는 없다.
	 *
	 * 자탄은 모탄과 같은 태그를 쓴다. 자탄이 약한 것은 공격 종류가 달라서가
	 * 아니라 크기가 줄었기 때문이고, 그 감소는 RuntimeDamageMultiplier가 맡는다.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Projectile|Damage")
	FGameplayTag AttackTag;

	/*
	 * 발사 후 타깃을 추적하는 시간(초). 이 시간이 지나면 유도가 꺼지고
	 * 그 순간의 속도로 등속 직진한다. 즉 이 값이 회피 창이 열리는 시점이며,
	 * 0이면 처음부터 직진이고 크게 잡을수록 피하기 어렵다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Projectile|Homing", meta = (ClampMin = "0.0"))
	float HomingDuration = 1.2f;

	/** 같은 몬스터 및 다른 몬스터가 쏜 유도탄이 포개지는 것을 완화한다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Projectile|Homing|Separation")
	bool bEnableHomingSeparation = true;

	/** 이 거리 안의 활성 유도탄만 서로 밀어낸다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Projectile|Homing|Separation", meta = (ClampMin = "0.0"))
	float HomingSeparationRadius = 200.0f;

	/** 추적 가속도에 더할 최대 분리 가속도. Homing Acceleration보다 낮게 두는 것이 자연스럽다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Projectile|Homing|Separation", meta = (ClampMin = "0.0"))
	float HomingSeparationAcceleration = 1500.0f;

	// 아무것도 안 맞았을 때 스스로 사라지기까지의 시간(초).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile", meta = (ClampMin = "0.0"))
	float ProjectileLifeSpan = 5.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile", meta = (ClampMin = "0.0"))
	float CollisionRadius = 20.0f;

	/**
	 * 명중 지점의 판정 반경을 구로 그린다.
	 *
	 * 자탄과 강화탄은 반경이 배율만큼 줄거나 늘기 때문에, 실제로 몇이 적용됐는지
	 * 눈으로 봐야 알 수 있다. 그려지는 값은 CollisionRadius가 아니라 배율까지
	 * 반영된 실제 스피어 반경이다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Debug")
	bool bDebugDrawHit = false;

	/** 판정 구를 남겨 둘 시간(초). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Debug",
		meta = (EditCondition = "bDebugDrawHit", ClampMin = "0.0"))
	float DebugDrawDuration = 3.0f;

	void ApplyOwnerProjectileEffect();

	float RuntimeDamageMultiplier = 1.0f;

	// 이전 전진형 분열 에셋과의 호환용 값이다. 횡열 분열에서는 사용하지 않는다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Projectile|Split", meta = (ClampMin = "0.0"))
	float SplitDelay = 0.40f;

	// 스폰 위치에서 정지한 채 분열을 기다리는 시간. 플레이어가 회피할 예고 구간이다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Projectile|Split", meta = (ClampMin = "0.0"))
	float SplitHoverDuration = 0.50f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Projectile|Split", meta = (ClampMin = "1", UIMin = "1"))
	int32 SplitProjectileCount = 4;

	/** 분열 자탄 사이의 좌우 간격. 타깃의 수평 Right 축을 따른다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Projectile|Split", meta = (ClampMin = "0.0"))
	float SplitLateralSpacing = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Projectile|Split|Child", meta = (ClampMin = "0.0"))
	float SplitChildScale = 0.5f;

	/** 자탄 VFX 크기에 넣을 Niagara User 파라미터. 비어 있으면 투사체 기본 파라미터를 사용한다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Projectile|Split|Child|VFX")
	FName SplitChildScaleParameterName = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Projectile|Split|Child", meta = (ClampMin = "0.0"))
	float SplitChildHomingAcceleration = 3500.0f;

	// 자탄 전용 추적 시간. 모탄이 이미 시간을 썼으므로 보통 더 짧게 잡는다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Projectile|Split|Child", meta = (ClampMin = "0.0"))
	float SplitChildHomingDuration = 0.8f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Projectile|Split|Child", meta = (ClampMin = "0.0"))
	float SplitChildInitialSpeed = 900.0f;

	// 자탄 전용 최고 속도. 일반탄의 MaxSpeed와 분리해서 잡는다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Projectile|Split|Child", meta = (ClampMin = "0.0"))
	float SplitChildMaxSpeed = 1800.0f;

	/*
	 * 자탄이 타깃 방향에서 바깥으로 벌어지는 각도(도).
	 *
	 * 0이면 전부 타깃을 곧장 보고 태어나 한 점으로 모인다.
	 * 키우면 자기 배치 방향 쪽으로 기울어져 퍼져 나간다.
	 * SplitChildHomingDuration과 짝으로 동작한다. 추적 시간이 0이면 퍼진
	 * 채로 직진하고, 남아 있으면 퍼졌다가 다시 타깃 쪽으로 휘어 들어온다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Projectile|Split|Child", meta = (ClampMin = "0.0", ClampMax = "90.0"))
	float SplitChildSpreadAngle = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Projectile|Split|VFX")
	TObjectPtr<UNiagaraSystem> SplitNiagaraEffect;

	// 분열 순간 정지 위치에서 실행할 게임플레이 큐. 모든 몬스터가 공유한다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Split|VFX", meta = (Categories = "GameplayCue"))
	FGameplayTag SplitBurstCueTag;

	/*
	 * 위 큐가 재생할 이펙트와 사운드. 몬스터마다 다른 에셋을 꽂으면
	 * 같은 큐 태그로도 연출이 달라진다. 비워두면 큐 에셋의 기본값으로 나간다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Split|VFX")
	TObjectPtr<UNiagaraSoundCueData> SplitBurstCueData;

	/*
	 * 분열 모탄 여부. LaunchAtTarget의 정지 처리는 서버에서만 돌기 때문에,
	 * 클라이언트도 같이 멈추려면 이 상태가 복제되어야 한다.
	 */
	UPROPERTY(ReplicatedUsing = OnRep_UseSplitPattern)
	bool bUseSplitPattern = false;

	UPROPERTY(ReplicatedUsing = OnRep_ProjectileVisualState)
	bool bIsSplitChild = false;

	UPROPERTY(ReplicatedUsing = OnRep_ProjectileVisualState)
	bool bIsEnhancedProjectile = false;
	bool bHasSplit = false;

	// 이 투사체가 쫓는 대상. 자탄도 같은 액터를 물려받는다.
	TObjectPtr<AActor> HomingTargetActor = nullptr;

	FTimerHandle SplitDelayTimerHandle;
	FTimerHandle SplitHoverTimerHandle;

	// 모탄 스폰 시의 진행 방향. 분열 자탄도 이 방향과 평행하게 날아간다.
	FVector SplitForwardDirection = FVector::ForwardVector;

	// 시각과 콜리전에 똑같이 곱해지는 런타임 크기 배율. 자탄이면 1보다 작다.
	UPROPERTY(ReplicatedUsing = OnRep_ProjectileVisualState)
	float RuntimeScaleMultiplier = 1.0f;

	/*
	 * 크기를 적용하는 유일한 지점.
	 * 콜리전 반경과 나이아가라 사용자 파라미터를 항상 같은 배율로 맞춘다.
	 */
	void ApplyProjectileScale();

	void BeginSplitHover();
	void SplitIntoProjectiles();
	void PlaySplitBurstCue(const FVector& InLocation);
	void ApplySplitVisual();
	void OnInstigatorDeathStarted(ABaseCharacter* DeadCharacter);

	TWeakObjectPtr<ABaseCharacter> InstigatorCharacter;
};
