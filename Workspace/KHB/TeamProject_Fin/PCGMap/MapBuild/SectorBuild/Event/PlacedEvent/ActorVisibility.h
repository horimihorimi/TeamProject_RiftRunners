#pragma once
#include "CoreMinimal.h"
#include "../SectorEvent.h"
#include "ActorVisibility.generated.h"

/** 액터를 삭제하지 않고 표시/충돌 상태를 전환하는 레벨 배치 이벤트. */
UCLASS(Blueprintable)
class TEAMPROJECT_FIN_API AActorVisibility : public AScenePlacedEvent
{
	GENERATED_BODY()
public:
	AActorVisibility();
	virtual void Execute(const UObject* WorldContext = nullptr) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(EditInstanceOnly, Category="Actor Visibility")
	TArray<TObjectPtr<AActor>> ShowActors;
	UPROPERTY(EditInstanceOnly, Category="Actor Visibility")
	TArray<TObjectPtr<AActor>> HideActors;
	UPROPERTY(EditInstanceOnly, Category="Actor Visibility")
	bool bApplyOnBeginPlay = false;
	UPROPERTY(EditInstanceOnly, Category="Actor Visibility")
	bool bUpdateCollision = true;

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Actor Visibility")
	void ApplyVisibility();
protected:
	virtual void BeginPlay() override;
private:
	UPROPERTY(ReplicatedUsing=OnRep_ExecutionRevision)
	int64 ExecutionRevision = 0;
	UFUNCTION()
	void OnRep_ExecutionRevision();
	UFUNCTION(NetMulticast, Reliable)
	void MulticastApply(int64 Revision);
	void ApplyLocally(int64 Revision);
};
