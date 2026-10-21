#pragma once

#include "CoreMinimal.h"
#include "GameplayCueNotify_Static.h"
#include "GCN_SpawnNiagaraAndSound.generated.h"

class UNiagaraSystem;
class USoundAttenuation;
class USoundBase;

UCLASS(Blueprintable)
class TEAMPROJECT_FIN_API UGCN_SpawnNiagaraAndSound : public UGameplayCueNotify_Static
{
	GENERATED_BODY()

public:
	virtual bool OnExecute_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) const override;

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cue|VFX")
	TObjectPtr<UNiagaraSystem> Effect;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cue|Sound")
	TObjectPtr<USoundBase> Sound;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cue|Sound")
	TArray<TObjectPtr<USoundBase>> RandomSounds;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cue|Sound", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float SoundVolume = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cue|Sound", meta = (ClampMin = "0.1", UIMin = "0.1"))
	float SoundPitch = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cue|Sound")
	TObjectPtr<USoundAttenuation> SoundAttenuation;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cue|Transform")
	bool bUseTargetRotation = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cue|Transform")
	bool bUseParameterLocation = true;

private:
};
