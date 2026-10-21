#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameCore/Weapon/Data/WeaponTypes.h"
#include "TurretCombatComponent.generated.h"

class ATurretActor;
class AWeaponBase;
class AWeaponProjectile;
class UAbilitySystemComponent;
class UGameplayEffect;
class UProjectilePoolComponent;
class UTurretMovementComponent;
struct FGameplayCueParameters;

UENUM(BlueprintType)
enum class EDeployTurretState : uint8
{
	Following,
	Intercepting,
	Attacking,
	Repositioning,
	Deactivated
};

UCLASS(ClassGroup = (Weapon), meta = (BlueprintSpawnableComponent))
class TEAMPROJECT_FIN_API UTurretCombatComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UTurretCombatComponent();
	void Initialize(AWeaponBase* InWeapon, UAbilitySystemComponent* InASC,
		TSubclassOf<UGameplayEffect> InDamageEffect, float InAbilityLevel, float InDamage, int32 InAmmo);
	bool IsConfigured(const AWeaponBase* Weapon) const;

	bool Start();
	void Stop();
	void NotifyTargetDetected();

protected:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Turret|Combat")
	TSubclassOf<AWeaponProjectile> ProjectileClass;
	UPROPERTY(EditDefaultsOnly, Category = "Turret|Combat", meta = (ToolTip = "Rifle burst size. Sniper rifles and grenade launchers always fire once per position."))
	TMap<EWeaponCategory, int32> ShotsPerPosition;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Turret|Combat", meta = (ClampMin = "0.0", ClampMax = "90.0", Units = "deg"))
	float SpreadAngle = 15.0f;
	UPROPERTY(EditDefaultsOnly, Category = "Turret|Targeting", meta = (ClampMin = "1.0", Units = "cm"))
	float SearchRadius = 2000.0f;
	UPROPERTY(EditDefaultsOnly, Category = "Turret|Targeting", meta = (ClampMin = "0.05", Units = "s"))
	float SearchInterval = 0.2f;
	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category = "Turret|State")
	EDeployTurretState State = EDeployTurretState::Following;
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Turret|State")
	int32 CurrentAmmo = 0;
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Turret|State")
	float Damage = 0.0f;

private:
	ATurretActor* GetTurret() const;
	AActor* FindNearestEnemy() const;
	bool IsValidTarget(const AActor* Target) const;
	bool HasLineOfSight(const AActor* Target) const;
	void SearchForTarget();
	void StartIntercept(AActor* Target);
	void UpdateCombat(float DeltaTime);
	void Reposition();
	bool FireAtTarget();
	bool FireProjectile(const FTransform& FireTransform);
	void FireHitScan(const FTransform& FireTransform);
	void PlayFireCue(FGameplayCueParameters Parameters) const;
	void SetState(EDeployTurretState NewState);
	UFUNCTION()
	void OnRep_CurrentTarget();

	UPROPERTY()
	TObjectPtr<UTurretMovementComponent> Movement;
	UPROPERTY()
	TObjectPtr<AWeaponBase> SourceWeapon;
	UPROPERTY()
	TObjectPtr<UAbilitySystemComponent> SourceASC;
	UPROPERTY()
	TSubclassOf<UGameplayEffect> DamageEffect;
	UPROPERTY()
	TObjectPtr<UProjectilePoolComponent> ProjectilePool;
	FWeaponRuntimeStats ShotStats;
	UPROPERTY(ReplicatedUsing = OnRep_CurrentTarget)
	TWeakObjectPtr<AActor> CurrentTarget;
	FTimerHandle SearchTimer;
	float AbilityLevel = 1.0f;
	double LastShotTime = -9999.0;
	int32 BurstSize = 3;
	int32 ShotsFiredAtPosition = 0;
};
