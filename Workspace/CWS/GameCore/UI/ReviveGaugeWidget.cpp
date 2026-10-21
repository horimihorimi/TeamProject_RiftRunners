#include "ReviveGaugeWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/HorizontalBox.h"
#include "Components/Overlay.h"

namespace ReviveGauge
{
	constexpr int32 SegmentCount = 20;

	float ClampPercent(float Value)
	{
		return FMath::IsFinite(Value) ? FMath::Clamp(Value, 0.0f, 1.0f) : 0.0f;
	}

	float CalculateScale(float InPercent, int32 SegmentIndex)
	{
		const float FillPosition = ClampPercent(InPercent) * SegmentCount;
		const float Distance = FillPosition - SegmentIndex;
		if (Distance <= 0.0f)
		{
			return 0.0f;
		}

		float Scale = 1.0f;
		if (Distance < 1.0f)
		{
			Scale = FMath::Lerp(0.0f, 0.35f, Distance);
		}
		else if (Distance < 2.0f)
		{
			Scale = FMath::Lerp(0.35f, 0.65f, Distance - 1.0f);
		}
		else if (Distance < 3.0f)
		{
			Scale = FMath::Lerp(0.65f, 1.0f, Distance - 2.0f);
		}

		// 마지막 한 칸을 채우면서 끝부분도 복원하여 100% 직전의 크기 점프를 없앱니다.
		const float FullBlend = FMath::Clamp(FillPosition - (SegmentCount - 1), 0.0f, 1.0f);
		return FMath::Lerp(Scale, 1.0f, FullBlend);
	}
}

void UReviveGaugeWidget::NativePreConstruct()
{
	Super::NativePreConstruct();
	CacheFillGroups();
	ApplyPercent(IsDesignTime() ? PreviewPercent : Percent);
}

void UReviveGaugeWidget::CacheFillGroups()
{
	FillGroups.Reset(ReviveGauge::SegmentCount);
	if (!WidgetTree || !SegmentBox)
	{
		return;
	}

	for (int32 Index = 0; Index < ReviveGauge::SegmentCount; ++Index)
	{
		const FName GroupName(*FString::Printf(TEXT("FillGroup_%d"), Index));
		FillGroups.Add(WidgetTree->FindWidget<UOverlay>(GroupName));
	}
}

void UReviveGaugeWidget::UpdateGauge(float InPercent)
{
	Percent = ReviveGauge::ClampPercent(InPercent);
	ApplyPercent(Percent);
}

void UReviveGaugeWidget::UpdateGaugeFromValues(float CurrentValue, float MaximumValue)
{
	const bool bValidValues = FMath::IsFinite(CurrentValue)
		&& FMath::IsFinite(MaximumValue) && MaximumValue > 0.0f;
	if (!bValidValues || CurrentValue <= 0.0f)
	{
		UpdateGauge(0.0f);
	}
	else
	{
		// 최대값이 매우 작더라도 나눗셈 오버플로 없이 만충으로 제한합니다.
		UpdateGauge(CurrentValue >= MaximumValue ? 1.0f : CurrentValue / MaximumValue);
	}
}

