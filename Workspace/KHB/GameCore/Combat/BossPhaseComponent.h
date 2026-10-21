#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "BossPhaseComponent.generated.h"

class UBossPhaseSetDataAsset;
class UEnemyPatternSetDataAsset;
struct FEnemyBossPhaseDefinition;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FOnBossPhaseTransitionRequested,
	int32,
	NextPhaseIndex,
	FGameplayTag,
	NextPhaseId);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FOnBossPhaseChanged,
	int32,
	CurrentPhaseIndex,
	FGameplayTag,
	CurrentPhaseId);

/**
 * 보스의 단방향 페이즈 진행만 관리합니다.
 *
 * 패턴 교체, 전환 GA 실행, StateTree 전이는 이 컴포넌트의 이벤트를 받는
 * 보스 전투 프레임워크가 담당합니다. 따라서 페이즈 판정 규칙이 Character나
 * StateTree에 중복되지 않습니다.
 */
UCLASS(ClassGroup = (Enemy), BlueprintType, meta = (BlueprintSpawnableComponent))
class TEAMPROJECT_FIN_API UBossPhaseComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UBossPhaseComponent();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** 보스 Definition에서 받은 페이즈 세트를 초기화합니다. */
	UFUNCTION(BlueprintCallable, Category = "Boss|Phase")
	bool InitializePhaseSet(UBossPhaseSetDataAsset* InPhaseSet);

	/** 체력 비율이 다음 페이즈의 임계값을 넘었는지 검사하고 전환을 요청합니다. */
	UFUNCTION(BlueprintCallable, Category = "Boss|Phase")
	bool TryRequestPhaseTransition(float HealthRatio);

	/** 전환 GA 종료 뒤 호출합니다. 이 시점에만 현재 페이즈가 바뀝니다. */
	UFUNCTION(BlueprintCallable, Category = "Boss|Phase")
	bool CompletePhaseTransition();

	/** 현재 전환의 다음 페이즈까지 이미 체력이 내려갔는지 검사한다. 전환 자체는 완료하지 않는다. */
	bool ShouldPreemptTransitionForHealth(float HealthRatio) const;

	UFUNCTION(BlueprintPure, Category = "Boss|Phase")
	int32 GetCurrentPhaseIndex() const { return CurrentPhaseIndex; }

	UFUNCTION(BlueprintPure, Category = "Boss|Phase")
	bool IsPhaseTransitionInProgress() const { return bTransitionInProgress; }

	UFUNCTION(BlueprintPure, Category = "Boss|Phase")
	FGameplayTag GetCurrentPhaseId() const;

	UFUNCTION(BlueprintPure, Category = "Boss|Phase")
	FGameplayTag GetPendingPhaseId() const;

	/** 전환 중인 다음 페이즈가 실행해야 할 GA의 Asset Tag입니다. */
	UFUNCTION(BlueprintPure, Category = "Boss|Phase")
	FGameplayTag GetPendingTransitionAbilityTag() const;

	/** 현재 페이즈가 CombatLoop에 제공하는 PatternSet입니다. */
	UFUNCTION(BlueprintPure, Category = "Boss|Phase")
	UEnemyPatternSetDataAsset* GetCurrentPatternSet() const;

	/** 현재 페이즈의 Form/패턴/이동 오버라이드 정의입니다. */
	const FEnemyBossPhaseDefinition* GetCurrentPhaseDefinition() const;

	UPROPERTY(BlueprintAssignable, Category = "Boss|Phase")
	FOnBossPhaseTransitionRequested OnPhaseTransitionRequested;

	UPROPERTY(BlueprintAssignable, Category = "Boss|Phase")
	FOnBossPhaseChanged OnPhaseChanged;

private:
	UFUNCTION()
	void OnRep_CurrentPhaseIndex();

	UPROPERTY(Transient)
	TObjectPtr<UBossPhaseSetDataAsset> PhaseSet;

	UPROPERTY(ReplicatedUsing = OnRep_CurrentPhaseIndex)
	int32 CurrentPhaseIndex = INDEX_NONE;

	UPROPERTY(Replicated)
	int32 PendingPhaseIndex = INDEX_NONE;

	UPROPERTY(Replicated)
	bool bTransitionInProgress = false;
};
