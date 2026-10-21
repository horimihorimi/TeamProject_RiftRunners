#pragma once

#include "CoreMinimal.h"
#include "GCN_NiagaraSoundCueBase.h"
#include "GCN_AttachNiagaraAndSound.generated.h"

class UAudioComponent;
class AWeaponBase;
class UNiagaraComponent;
class UNiagaraSystem;
class USoundAttenuation;
class USoundBase;
class UWeaponComponent;

UENUM(BlueprintType)
enum class EGameplayCueAttachTarget : uint8
{
	TargetRoot,
	TargetFeet,
	EquippedWeapon
};

UCLASS(Blueprintable)
class TEAMPROJECT_FIN_API AGCN_AttachNiagaraAndSound : public AGCN_NiagaraSoundCueBase
{
	GENERATED_BODY()

public:
	AGCN_AttachNiagaraAndSound();
	virtual void Tick(float DeltaSeconds) override;

	virtual bool OnExecute_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) override;
	virtual bool OnActive_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) override;
	virtual bool WhileActive_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) override;
	virtual bool OnRemove_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) override;
	virtual bool Recycle() override;

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cue|Attach")
	EGameplayCueAttachTarget AttachTarget = EGameplayCueAttachTarget::TargetRoot;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cue|VFX")
	TObjectPtr<UNiagaraSystem> Effect;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cue|Sound")
	TObjectPtr<USoundBase> Sound;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cue|Sound", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float SoundVolume = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cue|Sound", meta = (ClampMin = "0.1", UIMin = "0.1"))
	float SoundPitch = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cue|Sound")
	TObjectPtr<USoundAttenuation> SoundAttenuation;

private:
	UFUNCTION()
	void OnBurstEffectFinished(UNiagaraComponent* FinishedComponent);

	UFUNCTION()
	void OnBurstSoundFinished();

	bool StartEffects(AActor* MyTarget, const FGameplayCueParameters& Parameters);
	void TryFinishBurst();
	void BindWeaponChanged(AActor* MyTarget, const FGameplayCueParameters& Parameters);
	void UnbindWeaponChanged();

	UFUNCTION()
	void OnEquippedWeaponChanged(AWeaponBase* NewWeapon);
	void StopEffects();
	USceneComponent* ResolveAttachComponent(
		AActor* MyTarget,
		const FGameplayCueParameters& Parameters,
		FName& OutSocketName,
		FVector& OutRelativeLocation) const;

	bool bExecutedBurst = false;
	bool bBurstEffectFinished = true;
	bool bBurstSoundFinished = true;

	UPROPERTY()
	TObjectPtr<UNiagaraComponent> EffectComponent;

	UPROPERTY()
	TObjectPtr<UAudioComponent> SoundComponent;

	TWeakObjectPtr<UWeaponComponent> BoundWeaponComponent;
	TWeakObjectPtr<AActor> ActiveTarget;
	FGameplayCueParameters ActiveParameters;
};
