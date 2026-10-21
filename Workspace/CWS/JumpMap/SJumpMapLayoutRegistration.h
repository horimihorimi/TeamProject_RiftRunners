#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class AJumpMapLayoutController;
template <typename OptionType> class SComboBox;

struct FJumpMapControllerOption
{
	TWeakObjectPtr<AJumpMapLayoutController> Controller;
};

/** 월드 선택을 유지한 채 인원별 배열에 액터를 일괄 등록하는 에디터 창. */
class SJumpMapLayoutRegistration : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SJumpMapLayoutRegistration) {}
	SLATE_END_ARGS()
	void Construct(const FArguments& InArgs);

private:
	using FControllerOption = TSharedPtr<FJumpMapControllerOption>;
	TArray<FControllerOption> ControllerOptions;
	TSharedPtr<SComboBox<FControllerOption>> ControllerCombo;
	FControllerOption SelectedController;
	int32 PlayerCount = 1;
	FText Status;

	FReply RefreshControllers();
	FReply RegisterSelection();
	TSharedRef<SWidget> MakeControllerOption(FControllerOption Option) const;
	FText GetControllerLabel() const;
	void SelectController(FControllerOption Option, ESelectInfo::Type SelectionType);
};
