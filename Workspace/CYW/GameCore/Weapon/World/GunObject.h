#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameCore/Interface/Interactable.h"
#include "GameCore/Weapon/Data/WeaponTypes.h"
#include "GameCore/Weapon/Generation/WeaponGenerationTypes.h"
#include "GunObject.generated.h"

class AWeaponBase;
class UDataTable;
class UInteractTriggerComponent;
class UNiagaraComponent;
class UNiagaraSystem;
class UStaticMeshComponent;
class UWeaponDataAsset;
class UGunObjectVisualComponent;
class UGunPickupUIComponent;

/** Replicated personal weapon pickup. Generation is completed before this actor is spawned. */
UCLASS()
class TEAMPROJECT_FIN_API AGunObject : public AActor, public IInteractable
{
	GENERATED_BODY()

public:
	AGunObject();
	virtual void BeginPlay() override;
	virtual void Execute(APlayerController* PC) override;
	virtual FText GetInteractText() const override;

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Gun Object", meta=(WorldContext="WorldContextObject"))
	static AGunObject* SpawnRandomShopWeapon(
		UObject* WorldContextObject,
		APlayerController* OwningPlayer,
		const FTransform& Transform,
		TSubclassOf<AGunObject> GunObjectClass = nullptr);

	/** Creates a personal weapon drop for the supplied eligible player. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Gun Object", meta=(WorldContext="WorldContextObject"))
	static AGunObject* SpawnRandomEnemyDrop(
		UObject* WorldContextObject,
		APlayerController* EligiblePlayer,
		const FTransform& Transform,
		TSubclassOf<AGunObject> GunObjectClass = nullptr);

	/** Shared generation entry point; optional weights override the pickup class's enemy rarity weights. */
	static AGunObject* SpawnGeneratedWeapon(
		UObject* WorldContextObject,
		APlayerController* OwningPlayer,
		const FTransform& Transform,
		TSubclassOf<AGunObject> GunObjectClass,
		EWeaponGenerationSource Source,
		const TMap<EGunRarity, float>* RarityWeights = nullptr);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Gun Object")
	void LaunchFromShop(const FVector& EjectionVelocity);

	UFUNCTION(BlueprintPure, Category="Gun Object")
	const FWeaponRuntimeStats& GetResolvedStats() const { return GeneratedData.RuntimeStats; }

	UFUNCTION(BlueprintPure, Category="Gun Object")
	EGunRarity GetRarity() const { return GeneratedData.Rarity; }

	UFUNCTION(BlueprintPure, Category="Gun Object")
	EWeaponCategory GetWeaponCategory() const { return GeneratedData.WeaponCategory; }

	UFUNCTION(BlueprintPure, Category="Gun Object")
	FName GetFrameName() const { return GeneratedData.FrameName; }

	UFUNCTION(BlueprintPure, Category="Gun Object")
	FName GetBarrelName() const { return GeneratedData.BarrelName; }

	UFUNCTION(BlueprintPure, Category="Gun Object")
	FText GetGunName() const { return GeneratedData.GunName; }

	const FGeneratedWeaponData& GetGeneratedData() const { return GeneratedData; }
	const UDataTable* GetModifierTable() const { return ModifierTable; }
	const UDataTable* GetBarrelTable() const { return BarrelTable; }
	FVector GetPickupDisplayLocation() const;
	bool CanInteract(const APlayerController* PC) const;

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Gun Object|Swap Drop", meta=(ClampMin="0.0", Units="cm/s"))
	float SwapOutwardSpeed = 350.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Gun Object|Swap Drop", meta=(ClampMin="0.0", Units="cm/s"))
	float SwapUpwardSpeed = 180.0f;

	/** Relative weights for enemy drops only. Zero disables a rarity; all zero falls back to Common. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Gun Object|Enemy Drop", meta=(ClampMin="0.0"))
	TMap<EGunRarity, float> EnemyDropRarityWeights;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Gun Object|Enemy Drop", meta=(ClampMin="0.0", Units="cm/s"))
	float DropOutwardSpeed = 180.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Gun Object|Enemy Drop", meta=(ClampMin="0.0", Units="cm/s"))
	float DropUpwardSpeed = 300.0f;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void OnRep_Owner() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Gun Object")
	TObjectPtr<USceneComponent> GunRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Gun Object")
	TObjectPtr<UStaticMeshComponent> PhysicsMeshComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Gun Object")
	TObjectPtr<UStaticMeshComponent> FrameMeshComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Gun Object")
	TObjectPtr<UStaticMeshComponent> BarrelMeshComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Gun Object")
	TObjectPtr<UInteractTriggerComponent> InteractTrigger;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Gun Object")
	TObjectPtr<UGunObjectVisualComponent> PickupVisualComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Gun Object")
	TObjectPtr<UGunPickupUIComponent> GunPickupUIComponent;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gun Object|Data")
	TObjectPtr<UDataTable> FrameTable;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gun Object|Data")
	TObjectPtr<UDataTable> BarrelTable;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gun Object|Data")
	TObjectPtr<UDataTable> ModifierTable;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gun Object|Data")
	TObjectPtr<const UWeaponDataAsset> WeaponTemplate;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gun Object|Data")
	TSubclassOf<AWeaponBase> WeaponClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gun Object|Presentation")
	TMap<EGunRarity, TObjectPtr<UNiagaraSystem>> RarityEffects;

	UPROPERTY(ReplicatedUsing=OnRep_GeneratedData, BlueprintReadOnly, Category="Gun Object")
	FGeneratedWeaponData GeneratedData;

private:
	// Server-only ammo snapshot. INDEX_NONE uses initial ammo for a newly generated gun.
	int32 CurrentAmmo = INDEX_NONE;

	UFUNCTION()
	void OnRep_GeneratedData();

	void ApplyGeneratedState();
	void RefreshLocalVisibility();
	void RefreshRarityEffect();

	UPROPERTY(Transient)
	TObjectPtr<UNiagaraComponent> ActiveRarityEffect;
};
