#include "SJumpMapLayoutRegistration.h"

#include "Editor.h"
#include "Engine/Level.h"
#include "Environment/JumpMapLayoutController.h"
#include "ScopedTransaction.h"
#include "Subsystems/EditorActorSubsystem.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "JumpMapLayoutRegistration"

void SJumpMapLayoutRegistration::Construct(const FArguments& InArgs)
{
	ChildSlot
	[
		SNew(SBox).Padding(12.f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 12.f)
			[
				SNew(STextBlock).AutoWrapText(true)
				.Text(LOCTEXT("Instructions", "월드 아웃라이너에서 코스 액터를 여러 개 선택하세요.\n이 창에서 컨트롤러와 인원을 지정한 뒤 등록합니다."))
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 8.f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(1.f)
				[
					SAssignNew(ControllerCombo, SComboBox<FControllerOption>)
					.OptionsSource(&ControllerOptions)
					.OnGenerateWidget(this, &SJumpMapLayoutRegistration::MakeControllerOption)
					.OnSelectionChanged(this, &SJumpMapLayoutRegistration::SelectController)
					[
						SNew(STextBlock).Text(this, &SJumpMapLayoutRegistration::GetControllerLabel)
					]
				]
				+ SHorizontalBox::Slot().AutoWidth().Padding(8.f, 0.f)
				[
					SNew(SButton).Text(LOCTEXT("Refresh", "컨트롤러 새로고침"))
					.OnClicked(this, &SJumpMapLayoutRegistration::RefreshControllers)
				]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 12.f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 8.f, 0.f)
				[
					SNew(STextBlock).Text(LOCTEXT("PlayerCount", "등록할 인원"))
				]
				+ SHorizontalBox::Slot().AutoWidth()
				[
					SNew(SSpinBox<int32>).MinValue(1).MaxValue(4).MinSliderValue(1).MaxSliderValue(4)
					.MinDesiredWidth(70.f).Delta(1)
					.Value_Lambda([this]() { return PlayerCount; })
					.OnValueChanged_Lambda([this](int32 Value) { PlayerCount = FMath::Clamp(Value, 1, 4); })
				]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 12.f)
			[
				SNew(SButton).Text(LOCTEXT("Register", "선택 액터 등록"))
				.OnClicked(this, &SJumpMapLayoutRegistration::RegisterSelection)
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 8.f)
			[
				SNew(STextBlock).AutoWrapText(true)
				.Text(LOCTEXT("Preparation", "PrepareLayout을 이미 눌렀다면 RestoreLayout 후 등록하세요.\n등록 후 PrepareLayout을 누르고 맵을 저장하세요. 실행 취소: Ctrl+Z"))
			]
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(STextBlock).AutoWrapText(true).Text_Lambda([this]() { return Status; })
			]
		]
	];
	RefreshControllers();
}

FReply SJumpMapLayoutRegistration::RefreshControllers()
{
	const TWeakObjectPtr<AJumpMapLayoutController> Previous = SelectedController.IsValid()
		? SelectedController->Controller : TWeakObjectPtr<AJumpMapLayoutController>();
	ControllerOptions.Reset();
	SelectedController.Reset();
	if (GEditor)
	{
		if (UEditorActorSubsystem* Actors = GEditor->GetEditorSubsystem<UEditorActorSubsystem>())
		{
			for (AActor* Actor : Actors->GetAllLevelActors())
			{
				if (AJumpMapLayoutController* Controller = Cast<AJumpMapLayoutController>(Actor))
				{
					FControllerOption Option = MakeShared<FJumpMapControllerOption>();
					Option->Controller = Controller;
					ControllerOptions.Add(Option);
					if (Previous.Get() == Controller)
					{
						SelectedController = Option;
					}
				}
			}
		}
	}
	if (!SelectedController.IsValid() && ControllerOptions.Num() == 1)
	{
		SelectedController = ControllerOptions[0];
	}
	ControllerCombo->RefreshOptions();
	ControllerCombo->SetSelectedItem(SelectedController);
	Status = ControllerOptions.IsEmpty()
		? LOCTEXT("NoController", "현재 맵에 JumpMapLayoutController가 없습니다. 배치 후 새로고침하세요.") : FText::GetEmpty();
	return FReply::Handled();
}

TSharedRef<SWidget> SJumpMapLayoutRegistration::MakeControllerOption(FControllerOption Option) const
{
	AJumpMapLayoutController* Controller = Option.IsValid() ? Option->Controller.Get() : nullptr;
	return SNew(STextBlock).Text(Controller
		? FText::FromString(FString::Printf(TEXT("%s (%s)"), *Controller->GetActorLabel(), *Controller->GetLevel()->GetName()))
		: LOCTEXT("InvalidController", "유효하지 않은 컨트롤러"));
}

FText SJumpMapLayoutRegistration::GetControllerLabel() const
{
	AJumpMapLayoutController* Controller = SelectedController.IsValid() ? SelectedController->Controller.Get() : nullptr;
	return Controller ? FText::FromString(Controller->GetActorLabel()) : LOCTEXT("SelectController", "컨트롤러 선택");
}

void SJumpMapLayoutRegistration::SelectController(FControllerOption Option, ESelectInfo::Type SelectionType)
{
	SelectedController = Option;
	Status = FText::GetEmpty();
}

FReply SJumpMapLayoutRegistration::RegisterSelection()
{
	if (!GEditor || GEditor->PlayWorld)
	{
		Status = LOCTEXT("Playing", "플레이를 종료한 뒤 등록하세요.");
		return FReply::Handled();
	}
	AJumpMapLayoutController* Controller = SelectedController.IsValid() ? SelectedController->Controller.Get() : nullptr;
	if (!IsValid(Controller))
	{
		Status = LOCTEXT("ChooseController", "컨트롤러를 선택하세요. 맵을 바꿨다면 새로고침하세요.");
		return FReply::Handled();
	}
	UEditorActorSubsystem* Actors = GEditor->GetEditorSubsystem<UEditorActorSubsystem>();
	if (!Actors)
	{
		Status = LOCTEXT("NoSubsystem", "에디터의 선택 액터 목록을 가져올 수 없습니다.");
		return FReply::Handled();
	}
	const TArray<AActor*> Selection = Actors->GetSelectedLevelActors();
	FScopedTransaction Transaction(LOCTEXT("Transaction", "점프맵 선택 액터 일괄 등록"));
	int32 AddedCount = 0;
	FString Error;
	if (!Controller->AddActorsToProfile(PlayerCount, Selection, AddedCount, Error))
	{
		Transaction.Cancel();
		Status = FText::FromString(Error);
		return FReply::Handled();
	}
	if (AddedCount == 0)
	{
		Transaction.Cancel();
	}
	Status = FText::Format(LOCTEXT("Registered", "{0}인 배열에 {1}개 등록했습니다. 이미 등록된 액터는 건너뜁니다."),
		FText::AsNumber(PlayerCount), FText::AsNumber(AddedCount));
	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
