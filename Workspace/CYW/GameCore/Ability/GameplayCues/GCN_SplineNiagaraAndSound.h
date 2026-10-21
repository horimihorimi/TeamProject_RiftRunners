#pragma once

#include "CoreMinimal.h"
#include "GCN_NiagaraSoundCueBase.h"
#include "GCN_SplineNiagaraAndSound.generated.h"

class UAudioComponent;
class UNiagaraComponent;
class UNiagaraSystem;
class USceneComponent;
class USoundAttenuation;
class USoundBase;
class USplineComponent;

UCLASS(Blueprintable)
class TEAMPROJECT_FIN_API AGCN_SplineNiagaraAndSound : public AGCN_NiagaraSoundCueBase
{
	GENERATED_BODY()

public:
	AGCN_SplineNiagaraAndSound();
	virtual void Tick(float DeltaSeconds) override;

	virtual bool OnExecute_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) override;
	virtual bool Recycle() override;

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cue|VFX")
	TObjectPtr<UNiagaraSystem> Effect;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cue|VFX")
	FName SplineUserParameter = TEXT("User.Spline");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cue|Spline", meta = (ClampMin = "0", UIMin = "0"))
	int32 IntermediatePointCount = 3;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cue|Spline", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float HorizontalJitter = 120.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cue|Spline", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float VerticalJitter = 80.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cue|Lifetime", meta = (ClampMin = "0.01", UIMin = "0.01"))
	float CueLifetime = 0.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cue|Sound")
	TObjectPtr<USoundBase> Sound;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cue|Sound")
	TArray<TObjectPtr<USoundBase>> RandomSounds;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cue|Sound", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float SoundVolume = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cue|Sound", meta = (ClampMin = "0.1", UIMin = "0.1"))
	float SoundPitchMin = 0.95f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cue|Sound", meta = (ClampMin = "0.1", UIMin = "0.1"))
	float SoundPitchMax = 1.05f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cue|Sound")
	TObjectPtr<USoundAttenuation> SoundAttenuation;

private:
	UFUNCTION()
	void FinishCue();

	UFUNCTION()
	void OnEffectFinished(UNiagaraComponent* FinishedComponent);

	UFUNCTION()
	void OnSoundFinished();

	void InitializeSplineJitter(const FVector& Start, const FVector& End);
	void BuildSpline(const FVector& Start, const FVector& End);
	void DrawSplineDebug(float Duration) const;
	void UpdateTrackedEndpoints();
	void TryFinishCue();
	void StopEffects();

	UPROPERTY(VisibleAnywhere, Category = "Cue")
	TObjectPtr<USceneComponent> CueRoot;

	UPROPERTY(VisibleAnywhere, Category = "Cue")
	TObjectPtr<USplineComponent> SplineComponent;

	UPROPERTY()
	TObjectPtr<UNiagaraComponent> EffectComponent;

	UPROPERTY()
	TObjectPtr<UAudioComponent> SoundComponent;

	bool bEffectFinished = true;
	bool bSoundFinished = true;
	bool bShowDebug = false;
	FVector StartRelativeLocation = FVector::ZeroVector;
	FVector EndRelativeLocation = FVector::ZeroVector;
	FVector LastStartLocation = FVector::ZeroVector;
	FVector LastEndLocation = FVector::ZeroVector;
	TArray<FVector2D> JitterSamples;
	TWeakObjectPtr<const AActor> StartTarget;
	TWeakObjectPtr<const AActor> EndTarget;
};