void UReviveGaugeWidget::ApplyPercent(float InPercent)
{
	for (int32 Index = 0; Index < FillGroups.Num(); ++Index)
	{
		if (UOverlay* Group = FillGroups[Index])
		{
			const float Scale = ReviveGauge::CalculateScale(InPercent, Index);
			Group->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
			Group->SetRenderScale(FVector2D(Scale, Scale));
			// Hidden은 칸의 배치를 보존하면서 알맹이와 글로우를 함께 숨깁니다.
			Group->SetVisibility(Scale > 0.0f ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
		}
	}
}


#if WITH_DEV_AUTOMATION_TESTS
#include "Components/Image.h"
#include "Misc/AutomationTest.h"
#include "UObject/StrongObjectPtr.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FReviveGaugeScaleTest, "TeamProject.UI.ReviveGauge.Scale",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FReviveGaugeScaleTest::RunTest(const FString& Parameters)
{
	for (int32 Index = 0; Index < ReviveGauge::SegmentCount; ++Index)
	{
		TestEqual(TEXT("0%는 모든 밝은 알맹이가 사라짐"), ReviveGauge::CalculateScale(0.0f, Index), 0.0f);
		TestEqual(TEXT("100%는 모든 알맹이가 최대 크기"), ReviveGauge::CalculateScale(1.0f, Index), 1.0f);
		const float ExpectedHalf = Index < 8 ? 1.0f : Index == 8 ? 0.65f : Index == 9 ? 0.35f : 0.0f;
		TestTrue(TEXT("50%는 큰 8개, 중간 1개, 작은 1개"),
			FMath::IsNearlyEqual(ReviveGauge::CalculateScale(0.5f, Index), ExpectedHalf));
	}

	// 칸 경계와 만충 전환에서 좌우 값이 가까운지 검사합니다.
	for (int32 Boundary = 1; Boundary <= ReviveGauge::SegmentCount; ++Boundary)
	{
		const float Percent = static_cast<float>(Boundary) / ReviveGauge::SegmentCount;
		for (int32 Index = 0; Index < ReviveGauge::SegmentCount; ++Index)
		{
			const float Left = ReviveGauge::CalculateScale(Percent - 0.0001f, Index);
			const float Right = ReviveGauge::CalculateScale(Percent + 0.0001f, Index);
			TestTrue(TEXT("크기 변화는 경계에서 연속적임"), FMath::Abs(Right - Left) < 0.01f);
		}
	}

	TestEqual(TEXT("음수 비율은 0으로 제한"), ReviveGauge::ClampPercent(-1.0f), 0.0f);
	TestEqual(TEXT("초과 비율은 1로 제한"), ReviveGauge::ClampPercent(2.0f), 1.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FReviveGaugeWidgetTest, "TeamProject.UI.ReviveGauge.Widget",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FReviveGaugeWidgetTest::RunTest(const FString& Parameters)
{
	UClass* WidgetClass = LoadClass<UReviveGaugeWidget>(nullptr,
		TEXT("/Game/TPSDevelop/UI/GamePlay/WBP_ReviveGaugeWidget.WBP_ReviveGaugeWidget_C"));
	if (!TestNotNull(TEXT("기존 부활 위젯 블루프린트 로드"), WidgetClass))
	{
		return false;
	}

	TStrongObjectPtr<UReviveGaugeWidget> Gauge(NewObject<UReviveGaugeWidget>(GetTransientPackage(), WidgetClass));
	Gauge->Initialize();
	// Slate 생성 전에 받은 현재값이 초기 구성으로 덮이지 않는지 검사합니다.
	Gauge->UpdateGaugeFromValues(50.0f, 100.0f);
	TSharedPtr<SWidget> SlateWidget = Gauge->TakeWidget();
	UHorizontalBox* Row = Cast<UHorizontalBox>(Gauge->GetWidgetFromName(TEXT("SegmentBox")));
	if (!TestNotNull(TEXT("블루프린트에 알맹이 컨테이너가 있음"), Row))
	{
		return false;
	}
	TestEqual(TEXT("고정된 칸은 20개"), Row->GetChildrenCount(), 20);

	for (int32 Index = 0; Index < 20; ++Index)
	{
		UOverlay* Group = Cast<UOverlay>(Gauge->GetWidgetFromName(
			FName(*FString::Printf(TEXT("FillGroup_%d"), Index))));
		UImage* Background = Cast<UImage>(Gauge->GetWidgetFromName(
			FName(*FString::Printf(TEXT("Background_%02d"), Index))));
		if (!TestNotNull(TEXT("밝은 그룹 존재"), Group) || !TestNotNull(TEXT("배경 알맹이 존재"), Background))
		{
			return false;
		}
		TestEqual(TEXT("글로우와 선명한 알맹이가 같은 그룹에 있음"), Group->GetChildrenCount(), 2);
		TestTrue(TEXT("생성 전 전달한 50% 반영"), FMath::IsNearlyEqual(
			Group->GetRenderTransform().Scale.X, ReviveGauge::CalculateScale(0.5f, Index)));
		Gauge->UpdateGauge(0.0f);
		TestTrue(TEXT("0%에서 밝은 그룹만 숨김"), Group->GetVisibility() == ESlateVisibility::Hidden);
		TestEqual(TEXT("배경 알맹이 크기는 고정"), Background->GetRenderTransform().Scale.X, 1.0);
		Gauge->UpdateGauge(1.0f);
		TestEqual(TEXT("100%에서 그룹 크기는 1"), Group->GetRenderTransform().Scale.X, 1.0);
		Gauge->UpdateGauge(0.5f);
	}

	Gauge->UpdateGaugeFromValues(5.0f, 0.0f);
	TestEqual(TEXT("최대값 0은 빈 게이지"), Gauge->GetGaugePercent(), 0.0f);
	Gauge->UpdateGaugeFromValues(-1.0f, 100.0f);
	TestEqual(TEXT("음수 현재값은 빈 게이지"), Gauge->GetGaugePercent(), 0.0f);
	Gauge->UpdateGaugeFromValues(10.0f, 1.e-30f);
	TestEqual(TEXT("매우 작은 최대값도 오버플로 없이 만충"), Gauge->GetGaugePercent(), 1.0f);
	Gauge->UpdateGaugeFromValues(25.0f, 100.0f);
	Gauge->ReleaseSlateResources(true);
	SlateWidget.Reset();
	SlateWidget = Gauge->TakeWidget();
	TestEqual(TEXT("위젯 재구성 후 값 보존"), Gauge->GetGaugePercent(), 0.25f);
	return true;
}
#endif
