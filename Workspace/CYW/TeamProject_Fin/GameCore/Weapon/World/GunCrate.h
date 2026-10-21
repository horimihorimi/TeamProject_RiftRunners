#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameCore/Interface/Interactable.h"
#include "GameCore/Weapon/Data/WeaponTypes.h"
#include "GunCrate.generated.h"

class UInteractTriggerComponent;
class USkeletalMeshComponent;
class UAnimMontage;
class USoundBase;
class USoundAttenuation;
class AGunObject;

UCLASS()
class TEAMPROJECT_FIN_API AGunCrate : public AActor, public IInteractable
{
	GENERATED_BODY()

public:
	AGunCrate();
	virtual void Execute(APlayerController* PC) override;
	virtual FText GetInteractText() const override;

protected:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Crate")
	TObjectPtr<USceneComponent> CrateRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Crate", meta = (DisplayName = "Main"))
	TObjectPtr<USkeletalMeshComponent> CrateMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Crate", meta = (DisplayName = "Cap"))
	TObjectPtr<USkeletalMeshComponent> CapMesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crate|Presentation")
	TObjectPtr<UAnimMontage> OpeningMontage;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crate|Presentation")
	TObjectPtr<USoundBase> OpeningSound;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crate|Presentation", meta = (ClampMin = "0.0"))
	float SoundVolume = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crate|Presentation")
	TObjectPtr<USoundAttenuation> SoundAttenuation;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Crate")
	TObjectPtr<UInteractTriggerComponent> InteractTrigger;

	/** Forward controls ejection direction. Position this at the crate's outlet in BP. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Crate")
	TObjectPtr<USceneComponent> GunSpawnPoint;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crate|Gun")
	TSubclassOf<AGunObject> GunObjectClass;

	/** Relative rarity weights for each personal gun. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crate|Gun", meta = (ClampMin = "0.0"))
	TMap<EGunRarity, float> RarityWeights;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crate|Gun", meta = (ClampMin = "0.0"))
	float EjectionSpeed = 350.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crate|Gun", meta = (ClampMin = "0.0"))
	float EjectionUpwardSpeed = 180.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crate")
	FText InteractText;

private:
	UPROPERTY(ReplicatedUsing = OnRep_Opened)
	bool bIsOpened = false;

	UFUNCTION()
	void OnRep_Opened();
};
