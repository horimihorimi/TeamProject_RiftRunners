#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "EnemySplitComponent.generated.h"
class ACommonEnemyCharacter;
class ABaseCharacter;
struct FGameplayEffectContextHandle;

/** 분열 계보가 공유하는 서버 체력 상태입니다. 조우나 클리어 이벤트는 처리하지 않습니다. */
UCLASS()
class TEAMPROJECT_FIN_API UEnemySplitHealthState : public UObject
{
	GENERATED_BODY()

public:
	virtual UWorld* GetWorld() const override;

	void Initialize(ACommonEnemyCharacter* Root);

	void RemoveMember(ACommonEnemyCharacter* Enemy);

	bool HasMembers() const
	{
		return !Members.IsEmpty();
	}

	FText GetTraitDisplayName() const;

	float GetHealth() const
	{
		return Health;
	}

	float GetMaxHealth() const
	{
		return MaxHealth;
	}

	float ApplySharedDamage(float Damage);

	void UpdateSharedVitals(float InHealth, float InMaxHealth);

	void FinishSharedDeath(AActor* HitEnemy, AActor* InstigatorActor, const FGameplayEffectContextHandle& Context);

private:
	void AddMember(ACommonEnemyCharacter* Enemy);

	void OnSplit(ACommonEnemyCharacter* Parent, ACommonEnemyCharacter* Left, ACommonEnemyCharacter* Right);

	void OnDeathStarted(ABaseCharacter* Character);

	void SyncHealth();

	UPROPERTY()
	float Health = 0.f;

	UPROPERTY()
	float MaxHealth = 0.f;

	TArray<TWeakObjectPtr<ACommonEnemyCharacter>> Members;
};

UCLASS(ClassGroup = (Enemy), meta = (BlueprintSpawnableComponent))
class TEAMPROJECT_FIN_API UEnemySplitComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UEnemySplitComponent();

	UPROPERTY(Transient)
	TObjectPtr<UEnemySplitHealthState> SharedHealthPool;

protected:
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Split")
	bool bShareHealth = false;

	void InitializeSharedHealth();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Split", meta = (ClampMin = "1"))
	int32 MaxSplitCount = 8;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Split", meta = (ClampMin = "0.1"))
	float SplitDelay = 10.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Split",
	          meta = (ClampMin = "0.01", ClampMax = "1.0"))
	float SplitRatio = 0.75f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Split", meta = (ClampMin = "1.0", Units = "cm"))
	float SplitOffset = 100.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Split", meta = (ClampMin = "1.0", Units = "cm"))
	float SplitSearchRadius = 500.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Split", meta = (ClampMin = "1", ClampMax = "8"))
	int32 SplitSearchRings = 5;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Split", meta = (ClampMin = "1.0", Units = "cm"))
	float SplitSearchHeight = 200.f;

	UFUNCTION(BlueprintPure, Category = "Enemy|Split")
	bool IsSplitReady() const;

	bool TrySplit();

	void ResetSplit();

	void StartSplitDelay();

	void InitializeChild(const UEnemySplitComponent* Parent, int32 InLeafBudget);

	bool IsSplitChild() const
	{
		return bSplitChild;
	}

	bool HasSplitCapacity() const
	{
		return LeafBudget != 1;
	}

private:
	// 각 분열 가지는 최종 개체 수 예산을 가집니다. 분열 시 나누며 사망해도 예산을 복구하지 않습니다.
	int32 LeafBudget = 0;

	bool bSplitChild = false;

	bool bSplitUsed = false;

	mutable double ReadyTime = -1.;
};
