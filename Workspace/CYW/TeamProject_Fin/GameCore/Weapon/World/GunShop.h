#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameCore/Interface/Interactable.h"
#include "GunShop.generated.h"

class UInteractTriggerComponent;
class UStaticMeshComponent;
class AGunObject;

/** Interaction spawns one personal random weapon using the centralized shop generation policy. */
UCLASS()
class TEAMPROJECT_FIN_API AGunShop : public AActor, public IInteractable
{
	GENERATED_BODY()

public:
	AGunShop();
	virtual void Execute(APlayerController* PC) override;
	virtual FText GetInteractText() const override;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Shop|Purchase", meta=(ClampMin="0"))
	int32 WeaponCost = 1;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Shop")
	TObjectPtr<USceneComponent> ShopRoot;

	/** Visual mesh for the shop. Assign the actual shop mesh in BP. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Shop")
	TObjectPtr<UStaticMeshComponent> ShopMesh;

	/** Interaction sphere. Its radius controls the usable range. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Shop")
	TObjectPtr<UInteractTriggerComponent> InteractTrigger;

	/** Move this component to the shop's physical gun outlet in BP. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Shop")
	TObjectPtr<USceneComponent> GunSpawnPoint;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Shop|Gun")
	TSubclassOf<AGunObject> GunObjectClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Shop|Gun", meta=(ClampMin="0.0"))
	float EjectionSpeed = 350.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Shop|Gun", meta=(ClampMin="0.0"))
	float EjectionUpwardSpeed = 180.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Shop")
	FText InteractText;

};
