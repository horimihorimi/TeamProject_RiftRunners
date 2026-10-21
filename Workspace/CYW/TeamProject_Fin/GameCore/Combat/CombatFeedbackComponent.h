#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CombatFeedbackTypes.h"
#include "CombatFeedbackComponent.generated.h"

class UAbilitySystemComponent;
class APlayerController;
class APawn;
struct FWeaponRuntimeStats;
struct FGameplayEventData;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnDamageFeedbackBatch, const TArray<FDamageFeedbackData>&);

UCLASS(ClassGroup = (Combat), meta = (BlueprintSpawnableComponent))
class TEAMPROJECT_FIN_API UCombatFeedbackComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCombatFeedbackComponent();
	void RebindToPawn(APawn* NewPawn);
	FOnDamageFeedbackBatch& OnDamageFeedbackReceived() { return DamageFeedbackReceived; }
	void ApplyWeaponRecoil(const FWeaponRuntimeStats& RuntimeStats, const UAbilitySystemComponent* SourceASC) const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void UnbindDamageDealtEvent();
	void OnDamageDealt(const FGameplayEventData* Payload);
	FVector ResolveDamageWorldLocation(const FGameplayEventData& Payload) const;
	void QueueDamageFeedback(const FDamageFeedbackData& Feedback);
	void FlushPendingFeedback();

	UFUNCTION(Client, Unreliable)
	void ClientReceiveDamageFeedbackBatch(const TArray<FDamageFeedbackData>& FeedbackBatch);

	UPROPERTY(EditDefaultsOnly, Category = "Damage Feedback|Network", meta = (ClampMin = "0.0", ClampMax = "0.1"))
	float BatchIntervalSeconds = 0.033f;

	UPROPERTY(EditDefaultsOnly, Category = "Damage Feedback|Network", meta = (ClampMin = "1", ClampMax = "64"))
	int32 MaxBatchSize = 16;

	TWeakObjectPtr<UAbilitySystemComponent> BoundAbilitySystemComponent;
	TWeakObjectPtr<APlayerController> OwningPlayerController;
	FDelegateHandle PawnChangedDelegateHandle;
	FDelegateHandle DamageDealtDelegateHandle;
	FTimerHandle BatchFlushTimerHandle;
	TArray<FDamageFeedbackData> PendingFeedback;
	FOnDamageFeedbackBatch DamageFeedbackReceived;
};
