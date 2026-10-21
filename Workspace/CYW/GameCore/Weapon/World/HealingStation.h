// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameCore/Interface/Interactable.h"
#include "HealingStation.generated.h"

class UGameplayEffect;
class UInteractTriggerComponent;
class UStaticMeshComponent;

UCLASS()
class TEAMPROJECT_FIN_API AHealingStation : public AActor, public IInteractable
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AHealingStation();

	virtual void Execute(APlayerController* PC) override;
	virtual FText GetInteractText() const override;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "HealingStation|Purchase", meta = (ClampMin = "0"))
	int32 HealingCost = 1;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HealingStation|Healing")
	TSubclassOf<UGameplayEffect> HealEffectClass;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "HealingStation")
	TObjectPtr<USceneComponent> HealingStationRoot;

	/** Visual mesh for the station. Assign the actual station mesh in BP. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "HealingStation")
	TObjectPtr<UStaticMeshComponent> HealingStationMesh;

	/** Interaction sphere. Its radius controls the usable range. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "HealingStation")
	TObjectPtr<UInteractTriggerComponent> InteractTrigger;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "HealingStation")
	FText InteractText;
};
