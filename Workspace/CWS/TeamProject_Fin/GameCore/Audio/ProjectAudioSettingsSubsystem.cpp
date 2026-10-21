#include "GameCore/Audio/ProjectAudioSettingsSubsystem.h"

#include "AudioModulationStatics.h"
#include "Containers/Ticker.h"
#include "Engine/World.h"
#include "FMODStudioModule.h"
#include "GameCore/Audio/ProjectAudioSaveGame.h"
#include "GameCore/Audio/ProjectAudioSettings.h"
#include "HAL/PlatformTime.h"
#include "Kismet/GameplayStatics.h"
#include "SoundControlBus.h"
#include "SoundControlBusMix.h"
#include "UObject/UObjectGlobals.h"
#include "fmod_studio.hpp"

DEFINE_LOG_CATEGORY_STATIC(LogProjectAudioSettings, Log, All);

namespace
{
	const FString AudioSlot = TEXT("ProjectAudioSettings");
	constexpr float FMODRetryInterval = 0.1f;
	constexpr double FMODRetryDuration = 10.0;
	constexpr float SaveDelay = 0.5f;

	float* FindValue(UProjectAudioSaveGame* Data, EProjectAudioCategory Category)
	{
		if (!Data) return nullptr;
		switch (Category)
		{
		case EProjectAudioCategory::Master: return &Data->Master;
		case EProjectAudioCategory::Music: return &Data->Music;
		case EProjectAudioCategory::SFX: return &Data->SFX;
		case EProjectAudioCategory::UI: return &Data->UI;
		default: return nullptr;
		}
	}
}

void UProjectAudioSettingsSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	SaveData = Cast<UProjectAudioSaveGame>(UGameplayStatics::LoadGameFromSlot(AudioSlot, 0));
	if (!SaveData)
	{
		SaveData = Cast<UProjectAudioSaveGame>(UGameplayStatics::CreateSaveGameObject(UProjectAudioSaveGame::StaticClass()));
	}
	if (SaveData)
	{
		for (float* Value : { &SaveData->Master, &SaveData->Music, &SaveData->SFX, &SaveData->UI })
		{
			*Value = FMath::IsFinite(*Value) ? FMath::Clamp(*Value, 0.0f, 1.0f) : 1.0f;
		}
	}

	const UProjectAudioSettings* Config = GetDefault<UProjectAudioSettings>();
	UserMix = Config->UserMix.LoadSynchronous();
	MasterBus = Config->MasterBus.LoadSynchronous();
	MusicBus = Config->MusicBus.LoadSynchronous();
	SFXBus = Config->SFXBus.LoadSynchronous();
	UIBus = Config->UIBus.LoadSynchronous();

	PostLoadMapHandle = FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &ThisClass::OnWorldLoaded);
	ApplyVolumes();
}

void UProjectAudioSettingsSubsystem::Deinitialize()
{
	StopFMODRetry();
	if (SaveTickerHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(SaveTickerHandle);
		SaveTickerHandle.Reset();
		SaveAudioSettings();
	}
	FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(PostLoadMapHandle);
	Super::Deinitialize();
}

float UProjectAudioSettingsSubsystem::GetVolume(EProjectAudioCategory Category) const
{
	const float* Value = FindValue(SaveData, Category);
	return Value ? *Value : 1.0f;
}

void UProjectAudioSettingsSubsystem::SetVolume(EProjectAudioCategory Category, float Value)
{
	float* Current = FindValue(SaveData, Category);
	if (!Current || !FMath::IsFinite(Value)) return;
	Value = FMath::Clamp(Value, 0.0f, 1.0f);
	if (FMath::IsNearlyEqual(*Current, Value)) return;

	*Current = Value;
	ApplyVolumes();
	OnVolumeChanged.Broadcast(Category, Value);
	ScheduleSave();
}

void UProjectAudioSettingsSubsystem::ScheduleSave()
{
	SecondsUntilSave = SaveDelay;
	if (!SaveTickerHandle.IsValid())
	{
		SaveTickerHandle = FTSTicker::GetCoreTicker().AddTicker(
			FTickerDelegate::CreateUObject(this, &ThisClass::SaveWhenIdle));
	}
}

void UProjectAudioSettingsSubsystem::SaveAudioSettings()
{
	if (SaveData)
	{
		UGameplayStatics::SaveGameToSlot(SaveData, AudioSlot, 0);
	}
}

