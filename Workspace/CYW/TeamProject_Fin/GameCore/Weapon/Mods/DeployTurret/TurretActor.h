#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TurretActor.generated.h"

class AWeaponBase;
class UAbilitySystemComponent;
class UGameplayEffect;
class UNiagaraComponent;
class UStaticMeshComponent;
class USceneComponent;
class UArrowComponent;
class UAudioComponent;
class UTurretMovementComponent;

class UTurretCombatComponent;
class UTurretPresentationComponent;

UCLASS()
class TEAMPROJECT_FIN_API ATurretActor : public AActor
{
	GENERATED_BODY()

public:
	ATurretActor();
	void Initialize(AWeaponBase* InWeapon, UAbilitySystemComponent* InASC,
		TSubclassOf<UGameplayEffect> InDamageEffect, float InAbilityLevel, float InDamage, int32 InAmmo);
	bool IsConfigured(const AWeaponBase* Weapon) const;

	void Deactivate();
	bool IsDeactivated() const { return bDeactivated; }
	UStaticMeshComponent* GetMesh() const { return Mesh; }
	USceneComponent* GetMuzzle() const { return Muzzle; }
	UNiagaraComponent* GetSpawnEffect() const { return SpawnEffect; }
	UNiagaraComponent* GetTrail() const { return Trail; }
	UAudioComponent* GetLoopAudio() const { return LoopAudio; }
	UTurretMovementComponent* GetMovement() const { return Movement; }
	UTurretCombatComponent* GetCombat() const { return Combat; }
	UTurretPresentationComponent* GetPresentation() const { return Presentation; }

protected:
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Turret")
	TObjectPtr<USceneComponent> SceneRoot;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Turret")
	TObjectPtr<UStaticMeshComponent> Mesh;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Turret")
	TObjectPtr<USceneComponent> Muzzle;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Turret")
	TObjectPtr<UArrowComponent> Direction;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Turret")
	TObjectPtr<UTurretMovementComponent> Movement;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Turret|FX")
	TObjectPtr<UNiagaraComponent> SpawnEffect;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Turret|FX")
	TObjectPtr<UNiagaraComponent> Trail;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Turret|Sound")
	TObjectPtr<UAudioComponent> LoopAudio;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Turret")
	TObjectPtr<UTurretCombatComponent> Combat;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Turret")
	TObjectPtr<UTurretPresentationComponent> Presentation;

	UPROPERTY(EditDefaultsOnly, Category = "Turret|Deactivate", meta = (ClampMin = "0.1", Units = "s"))
	float DestroyDelay = 3.0f;
	UPROPERTY(EditDefaultsOnly, Category = "Turret|Deactivate", meta = (ClampMin = "0.0"))
	float DropImpulse = 300.0f;
private:
	UFUNCTION()
	void OnRep_Deactivated();
	UPROPERTY(ReplicatedUsing = OnRep_Deactivated)
	bool bDeactivated = false;
};
