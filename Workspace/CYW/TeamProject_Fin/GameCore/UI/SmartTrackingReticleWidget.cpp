#include "SmartTrackingReticleWidget.h"

void USmartTrackingReticleWidget::ShowReticle()
{
	SetVisibility(ESlateVisibility::HitTestInvisible);
	OnShowReticle();
}

void USmartTrackingReticleWidget::HideReticle()
{
	SetVisibility(ESlateVisibility::Collapsed);
	OnHideReticle();
}

void USmartTrackingReticleWidget::SetNeutralColor()
{
	OnReticleColorChanged(NeutralColor);
}

void USmartTrackingReticleWidget::SetLockedColor()
{
	OnReticleColorChanged(LockedColor);
}

void USmartTrackingReticleWidget::SetFullyLockedColor()
{
	OnReticleColorChanged(FullyLockedColor);
}
