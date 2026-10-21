#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "ANS_EnemyAttackPositioning.generated.h"

/** 공격 중 위치 보정의 기준점입니다. */
UENUM(BlueprintType)
enum class EEnemyAttackPositioningTarget : uint8
{
	CombatTarget UMETA(DisplayName = "Combat Target"),
	CenterAnchor UMETA(DisplayName = "Center Anchor"),
};

/**
 * 루트 모션이 없는 공격 애님 구간에서 비행 적의 공격 시작 위치를 보정한다.
 *
 * Combat Target은 타깃과의 거리를, Center Anchor는 보스 전투방 중앙 앵커와의 거리를 맞춘다.
 */
UCLASS(meta = (DisplayName = "Enemy Attack Positioning"))
class TEAMPROJECT_FIN_API UANS_EnemyAttackPositioning : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	virtual FString GetNotifyName_Implementation() const override;
	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		float TotalDuration, const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyTick(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		float FrameDeltaTime, const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;

	/** 이동할 기준점입니다. Center Anchor는 보스에서만 사용할 수 있습니다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy Attack Positioning")
	EEnemyAttackPositioningTarget PositioningTarget = EEnemyAttackPositioningTarget::CombatTarget;

	/** 타깃과 이 거리보다 멀 때만 수평 접근한다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy Attack Positioning",
		meta = (EditCondition = "PositioningTarget == EEnemyAttackPositioningTarget::CombatTarget", ClampMin = "0.0"))
	float AttackDistance = 500.0f;

	/** 끄면 AI 타깃을 조회하지 않고, 지면 기준 고도만 보정한다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy Attack Positioning",
		meta = (EditCondition = "PositioningTarget == EEnemyAttackPositioningTarget::CombatTarget"))
	bool bAdjustTargetDistance = true;

	/** 중앙 앵커와 이 거리보다 멀 때만 수평 접근한다. 0이면 앵커 정중앙으로 이동한다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy Attack Positioning",
		meta = (EditCondition = "PositioningTarget == EEnemyAttackPositioningTarget::CenterAnchor", ClampMin = "0.0"))
	float AnchorDistance = 0.0f;

	/** 현재 지면에서 유지할 고도다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy Attack Positioning", meta = (ClampMin = "0.0"))
	float HeightFromFloor = 300.0f;

	/** 공격 거리 판정의 허용 오차다. 이 안에서는 수평 이동하지 않는다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy Attack Positioning", meta = (ClampMin = "0.0"))
	float AcceptanceRadius = 50.0f;

	/** 0이면 비행 이동 컴포넌트의 MaxFlySpeed를 사용한다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy Attack Positioning", meta = (ClampMin = "0.0"))
	float SpeedOverride = 0.0f;

	/** 0이면 비행 이동 컴포넌트의 MaxAcceleration을 사용한다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy Attack Positioning", meta = (ClampMin = "0.0"))
	float AccelerationOverride = 0.0f;
};
