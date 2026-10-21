#pragma once

#include "GameCore/Ability/GA_EnemyPatternBase.h"
#include "GA_BossShatteredMirror.generated.h"

class ABossWaveProjectile;
class UAbilityTask_WaitGameplayEvent;
class UGameplayEffect;

/**
 * 판 하나가 벽으로 발사되는 지점입니다.
 *
 * 판이 본 하나가 아니라 여러 본으로 구부러지는 구조라, 판을 실제로 떼어내
 * 날리지 않는다. 판의 루트 본에서 위치와 방향만 읽어 별도의 벽을 스폰한다.
 * 본체의 판은 몽타주가 애니메이션으로 처리한다.
 */
USTRUCT(BlueprintType)
struct TEAMPROJECT_FIN_API FBossWaveEmitter
{
	GENERATED_BODY()

	/** 판의 루트 본. 예: Bone_AT_Plate_Front_UpperIn_L */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Wave")
	FName PlateBoneName;

	/*
	 * 박스 콜리전의 절반 크기(cm).
	 *
	 * 400x30x800 판이면 (200, 15, 400)이다. 판마다 치수가 달라 하나로 묶지 않는다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Wave")
	FVector BoxExtent = FVector(200.0f, 15.0f, 400.0f);
};

/** 본의 어느 축을 판의 법선(발사 방향)으로 볼지 지정합니다. */
UENUM(BlueprintType)
enum class EBossWaveLaunchAxis : uint8
{
	BoneX UMETA(DisplayName = "Bone X (Forward)"),
	BoneY UMETA(DisplayName = "Bone Y (Right)"),
	BoneZ UMETA(DisplayName = "Bone Z (Up)")
};

/**
 * 알렙시스 타우라의 사각 투영체-파동 패턴.
 *
 * 몽타주는 베이스가 재생하고, 이 클래스는 노티파이가 알려준 프레임에
 * 판 넷을 각자가 향한 방향으로 발사한다.
 */
UCLASS()
class TEAMPROJECT_FIN_API UGA_BossShatteredMirror : public UGA_EnemyPatternBase
{
	GENERATED_BODY()

public:
	UGA_BossShatteredMirror();

	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

protected:
	UFUNCTION()
	void OnWaveBlastEvent(FGameplayEventData Payload);

	/** 발사할 판들입니다. 배열이라 페이즈마다 개수가 달라져도 데이터만 고치면 됩니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Wave")
	TArray<FBossWaveEmitter> WaveEmitters;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Wave")
	TSubclassOf<ABossWaveProjectile> WaveProjectileClass;

	/*
	 * 판의 법선에 해당하는 본 축.
	 *
	 * 판마다 본이 이미 정렬되어 있어 축 하나만 정하면 넷 모두에 적용된다.
	 * 가장 얇은 치수의 축과 같아야 한다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Wave")
	EBossWaveLaunchAxis LaunchAxis = EBossWaveLaunchAxis::BoneX;

	/** 축 방향이 안쪽을 향하면 켭니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Wave")
	bool bInvertLaunchAxis = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Wave", meta = (ClampMin = "0.0"))
	float WaveSpeed = 800.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Wave", meta = (ClampMin = "0.0"))
	float WaveLifeSpan = 6.0f;

	/** 위력표에서 계수를 찾을 공격 태그입니다. 비우면 계수 1.0으로 처리됩니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Wave")
	FGameplayTag WaveAttackTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Wave")
	TSubclassOf<UGameplayEffect> WaveDamageEffectClass;

	/** 켜면 스폰 지점과 발사 방향을 화면에 그립니다. 축을 맞출 때 씁니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Wave|Debug")
	bool bDrawDebugWave = false;

private:
	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitGameplayEvent> WaveBlastTask;
};
