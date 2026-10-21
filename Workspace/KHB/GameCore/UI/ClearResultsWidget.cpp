#include "ClearResultsWidget.h"

#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "InputCoreTypes.h"

void UClearResultsWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetIsFocusable(true);
	Button_ReturnToRoom->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleReturnClicked);
	// 통계 미연결 상태를 실제 0점 결과처럼 표시하지 않는다.
	const FText Pending = FText::FromString(TEXT("—"));
	Text_ClearTime->SetText(Pending);
	Text_TotalDamage->SetText(Pending);
	Text_BossDamage->SetText(Pending);
	Text_KillCount->SetText(Pending);
	Text_DamageTaken->SetText(Pending);
	Text_ReviveCount->SetText(Pending);
	Text_ReadyCount->SetText(FText::GetEmpty());
	Text_ReturnButton->SetText(NSLOCTEXT("ClearResults", "Return", "대기방으로 돌아가기"));
}

void UClearResultsWidget::NativeDestruct()
{
	Button_ReturnToRoom->OnClicked.RemoveDynamic(this, &ThisClass::HandleReturnClicked);
	Super::NativeDestruct();
}

void UClearResultsWidget::SetResultsData(const FClearResultsDisplayData& Data)
{
	FNumberFormattingOptions Format;
	Format.SetMaximumFractionalDigits(0);
	Text_TotalDamage->SetText(FText::AsNumber(Data.TotalDamage, &Format));
	Text_BossDamage->SetText(FText::AsNumber(Data.BossDamage, &Format));
	Text_KillCount->SetText(FText::AsNumber(Data.KillCount));
	Text_DamageTaken->SetText(FText::AsNumber(Data.DamageTaken, &Format));
	Text_ReviveCount->SetText(FText::AsNumber(Data.ReviveCount));
	const int32 Seconds = FMath::Max(0, FMath::FloorToInt(Data.ClearDurationSeconds));
	Text_ClearTime->SetText(FText::FromString(FString::Printf(TEXT("%02d:%02d"), Seconds / 60, Seconds % 60)));
}

void UClearResultsWidget::SetReturnReadyState(int32 ReadyCount, int32 PlayerCount, bool bLocalReady)
{
	PlayerCount = FMath::Max(0, PlayerCount);
	ReadyCount = FMath::Clamp(ReadyCount, 0, PlayerCount);
	Text_ReadyCount->SetText(FText::Format(
		NSLOCTEXT("ClearResults", "ReadyCount", "복귀 준비 {0} / {1}"),
		FText::AsNumber(ReadyCount), FText::AsNumber(PlayerCount)));
	Button_ReturnToRoom->SetIsEnabled(!bLocalReady);
	Text_ReturnButton->SetText(bLocalReady
		? NSLOCTEXT("ClearResults", "Waiting", "다른 파티원 대기 중")
		: NSLOCTEXT("ClearResults", "Return", "대기방으로 돌아가기"));
}

void UClearResultsWidget::HandleReturnClicked()
{
	OnReturnRequested.Broadcast();
}

FReply UClearResultsWidget::NativeOnKeyDown(const FGeometry& Geometry, const FKeyEvent& KeyEvent)
{
	// 결과 화면에서 ESC로 게임 입력을 다시 열지 않는다.
	if (KeyEvent.GetKey() == EKeys::Escape)
	{
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(Geometry, KeyEvent);
}
