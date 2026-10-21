// Copyright Epic Games, Inc. All Rights Reserved.

#include "TeamProject_FinEditor.h"

#include "Modules/ModuleManager.h"
#include "Framework/Docking/TabManager.h"
#include "JumpMap/SJumpMapLayoutRegistration.h"
#include "ToolMenus.h"
#include "Widgets/Docking/SDockTab.h"

namespace
{
	const FName JumpMapRegistrationTab(TEXT("JumpMapLayoutRegistration"));
}

class FTeamProjectFinEditorModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		// 쿠킹 등 창을 사용하지 않는 실행에서는 메뉴와 Slate 탭을 등록하지 않는다.
		if (IsRunningCommandlet())
		{
			return;
		}
		FGlobalTabmanager::Get()->RegisterNomadTabSpawner(JumpMapRegistrationTab,
			FOnSpawnTab::CreateRaw(this, &FTeamProjectFinEditorModule::SpawnRegistrationTab))
			.SetDisplayName(NSLOCTEXT("JumpMapEditor", "TabName", "점프맵 액터 일괄 등록"))
			.SetMenuType(ETabSpawnerMenuType::Hidden);
		UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(
			this, &FTeamProjectFinEditorModule::RegisterMenus));
		bTabRegistered = true;
	}

	virtual void ShutdownModule() override
	{
		if (!bTabRegistered)
		{
			return;
		}
		UToolMenus::UnRegisterStartupCallback(this);
		UToolMenus::UnregisterOwner(this);
		if (TSharedPtr<SDockTab> Tab = RegistrationTab.Pin())
		{
			Tab->RequestCloseTab();
		}
		FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(JumpMapRegistrationTab);
	}

private:
	bool bTabRegistered = false;
	TWeakPtr<SDockTab> RegistrationTab;

	TSharedRef<SDockTab> SpawnRegistrationTab(const FSpawnTabArgs& Args)
	{
		TSharedRef<SDockTab> Tab = SNew(SDockTab).TabRole(ETabRole::NomadTab)
		[
			SNew(SJumpMapLayoutRegistration)
		];
		RegistrationTab = Tab;
		return Tab;
	}

	void RegisterMenus()
	{
		FToolMenuOwnerScoped Owner(this);
		UToolMenu* Menu = UToolMenus::Get()->ExtendMenu(TEXT("LevelEditor.MainMenu.Tools"));
		Menu->FindOrAddSection(TEXT("JumpMap")).AddMenuEntry(
			TEXT("JumpMapLayoutRegistration"),
			NSLOCTEXT("JumpMapEditor", "MenuName", "점프맵 액터 일괄 등록"),
			NSLOCTEXT("JumpMapEditor", "MenuTooltip", "선택한 액터를 인원별 점프맵 배열에 등록합니다."),
			FSlateIcon(), FUIAction(FExecuteAction::CreateLambda([]()
			{
				FGlobalTabmanager::Get()->TryInvokeTab(JumpMapRegistrationTab);
			})));
	}
};

IMPLEMENT_MODULE(FTeamProjectFinEditorModule, TeamProject_FinEditor);
