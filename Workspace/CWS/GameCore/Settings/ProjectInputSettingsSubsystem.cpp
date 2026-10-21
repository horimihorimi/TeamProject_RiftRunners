#include "GameCore/Settings/ProjectInputSettingsSubsystem.h"

#include "Engine/GameInstance.h"
#include "GameCore/Settings/ProjectInputSaveGame.h"
#include "Kismet/GameplayStatics.h"

DEFINE_LOG_CATEGORY_STATIC(LogProjectInputSettings, Log, All);

namespace
{
	const FString InputSettingsSlot = TEXT("ProjectInputSettings");
	constexpr int32 SettingsUserIndex = 0;
	constexpr float DefaultSensitivity = 1.0f;
	constexpr float MinSensitivity = 0.1f;
	constexpr float MaxSensitivity = 3.0f;
	constexpr float SaveDelay = 0.5f;

	bool IsValidInputMode(EProjectInputMode Mode)
	{
		return Mode == EProjectInputMode::Hold || Mode == EProjectInputMode::Toggle;
	}

	float SanitizeSensitivity(float Value)
	{
		return FMath::IsFinite(Value) ? FMath::Clamp(Value, MinSensitivity, MaxSensitivity) : DefaultSensitivity;
	}

	EProjectInputMode* FindInputMode(UProjectInputSaveGame* Data, EProjectInputAction Action)
	{
		if (!Data)
		{
			return nullptr;
		}
		switch (Action)
		{
		case EProjectInputAction::Sprint: return &Data->SprintMode;
		case EProjectInputAction::Crouch: return &Data->CrouchMode;
		case EProjectInputAction::Aim: return &Data->AimMode;
		default: return nullptr;
		}
	}
}

bool UProjectInputSettingsSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UGameInstance* GameInstance = Cast<UGameInstance>(Outer);
	return Super::ShouldCreateSubsystem(Outer) && GameInstance && !GameInstance->IsDedicatedServerInstance();
}

void UProjectInputSettingsSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	if (UGameplayStatics::DoesSaveGameExist(InputSettingsSlot, SettingsUserIndex))
	{
		SaveData = Cast<UProjectInputSaveGame>(UGameplayStatics::LoadGameFromSlot(InputSettingsSlot, SettingsUserIndex));
		if (!SaveData)
		{
			UE_LOG(LogProjectInputSettings, Warning, TEXT("입력 설정을 불러오지 못해 기본값을 사용합니다."));
		}
	}
	if (!SaveData)
	{
		SaveData = Cast<UProjectInputSaveGame>(UGameplayStatics::CreateSaveGameObject(UProjectInputSaveGame::StaticClass()));
	}
	if (!SaveData)
	{
		UE_LOG(LogProjectInputSettings, Error, TEXT("입력 설정 저장 객체를 생성하지 못했습니다."));
		return;
	}

	SaveData->LookSensitivity = SanitizeSensitivity(SaveData->LookSensitivity);
	SaveData->AimSensitivity = SanitizeSensitivity(SaveData->AimSensitivity);
	for (EProjectInputMode* Mode : { &SaveData->SprintMode, &SaveData->CrouchMode, &SaveData->AimMode })
	{
		if (!IsValidInputMode(*Mode))
		{
			*Mode = EProjectInputMode::Hold;
		}
	}
}

void UProjectInputSettingsSubsystem::Deinitialize()
{
	StopPendingSave();
	if (bSettingsDirty)
	{
		SaveInputSettings();
	}
	OnInputModeChanged.Clear();
	Super::Deinitialize();
}

float UProjectInputSettingsSubsystem::GetLookSensitivity() const
{
	return SaveData ? SaveData->LookSensitivity : DefaultSensitivity;
}

void UProjectInputSettingsSubsystem::SetLookSensitivity(float Value)
{
	if (!SaveData || !FMath::IsFinite(Value))
	{
		return;
	}
	Value = SanitizeSensitivity(Value);
	if (SaveData->LookSensitivity != Value)
	{
		SaveData->LookSensitivity = Value;
		ScheduleSave();
	}
}

float UProjectInputSettingsSubsystem::GetAimSensitivity() const
{
	return SaveData ? SaveData->AimSensitivity : DefaultSensitivity;
}

void UProjectInputSettingsSubsystem::SetAimSensitivity(float Value)
{
	if (!SaveData || !FMath::IsFinite(Value))
	{
		return;
	}
	Value = SanitizeSensitivity(Value);
	if (SaveData->AimSensitivity != Value)
	{
		SaveData->AimSensitivity = Value;
		ScheduleSave();
	}
}

EProjectInputMode UProjectInputSettingsSubsystem::GetInputMode(EProjectInputAction Action) const
{
	const EProjectInputMode* Mode = FindInputMode(SaveData, Action);
	return Mode ? *Mode : EProjectInputMode::Hold;
}

void UProjectInputSettingsSubsystem::SetInputMode(EProjectInputAction Action, EProjectInputMode Mode)
{
	EProjectInputMode* Current = FindInputMode(SaveData, Action);
	if (!Current || !IsValidInputMode(Mode) || *Current == Mode)
	{
		return;
	}
	*Current = Mode;
	ScheduleSave();
	OnInputModeChanged.Broadcast(Action, Mode);
}

void UProjectInputSettingsSubsystem::ResetInputSettings()
{
	SetLookSensitivity(DefaultSensitivity);
	SetAimSensitivity(DefaultSensitivity);
	SetInputMode(EProjectInputAction::Sprint, EProjectInputMode::Hold);
	SetInputMode(EProjectInputAction::Crouch, EProjectInputMode::Hold);
	SetInputMode(EProjectInputAction::Aim, EProjectInputMode::Hold);
}

void UProjectInputSettingsSubsystem::ScheduleSave()
{
	bSettingsDirty = true;
	SecondsUntilSave = SaveDelay;
	if (!SaveTickerHandle.IsValid())
	{
		SaveTickerHandle = FTSTicker::GetCoreTicker().AddTicker(
			FTickerDelegate::CreateUObject(this, &ThisClass::SaveWhenIdle));
	}
}

void UProjectInputSettingsSubsystem::StopPendingSave()
{
	if (SaveTickerHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(SaveTickerHandle);
		SaveTickerHandle.Reset();
	}
}

bool UProjectInputSettingsSubsystem::SaveInputSettings()
{
	StopPendingSave();
	if (!SaveData || !UGameplayStatics::SaveGameToSlot(SaveData, InputSettingsSlot, SettingsUserIndex))
	{
		UE_LOG(LogProjectInputSettings, Warning, TEXT("입력 설정 저장에 실패했습니다. 이번 실행의 설정값은 유지합니다."));
		bSettingsDirty = true;
		return false;
	}
	bSettingsDirty = false;
	return true;
}

bool UProjectInputSettingsSubsystem::SaveWhenIdle(float DeltaTime)
{
	SecondsUntilSave -= DeltaTime;
	if (SecondsUntilSave > 0.0f)
	{
		return true;
	}
	SaveTickerHandle.Reset();
	SaveInputSettings();
	return false;
}
