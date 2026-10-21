#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "GameCore/Weapon/Data/WeaponFeedbackTypes.h"
#include "RocketActor.generated.h"

class UAbilitySystemComponent;
class UGameplayEffect;
class USphereComponent;
class UStaticMeshComponent;
class UArrowComponent;
class UProjectileMovementComponent;
class UProjectilePoolComponent;
class UNiagaraComponent;
class UAudioComponent;

UENUM()
enum class ERocketState : uint8
{
	Inactive,
	Spawning,
	Ready,
	Launched,
	Detonated
};

UCLASS()
class TEAMPROJECT_FIN_API ARocketActor : public AActor
{
	GENERATED_BODY()

public:
	ARocketActor();
	void ActivateFromPool(UProjectilePoolComponent* InPool);
	void DeactivateToPool();
	bool IsActiveRocket() const { return State != ERocketState::Inactive; }
	bool CanLaunch() const;
	void SetStackIndex(int32 InIndex) { StackIndex = InIndex; }
	void Prepare(UAbilitySystemComponent* InASC, int32 InStackIndex);
	void SetTarget(AActor* InTarget) { Target = InTarget; }
	AActor* GetTarget() const { return Target.Get(); }
	void Launch(const FVector& AimLocation, TSubclassOf<UGameplayEffect> InDamageEffect, float InAbilityLevel);

protected:
	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void LifeSpanExpired() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rocket")
	TObjectPtr<USphereComponent> Collision;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rocket")
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rocket")
	TObjectPtr<UArrowComponent> Direction;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rocket")
	TObjectPtr<UNiagaraComponent> Trail;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rocket")
	TObjectPtr<UNiagaraComponent> Beam;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rocket")
	TObjectPtr<UNiagaraComponent> SpawnEffect;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rocket")
	TObjectPtr<UAudioComponent> FlightAudio;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rocket")
	TObjectPtr<UProjectileMovementComponent> Movement;

	UPROPERTY(EditDefaultsOnly, Category = "Rocket|Spawn", meta = (ClampMin = "0.0", Units = "s"))
	float MeshRevealDelay = 0.15f;

	UPROPERTY(EditDefaultsOnly, Category = "Rocket|Beam")
	FName BeamEndParameter = TEXT("User.Beam End");

	UPROPERTY(EditDefaultsOnly, Category = "Rocket|Formation")
	FVector FollowOffset = FVector(0.0f, 90.0f, 40.0f);

	UPROPERTY(EditDefaultsOnly, Category = "Rocket|Formation", meta = (ClampMin = "0.0"))
	float StackSpacing = 35.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Rocket|Formation", meta = (ClampMin = "0.1"))
	float FollowSpeed = 10.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Rocket|Formation", meta = (ClampMin = "0.0", Units = "cm"))
	float HorizontalOffsetRange = 12.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Rocket|Formation", meta = (ClampMin = "0.0", Units = "cm"))
	float HoverAmplitude = 6.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Rocket|Formation", meta = (ClampMin = "0.0", Units = "Hz"))
	float HoverFrequency = 0.6f;

	UPROPERTY(EditDefaultsOnly, Category = "Rocket|Flight", meta = (ClampMin = "1.0"))
	float FlightSpeed = 2200.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Rocket|Flight", meta = (ClampMin = "0.1"))
	float FlightLifetime = 8.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Rocket|Damage", meta = (ClampMin = "0.0"))
	float Damage = 50.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Rocket|Damage", meta = (ClampMin = "1.0"))
	float ExplosionRadius = 300.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Rocket|Cues", meta = (Categories = "GameplayCue"))
	FGameplayTag LaunchCue;

	UPROPERTY(EditDefaultsOnly, Category = "Rocket|Cues", meta = (Categories = "GameplayCue"))
	FGameplayTag ExplosionCue;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rocket|Sound")
	FWeaponSoundSettings SpawnSound;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rocket|Sound")
	FWeaponSoundSettings LaunchSound;
	// Each sound asset must itself be configured to loop.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rocket|Sound")
	FWeaponSoundSettings FlightSound;

private:
	UFUNCTION()
	void OnHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse,
		const FHitResult& Hit);

	UFUNCTION()
	void OnRep_State();

	UFUNCTION()
	void UpdateBeam();
	void SetFlightEffects(bool bEnabled);
	void PlaySoundAtLocation(const FWeaponSoundSettings& Settings, const FVector& Location, const FRotator& Rotation) const;
	void RevealMesh();

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastPlayLaunchSound(FVector_NetQuantize Location);
	void ExecuteCue(FGameplayTag Tag, const FVector& Location);
	FVector GetFormationLocation() const;

	UPROPERTY(ReplicatedUsing = OnRep_State)
	ERocketState State = ERocketState::Inactive;

	UPROPERTY()
	TObjectPtr<UProjectilePoolComponent> OwningPool;

	UPROPERTY()
	TObjectPtr<UAbilitySystemComponent> SourceASC;

	UPROPERTY()
	TSubclassOf<UGameplayEffect> DamageEffect;

	UPROPERTY(ReplicatedUsing = UpdateBeam)
	TWeakObjectPtr<AActor> Target;
	int32 StackIndex = 0;
	FTimerHandle MeshRevealTimer;
	FVector HorizontalOffset = FVector::ZeroVector;
	float HoverPhase = 0.0f;
	float AbilityLevel = 1.0f;
};