void UProjectAudioSettingsSubsystem::ResetAudioSettings()
{
	SetVolume(EProjectAudioCategory::Master, 1.0f);
	SetVolume(EProjectAudioCategory::Music, 1.0f);
	SetVolume(EProjectAudioCategory::SFX, 1.0f);
	SetVolume(EProjectAudioCategory::UI, 1.0f);
	SaveAudioSettings();
}

void UProjectAudioSettingsSubsystem::ApplyVolumes()
{
	ApplyUnrealVolumes();
	if (ApplyFMODVolume())
	{
		StopFMODRetry();
	}
	else if (!FMODRetryTickerHandle.IsValid())
	{
		// Bank 로드가 늦어져도 저장된 볼륨을 적용할 수 있도록 제한된 시간만 재시도한다.
		FMODRetryDeadline = FPlatformTime::Seconds() + FMODRetryDuration;
		FMODRetryTickerHandle = FTSTicker::GetCoreTicker().AddTicker(
			FTickerDelegate::CreateUObject(this, &ThisClass::RetryFMODVolume), FMODRetryInterval);
	}
}

void UProjectAudioSettingsSubsystem::StopFMODRetry()
{
	if (FMODRetryTickerHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(FMODRetryTickerHandle);
		FMODRetryTickerHandle.Reset();
	}
}

bool UProjectAudioSettingsSubsystem::ApplyFMODVolume()
{
	if (!IFMODStudioModule::IsAvailable()) return false;
	FMOD::Studio::System* System = IFMODStudioModule::Get().GetStudioSystem(EFMODSystemContext::Runtime);
	if (!System) return false;

	// Bus 포인터는 Bank 재로드 시 무효화될 수 있으므로 매번 조회한다.
	FMOD::Studio::Bus* Bus = nullptr;
	if (System->getBus(TCHAR_TO_UTF8(TEXT("bus:/")), &Bus) != FMOD_OK || !Bus) return false;

	const float Volume = GetVolume(EProjectAudioCategory::Master)
		* GetVolume(EProjectAudioCategory::Music);
	return Bus->setVolume(Volume) == FMOD_OK;
}

bool UProjectAudioSettingsSubsystem::RetryFMODVolume(float DeltaTime)
{
	if (ApplyFMODVolume())
	{
		FMODRetryTickerHandle.Reset();
		return false;
	}
	if (FPlatformTime::Seconds() < FMODRetryDeadline) return true;

	if (!bLoggedFMODFailure)
	{
		UE_LOG(LogProjectAudioSettings, Warning,
			TEXT("FMOD Master Bus 볼륨을 10초 동안 적용하지 못했습니다. Runtime 시스템과 Master Bank 로드를 확인하세요."));
		bLoggedFMODFailure = true;
	}
	FMODRetryTickerHandle.Reset();
	return false;
}

void UProjectAudioSettingsSubsystem::ApplyUnrealVolumes()
{
	if (!GetWorld() || !UserMix || !MasterBus || !MusicBus || !SFXBus || !UIBus) return;

	TArray<FSoundControlBusMixStage> Stages;
	Stages.Add(UAudioModulationStatics::CreateBusMixStage(this, MasterBus, GetVolume(EProjectAudioCategory::Master)));
	Stages.Add(UAudioModulationStatics::CreateBusMixStage(this, MusicBus, GetVolume(EProjectAudioCategory::Music)));
	Stages.Add(UAudioModulationStatics::CreateBusMixStage(this, SFXBus, GetVolume(EProjectAudioCategory::SFX)));
	Stages.Add(UAudioModulationStatics::CreateBusMixStage(this, UIBus, GetVolume(EProjectAudioCategory::UI)));
	UAudioModulationStatics::ActivateBusMix(this, UserMix);
	UAudioModulationStatics::UpdateMix(this, UserMix, Stages, 0.05f);
}

void UProjectAudioSettingsSubsystem::OnWorldLoaded(UWorld* LoadedWorld)
{
	if (LoadedWorld && LoadedWorld->GetGameInstance() == GetGameInstance())
	{
		ApplyVolumes();
	}
}

bool UProjectAudioSettingsSubsystem::SaveWhenIdle(float DeltaTime)
{
	SecondsUntilSave -= DeltaTime;
	if (SecondsUntilSave > 0.0f) return true;
	SaveAudioSettings();
	SaveTickerHandle.Reset();
	return false;
}
