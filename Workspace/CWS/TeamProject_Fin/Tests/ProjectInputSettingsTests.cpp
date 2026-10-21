#if WITH_DEV_AUTOMATION_TESTS

#include "AbilitySystemComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameCore/Ability/GA_Aim.h"
#include "GameCore/Ability/GA_Sprint.h"
#include "GameCore/Attribute/CharacterAttributeSet.h"
#include "GameCore/BasePlayerController.h"
#include "GameCore/BasePlayerState.h"
#include "GameCore/Character/PlayerCharacter.h"
#include "GameCore/Settings/ProjectInputSaveGame.h"
#include "GameCore/Settings/ProjectInputSettingsSubsystem.h"
#include "GameCore/Tags/GameplayTagSystem.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "TimerManager.h"
#include "UObject/StrongObjectPtr.h"

#include <limits>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectInputSettingsTest, "TeamProject.InputSettings.StateAndPersistence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FProjectInputSettingsTest::RunTest(const FString& Parameters)
{
	// 사용자 저장 슬롯 대신 임시 객체와 메모리 직렬화를 사용한다.
	TStrongObjectPtr<UGameInstance> GameInstance(NewObject<UGameInstance>());
	TStrongObjectPtr<UProjectInputSettingsSubsystem> Settings(NewObject<UProjectInputSettingsSubsystem>(GameInstance.Get()));
	Settings->SaveData = NewObject<UProjectInputSaveGame>(Settings.Get());
	ON_SCOPE_EXIT
	{
		Settings->StopPendingSave();
		Settings->bSettingsDirty = false;
	};

	TestEqual(TEXT("일반 감도 기본값"), Settings->GetLookSensitivity(), 1.0f);
	TestEqual(TEXT("조준 감도 기본값"), Settings->GetAimSensitivity(), 1.0f);
	Settings->SetLookSensitivity(-10.0f);
	Settings->SetAimSensitivity(10.0f);
	TestEqual(TEXT("일반 감도 하한"), Settings->GetLookSensitivity(), 0.1f);
	TestEqual(TEXT("조준 감도 상한"), Settings->GetAimSensitivity(), 3.0f);
	Settings->SetLookSensitivity(std::numeric_limits<float>::quiet_NaN());
	Settings->SetAimSensitivity(std::numeric_limits<float>::infinity());
	TestEqual(TEXT("NaN 입력 무시"), Settings->GetLookSensitivity(), 0.1f);
	TestEqual(TEXT("무한대 입력 무시"), Settings->GetAimSensitivity(), 3.0f);
	Settings->SetInputMode(EProjectInputAction::Sprint, static_cast<EProjectInputMode>(255));
	TestTrue(TEXT("잘못된 입력 모드 무시"), Settings->GetInputMode(EProjectInputAction::Sprint) == EProjectInputMode::Hold);
	Settings->SetLookSensitivity(1.5f);
	Settings->SetAimSensitivity(0.7f);
	TestTrue(TEXT("변경 후 저장 대기"), Settings->SaveWhenIdle(0.2f));
	Settings->SetAimSensitivity(0.8f);
	TestEqual(TEXT("추가 변경 후 저장 지연 갱신"), Settings->SecondsUntilSave, 0.5f);
	Settings->SetAimSensitivity(0.7f);
	Settings->StopPendingSave();

	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("입력 테스트 임시 월드"), World))
	{
		return false;
	}
	ON_SCOPE_EXIT { World->DestroyWorld(false); };
	ABasePlayerController* Controller = World->SpawnActor<ABasePlayerController>();
	APlayerCharacter* Character = World->SpawnActor<APlayerCharacter>();
	if (!TestNotNull(TEXT("테스트 컨트롤러"), Controller) || !TestNotNull(TEXT("테스트 캐릭터"), Character))
	{
		return false;
	}
	Controller->PlayerState = World->SpawnActor<ABasePlayerState>();
	// 뷰포트가 없는 임시 월드에서도 실제 로컬 입력 분기를 사용한다.
	Controller->SetAsLocalPlayerController();
	Controller->Possess(Character);
	TestTrue(TEXT("로컬 입력 가능"), Character->IsLocallyControlled());
	Character->InputSettings = Settings.Get();
	Character->InputModeChangedHandle = Settings->OnInputModeChanged.AddUObject(Character, &APlayerCharacter::OnInputModeChanged);
	Character->GetAbilitySystemComponent()->AddAttributeSetSubobject(Character->GetCharacterAttributeSet());

	const auto WantsAction = [Character](EProjectInputAction Action)
	{
		switch (Action)
		{
		case EProjectInputAction::Sprint: return Character->bWantsToSprint;
		case EProjectInputAction::Crouch: return Character->bWantsToCrouch;
		case EProjectInputAction::Aim: return Character->bWantsToAim;
		default: return false;
		}
	};
	for (const EProjectInputAction Action : { EProjectInputAction::Sprint, EProjectInputAction::Crouch, EProjectInputAction::Aim })
	{
		Settings->SetInputMode(Action, EProjectInputMode::Hold);
		Character->HandlePersistentInputStarted(Action);
		TestTrue(TEXT("홀드 누르기"), WantsAction(Action));
		Character->HandlePersistentInputCompleted(Action);
		TestFalse(TEXT("홀드 놓기"), WantsAction(Action));
		Settings->SetInputMode(Action, EProjectInputMode::Toggle);
		Character->HandlePersistentInputStarted(Action);
		Character->HandlePersistentInputCompleted(Action);
		TestTrue(TEXT("토글 키 해제 후 유지"), WantsAction(Action));
		Character->EvaluateInputQueue();
		TestTrue(TEXT("입력 큐 재실행 시 토글 유지"), WantsAction(Action));
		Character->HandlePersistentInputStarted(Action);
		TestFalse(TEXT("두 번째 입력으로 토글 종료"), WantsAction(Action));
		Character->HandlePersistentInputStarted(Action);
		Character->HandlePersistentInputCanceled(Action);
		TestFalse(TEXT("취소 입력은 토글도 종료"), WantsAction(Action));
		Character->HandlePersistentInputStarted(Action);
		Settings->SetInputMode(Action, EProjectInputMode::Hold);
		TestFalse(TEXT("방식 변경은 기존 입력 해제"), WantsAction(Action));
	}

	// 실제 어빌리티 활성화와 태그 콜백을 통해 달리기의 조준 취소를 검증한다.
	UAbilitySystemComponent* ASC = Character->GetAbilitySystemComponent();
	Character->SprintAbilityClass = UGA_Sprint::StaticClass();
	Character->AimAbilityClass = UGA_Aim::StaticClass();
	const FGameplayAbilitySpecHandle SprintHandle = ASC->GiveAbility(FGameplayAbilitySpec(Character->SprintAbilityClass));
	const FGameplayAbilitySpecHandle AimHandle = ASC->GiveAbility(FGameplayAbilitySpec(Character->AimAbilityClass));
	// 소모 효과 대신 방향 입력과 달리기 입력의 조합으로 어빌리티 활성 여부를 검증한다.
	for (const EProjectInputMode SprintMode : { EProjectInputMode::Hold, EProjectInputMode::Toggle })
	{
		Settings->SetInputMode(EProjectInputAction::Sprint, SprintMode);
		Character->HandlePersistentInputStarted(EProjectInputAction::Sprint);
		if (SprintMode == EProjectInputMode::Toggle)
		{
			Character->HandlePersistentInputCompleted(EProjectInputAction::Sprint);
		}
		TestFalse(TEXT("방향 입력 없이 달리기 키만 누르면 달리기 비활성"), ASC->FindAbilitySpecFromHandle(SprintHandle)->IsActive());
		Character->EvaluateInputQueue();
		TestFalse(TEXT("방향 입력 없이 입력 큐가 달리기를 재개하지 않음"), ASC->FindAbilitySpecFromHandle(SprintHandle)->IsActive());
		Character->Move(FInputActionValue(FVector2D(0.0f, 1.0f)));
		TestTrue(TEXT("방향 입력과 달리기 입력이 함께 있으면 달리기 시작"), ASC->FindAbilitySpecFromHandle(SprintHandle)->IsActive());
		Character->StopMove();
		TestFalse(TEXT("방향 입력 종료 시 달리기 종료"), ASC->FindAbilitySpecFromHandle(SprintHandle)->IsActive());
		TestTrue(TEXT("방향 입력 종료 후 홀드/토글 의도 보존"), Character->bWantsToSprint);
		Character->EvaluateInputQueue();
		TestFalse(TEXT("방향 입력 종료 후 입력 큐에서 달리기 재활성화 방지"), ASC->FindAbilitySpecFromHandle(SprintHandle)->IsActive());
		Character->Move(FInputActionValue(FVector2D(1.0f, 0.0f)));
		TestTrue(TEXT("방향 입력 재개 시 유지 중인 달리기 재개"), ASC->FindAbilitySpecFromHandle(SprintHandle)->IsActive());
		Character->Move(FInputActionValue(FVector2D::ZeroVector));
		TestFalse(TEXT("방향 입력 값이 0이면 달리기 종료"), ASC->FindAbilitySpecFromHandle(SprintHandle)->IsActive());
		if (SprintMode == EProjectInputMode::Hold)
		{
			Character->HandlePersistentInputCompleted(EProjectInputAction::Sprint);
		}
		else
		{
			Character->HandlePersistentInputStarted(EProjectInputAction::Sprint);
		}
		Character->Move(FInputActionValue(FVector2D(0.0f, 1.0f)));
		TestFalse(TEXT("달리기 입력 해제 후 방향키만 누르면 달리기 비활성"), ASC->FindAbilitySpecFromHandle(SprintHandle)->IsActive());
		Character->ResetPersistentInput();
	}
	// 달리기 중 새 조준 입력이 달리기 의도까지 해제하는지 홀드/토글 조합별로 확인한다.
	for (const EProjectInputMode SprintMode : { EProjectInputMode::Hold, EProjectInputMode::Toggle })
	{
		Settings->SetInputMode(EProjectInputAction::Sprint, SprintMode);
		for (const EProjectInputMode AimMode : { EProjectInputMode::Hold, EProjectInputMode::Toggle })
		{
			Settings->SetInputMode(EProjectInputAction::Aim, AimMode);
			Character->Move(FInputActionValue(FVector2D(0.0f, 1.0f)));
			Character->HandlePersistentInputStarted(EProjectInputAction::Sprint);
			if (SprintMode == EProjectInputMode::Toggle)
			{
				Character->HandlePersistentInputCompleted(EProjectInputAction::Sprint);
			}
			TestTrue(TEXT("조준 전 달리기 활성화"), ASC->FindAbilitySpecFromHandle(SprintHandle)->IsActive());
			Character->HandlePersistentInputStarted(EProjectInputAction::Aim);
			TestFalse(TEXT("새 조준 입력으로 달리기 종료"), ASC->FindAbilitySpecFromHandle(SprintHandle)->IsActive());
			TestFalse(TEXT("새 조준 입력으로 달리기 재개 의도 해제"), Character->bWantsToSprint);
			TestTrue(TEXT("달리기에서 전환 후 조준 활성화"), ASC->FindAbilitySpecFromHandle(AimHandle)->IsActive());
			Character->EvaluateInputQueue();
			TestFalse(TEXT("입력 큐가 조준 중 달리기를 다시 켜지 않음"), ASC->FindAbilitySpecFromHandle(SprintHandle)->IsActive());
			TestTrue(TEXT("입력 큐 재평가 후 조준 유지"), ASC->FindAbilitySpecFromHandle(AimHandle)->IsActive());
			TestTrue(TEXT("입력 큐 재평가 후 조준 의도 유지"), Character->bWantsToAim);
			Character->StopMove();
			Character->Move(FInputActionValue(FVector2D(0.0f, 1.0f)));
			TestFalse(TEXT("방향 입력 재개 후에도 달리기 자동 재개 방지"), ASC->FindAbilitySpecFromHandle(SprintHandle)->IsActive());
			Character->HandlePersistentInputCompleted(EProjectInputAction::Aim);
			if (AimMode == EProjectInputMode::Toggle)
			{
				TestTrue(TEXT("조준 토글은 키 해제 후 유지"), ASC->FindAbilitySpecFromHandle(AimHandle)->IsActive());
				Character->HandlePersistentInputStarted(EProjectInputAction::Aim);
			}
			Character->EvaluateInputQueue();
			TestFalse(TEXT("조준 종료 후 달리기 자동 재개 방지"), ASC->FindAbilitySpecFromHandle(SprintHandle)->IsActive());
			Character->HandlePersistentInputStarted(EProjectInputAction::Sprint);
			TestTrue(TEXT("새 달리기 입력으로 다시 달리기 가능"), ASC->FindAbilitySpecFromHandle(SprintHandle)->IsActive());
			Character->ResetPersistentInput();
		}
	}
	Settings->SetInputMode(EProjectInputAction::Sprint, EProjectInputMode::Hold);
	for (const EProjectInputMode AimMode : { EProjectInputMode::Hold, EProjectInputMode::Toggle })
	{
		Settings->SetInputMode(EProjectInputAction::Aim, AimMode);
		Character->Move(FInputActionValue(FVector2D(0.0f, 1.0f)));
		Character->HandlePersistentInputStarted(EProjectInputAction::Aim);
		TestTrue(TEXT("달리기 전 조준 활성화"), ASC->FindAbilitySpecFromHandle(AimHandle)->IsActive());

		ASC->AddLooseGameplayTag(GameplayTagSystem::State_Debuff_CannotSprint);
		Character->HandlePersistentInputStarted(EProjectInputAction::Sprint);
		TestFalse(TEXT("달리기 차단 중 비활성"), ASC->FindAbilitySpecFromHandle(SprintHandle)->IsActive());
		TestTrue(TEXT("달리기 시작 실패 시 조준 의도 유지"), Character->bWantsToAim);
		TestTrue(TEXT("달리기 시작 실패 시 조준 유지"), ASC->FindAbilitySpecFromHandle(AimHandle)->IsActive());
		ASC->RemoveLooseGameplayTag(GameplayTagSystem::State_Debuff_CannotSprint);

		Character->EvaluateInputQueue();
		TestTrue(TEXT("입력 큐에서 달리기 시작"), ASC->FindAbilitySpecFromHandle(SprintHandle)->IsActive());
		TestFalse(TEXT("달리기 시작 시 기존 조준 의도 해제"), Character->bWantsToAim);
		TestFalse(TEXT("달리기 시작 시 조준 종료"), ASC->FindAbilitySpecFromHandle(AimHandle)->IsActive());
		Character->HandlePersistentInputCompleted(EProjectInputAction::Sprint);
		Character->EvaluateInputQueue();
		TestFalse(TEXT("달리기 종료 후 조준 자동 재개 방지"), ASC->FindAbilitySpecFromHandle(AimHandle)->IsActive());
		Character->HandlePersistentInputStarted(EProjectInputAction::Aim);
		TestTrue(TEXT("새 조준 입력으로 다시 조준 가능"), ASC->FindAbilitySpecFromHandle(AimHandle)->IsActive());
		Character->ResetPersistentInput();
	}
	ASC->ClearAbility(SprintHandle);
	ASC->ClearAbility(AimHandle);
	Character->SprintAbilityClass = nullptr;
	Character->AimAbilityClass = nullptr;

	// 실제 Look 경로의 회전 누적값으로 독립 감도 적용을 확인한다.
	Controller->RotationInput = FRotator::ZeroRotator;
	Character->bIsAiming = false;
	Character->Look(FInputActionValue(FVector2D(2.0f, 4.0f)));
	const FRotator NormalInput = Controller->RotationInput;
	Controller->RotationInput = FRotator::ZeroRotator;
	Character->bIsAiming = true;
	Character->Look(FInputActionValue(FVector2D(2.0f, 4.0f)));
	TestTrue(TEXT("일반 감도 회전 반영"), !FMath::IsNearlyZero(NormalInput.Yaw));
	// 입력 API의 float와 회전값의 double 사이에서 발생하는 반올림 오차를 허용한다.
	TestTrue(TEXT("조준 감도 독립 적용"), FMath::IsNearlyEqual(Controller->RotationInput.Yaw, NormalInput.Yaw * (0.7f / 1.5f), 1.e-6));
	TestTrue(TEXT("X/Y 동일 배율"), FMath::IsNearlyEqual(Controller->RotationInput.Pitch, NormalInput.Pitch * (0.7f / 1.5f), 1.e-6));
	Character->bIsAiming = false;

	Settings->SetInputMode(EProjectInputAction::Sprint, EProjectInputMode::Toggle);
	Character->bIsFiring = true;
	Character->HandlePersistentInputStarted(EProjectInputAction::Sprint);
	TestTrue(TEXT("사격 중 달리기 재개 의도 보존"), Character->bWantsToSprint);
	Character->StopFireInternal();
	TestTrue(TEXT("사격 종료 후 큐 예약"), World->GetTimerManager().IsTimerActive(Character->InputQueueTimerHandle));
	Character->ResetPersistentInput();
	Character->HandlePersistentInputStarted(EProjectInputAction::Sprint);
	FOnAttributeChangeData StaminaChange;
	StaminaChange.OldValue = 100.0f;
	StaminaChange.NewValue = 0.0f;
	Character->OnInputStaminaChanged(StaminaChange);
	TestFalse(TEXT("스태미나 고갈 시 토글 해제"), Character->bWantsToSprint);
	Character->GetCharacterAttributeSet()->SetStamina(0.0f);
	Character->HandlePersistentInputStarted(EProjectInputAction::Sprint);
	TestFalse(TEXT("고갈 상태에서 토글 재활성화 방지"), Character->bWantsToSprint);
	Character->bIsFiring = true;
	Character->HandlePersistentInputStarted(EProjectInputAction::Sprint);
	TestFalse(TEXT("사격 중에도 고갈 상태의 토글 예약 방지"), Character->bWantsToSprint);
	Character->StopFireInternal();
	Character->GetCharacterAttributeSet()->SetStamina(100.0f);

	Settings->SetInputMode(EProjectInputAction::Aim, EProjectInputMode::Toggle);
	Character->bAimInputUsesSmartTracking = true;
	Character->bWantsToAim = true;
	TestTrue(TEXT("스마트 트래킹은 홀드 우선"), Character->GetPersistentInputMode(EProjectInputAction::Aim) == EProjectInputMode::Hold);
	Character->HandlePersistentInputCompleted(EProjectInputAction::Aim);
	TestFalse(TEXT("스마트 트래킹 키 해제 시 의도 해제"), Character->bWantsToAim);
	Character->HandlePersistentInputStarted(EProjectInputAction::Aim);
	Character->OnInputWeaponEquipped(nullptr);
	TestFalse(TEXT("무기 교체 시 조준 해제"), Character->bWantsToAim);

	Character->bWantsToSprint = Character->bWantsToCrouch = Character->bWantsToAim = Character->bIsFiring = true;
	Controller->FlushPressedKeys();
	Controller->FlushPressedKeys();
	Character->EvaluateInputQueue();
	TestFalse(TEXT("입력 초기화 후 달리기 재개 방지"), Character->bWantsToSprint);
	TestFalse(TEXT("입력 초기화 후 방향 입력 상태 정리"), Character->bHasMovementInput);
	TestFalse(TEXT("입력 초기화 후 앉기 재개 방지"), Character->bWantsToCrouch);
	TestFalse(TEXT("입력 초기화 후 조준 재개 방지"), Character->bWantsToAim);
	TestFalse(TEXT("입력 초기화 후 사격 재개 방지"), Character->bIsFiring);
	Character->BeginDeath();
	Character->HandlePersistentInputStarted(EProjectInputAction::Aim);
	TestFalse(TEXT("사망 중 유지 입력 무시"), Character->bWantsToAim);
	Character->Revive();

	TArray<uint8> SerializedSettings;
	TestTrue(TEXT("설정 메모리 저장"), UGameplayStatics::SaveGameToMemory(Settings->SaveData, SerializedSettings));
	const UProjectInputSaveGame* Restored = Cast<UProjectInputSaveGame>(UGameplayStatics::LoadGameFromMemory(SerializedSettings));
	if (TestNotNull(TEXT("설정 메모리 복원"), Restored))
	{
		TestEqual(TEXT("일반 감도 저장 유지"), Restored->LookSensitivity, 1.5f);
		TestEqual(TEXT("조준 감도 저장 유지"), Restored->AimSensitivity, 0.7f);
		TestTrue(TEXT("입력 모드 저장 유지"), Restored->AimMode == EProjectInputMode::Toggle);
	}
	Settings->ResetInputSettings();
	TestEqual(TEXT("일반 감도 기본값 복원"), Settings->GetLookSensitivity(), 1.0f);
	TestEqual(TEXT("조준 감도 기본값 복원"), Settings->GetAimSensitivity(), 1.0f);
	Controller->UnPossess();
	TestFalse(TEXT("빙의 해제 시 설정 참조 정리"), Character->InputSettings.IsValid());
	TestFalse(TEXT("빙의 해제 시 콜백 정리"), Settings->OnInputModeChanged.IsBound());
	return !HasAnyErrors();
}

#endif
