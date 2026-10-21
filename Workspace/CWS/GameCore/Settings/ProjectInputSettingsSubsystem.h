#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "GameCore/Settings/ProjectInputSettingsTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "ProjectInputSettingsSubsystem.generated.h"

class UProjectInputSaveGame;

DECLARE_MULTICAST_DELEGATE_TwoParams(FOnProjectInputModeChanged, EProjectInputAction, EProjectInputMode);

UCLASS(BlueprintType)
class TEAMPROJECT_FIN_API UProjectInputSettingsSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	UFUNCTION(BlueprintPure, Category = "Input|Settings")
	float GetLookSensitivity() const;

	UFUNCTION(BlueprintCallable, Category = "Input|Settings")
	void SetLookSensitivity(float Value);

	UFUNCTION(BlueprintPure, Category = "Input|Settings")
	float GetAimSensitivity() const;

	UFUNCTION(BlueprintCallable, Category = "Input|Settings")
	void SetAimSensitivity(float Value);

	UFUNCTION(BlueprintPure, Category = "Input|Settings")
	EProjectInputMode GetInputMode(EProjectInputAction Action) const;

	UFUNCTION(BlueprintCallable, Category = "Input|Settings")
	void SetInputMode(EProjectInputAction Action, EProjectInputMode Mode);

	UFUNCTION(BlueprintCallable, Category = "Input|Settings")
	void ResetInputSettings();

	UFUNCTION(BlueprintCallable, Category = "Input|Settings")
	bool SaveInputSettings();

	// 입력 방식이 바뀌면 캐릭터가 이전 유지 상태를 종료한다.
	FOnProjectInputModeChanged OnInputModeChanged;

private:
	friend class FProjectInputSettingsTest;

	void ScheduleSave();
	void StopPendingSave();
	bool SaveWhenIdle(float DeltaTime);

	UPROPERTY(Transient)
	TObjectPtr<UProjectInputSaveGame> SaveData;

	FTSTicker::FDelegateHandle SaveTickerHandle;
	float SecondsUntilSave = 0.0f;
	bool bSettingsDirty = false;
};
