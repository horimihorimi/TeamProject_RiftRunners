#pragma once

#include "CoreMinimal.h"
#include "GameplayCueNotify_Static.h"
#include "GCN_SelfDestruct.generated.h"

class UNiagaraSystem;
class USoundAttenuation;
class USoundBase;

UCLASS(Blueprintable)
class TEAMPROJECT_FIN_API UGCN_SelfDestruct : public UGameplayCueNotify_Static
{
	GENERATED_BODY()

public:
	UGCN_SelfDestruct();

	virtual bool OnExecute_Implementation(
		AActor* MyTarget,
		const FGameplayCueParameters& Parameters) const override;

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SelfDestruct|VFX")
	TObjectPtr<UNiagaraSystem> ExplosionEffect;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SelfDestruct|VFX", meta = (ClampMin = "0.01"))
	float EffectRadius = 300.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SelfDestruct|Sound")
	TObjectPtr<USoundBase> ExplosionSound;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SelfDestruct|Sound")
	TObjectPtr<USoundAttenuation> SoundAttenuation;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SelfDestruct|Sound", meta = (ClampMin = "0.0"))
	float SoundVolume = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SelfDestruct|Sound", meta = (ClampMin = "0.1"))
	float SoundPitch = 1.0f;
};
