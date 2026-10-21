#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "GameCore/Audio/ProjectAudioTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "ProjectAudioSettingsSubsystem.generated.h"

class UProjectAudioSaveGame;
class USoundControlBus;
class USoundControlBusMix;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnProjectVolumeChanged, EProjectAudioCategory, Category, float, Value);

UCLASS(BlueprintType)
class TEAMPROJECT_FIN_API UProjectAudioSettingsSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	UFUNCTION(BlueprintCallable, Category = "Audio|Settings")
	float GetVolume(EProjectAudioCategory Category) const;

	UFUNCTION(BlueprintCallable, Category = "Audio|Settings")
	void SetVolume(EProjectAudioCategory Category, float Value);

	UFUNCTION(BlueprintCallable, Category = "Audio|Settings")
	void SaveAudioSettings();

	UFUNCTION(BlueprintCallable, Category = "Audio|Settings")
	void ResetAudioSettings();

	UPROPERTY(BlueprintAssignable, Category = "Audio|Settings")
	FOnProjectVolumeChanged OnVolumeChanged;

private:
	void ApplyVolumes();
	void ApplyUnrealVolumes();
	bool ApplyFMODVolume();
	bool RetryFMODVolume(float DeltaTime);
	void StopFMODRetry();
	void ScheduleSave();
	void OnWorldLoaded(UWorld* LoadedWorld);
	bool SaveWhenIdle(float DeltaTime);

	UPROPERTY(Transient)
	TObjectPtr<UProjectAudioSaveGame> SaveData;

	UPROPERTY(Transient)
	TObjectPtr<USoundControlBusMix> UserMix;

	UPROPERTY(Transient)
	TObjectPtr<USoundControlBus> MasterBus;

	UPROPERTY(Transient)
	TObjectPtr<USoundControlBus> MusicBus;

	UPROPERTY(Transient)
	TObjectPtr<USoundControlBus> SFXBus;

	UPROPERTY(Transient)
	TObjectPtr<USoundControlBus> UIBus;

	FTSTicker::FDelegateHandle SaveTickerHandle;
	FTSTicker::FDelegateHandle FMODRetryTickerHandle;
	FDelegateHandle PostLoadMapHandle;
	float SecondsUntilSave = 0.0f;
	double FMODRetryDeadline = 0.0;
	bool bLoggedFMODFailure = false;
};
