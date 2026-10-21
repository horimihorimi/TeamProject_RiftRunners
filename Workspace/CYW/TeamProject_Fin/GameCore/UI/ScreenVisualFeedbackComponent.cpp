#include "ScreenVisualFeedbackComponent.h"

#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "GameCore/Combat/CombatFeedbackComponent.h"
#include "GameCore/UI/DamageNumberWidget.h"
#include "GameFramework/Actor.h"
#include "GameFramework/PlayerController.h"

UScreenVisualFeedbackComponent::UScreenVisualFeedbackComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UScreenVisualFeedbackComponent::BeginPlay()
{
	Super::BeginPlay();

	APlayerController* PlayerController = Cast<APlayerController>(GetOwner());
	if (!PlayerController || !PlayerController->IsLocalController())
	{
		return;
	}

	LocalPlayerController = PlayerController;
	BindFeedbackSource();
}

void UScreenVisualFeedbackComponent::SetWidgetHost(UCanvasPanel* InWidgetHost)
{
	if (WidgetHost.Get() == InWidgetHost)
	{
		return;
	}

	for (UDamageNumberWidget* Widget : AllDamageNumberWidgets)
	{
		if (Widget)
		{
			Widget->SetPoolOwner(nullptr);
			Widget->RemoveFromParent();
		}
	}
	AvailableDamageNumberWidgets.Reset();
	AllDamageNumberWidgets.Reset();
	WidgetHost = InWidgetHost;

	if (WidgetHost.IsValid())
	{
		PrewarmDamageNumberPool();
	}
}

void UScreenVisualFeedbackComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UCombatFeedbackComponent* Source = FeedbackSource.Get())
	{
		Source->OnDamageFeedbackReceived().Remove(FeedbackDelegateHandle);
	}

	FeedbackSource.Reset();
	WidgetHost.Reset();
	LocalPlayerController.Reset();
	FeedbackDelegateHandle.Reset();

	for (UDamageNumberWidget* Widget : AllDamageNumberWidgets)
	{
		if (Widget)
		{
			Widget->SetPoolOwner(nullptr);
			Widget->RemoveFromParent();
		}
	}
	AvailableDamageNumberWidgets.Reset();
	AllDamageNumberWidgets.Reset();

	Super::EndPlay(EndPlayReason);
}

void UScreenVisualFeedbackComponent::BindFeedbackSource()
{
	UCombatFeedbackComponent* Source = GetOwner()
		? GetOwner()->FindComponentByClass<UCombatFeedbackComponent>()
		: nullptr;
	if (!Source)
	{
		return;
	}

	Source->OnDamageFeedbackReceived().RemoveAll(this);
	FeedbackDelegateHandle = Source->OnDamageFeedbackReceived().AddUObject(
		this,
		&UScreenVisualFeedbackComponent::OnDamageFeedbackBatch);
	FeedbackSource = Source;
}

void UScreenVisualFeedbackComponent::OnDamageFeedbackBatch(
	const TArray<FDamageFeedbackData>& FeedbackBatch)
{
	for (const FDamageFeedbackData& Feedback : FeedbackBatch)
	{
		ShowDamageFeedback(Feedback);
	}
}

void UScreenVisualFeedbackComponent::ShowDamageFeedback(const FDamageFeedbackData& Feedback)
{
	APlayerController* PlayerController = LocalPlayerController.Get();
	if (!PlayerController || Feedback.DamageAmount <= 0.0f)
	{
		return;
	}

	FVector2D ScreenPosition = FVector2D::ZeroVector;
	if (!UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(
		PlayerController,
		Feedback.WorldLocation,
		ScreenPosition,
		true))
	{
		return;
	}

	const FVector2D PlayerScreenSize =
		UWidgetLayoutLibrary::GetPlayerScreenWidgetGeometry(PlayerController).GetLocalSize();
	if (PlayerScreenSize.X <= 0.0f || PlayerScreenSize.Y <= 0.0f
		|| ScreenPosition.X < 0.0f || ScreenPosition.Y < 0.0f
		|| ScreenPosition.X > PlayerScreenSize.X || ScreenPosition.Y > PlayerScreenSize.Y)
	{
		return;
	}

	UCanvasPanel* Host = WidgetHost.Get();
	if (!Host)
	{
		return;
	}

	const FVector2D HostSize = Host->GetCachedGeometry().GetLocalSize();
	if (HostSize.X <= 0.0f || HostSize.Y <= 0.0f)
	{
		return;
	}
	const FVector2D HostPosition(
		ScreenPosition.X * HostSize.X / PlayerScreenSize.X,
		ScreenPosition.Y * HostSize.Y / PlayerScreenSize.Y);

	UDamageNumberWidget* Widget = AcquireDamageNumberWidget();
	if (!Widget)
	{
		return;
	}

	Widget->SetVisibility(ESlateVisibility::HitTestInvisible);
	Widget->InitializeDamageNumber(
		Feedback.DamageAmount,
		HostPosition,
		Feedback.bWeakpoint);
}

void UScreenVisualFeedbackComponent::PrewarmDamageNumberPool()
{
	const int32 PoolSize = FMath::Clamp(InitialPoolSize, 0, MaxPoolSize);
	for (int32 Index = 0; Index < PoolSize; ++Index)
	{
		if (UDamageNumberWidget* Widget = CreateDamageNumberWidget())
		{
			AvailableDamageNumberWidgets.Add(Widget);
		}
	}
}

UDamageNumberWidget* UScreenVisualFeedbackComponent::CreateDamageNumberWidget()
{
	APlayerController* PlayerController = LocalPlayerController.Get();
	UCanvasPanel* Host = WidgetHost.Get();
	if (!PlayerController || !Host || AllDamageNumberWidgets.Num() >= MaxPoolSize)
	{
		return nullptr;
	}

	TSubclassOf<UDamageNumberWidget> WidgetClass = DamageNumberWidgetClass;
	if (!WidgetClass)
	{
		return nullptr;
	}

	UDamageNumberWidget* Widget = CreateWidget<UDamageNumberWidget>(PlayerController, WidgetClass);
	if (!Widget)
	{
		return nullptr;
	}

	Widget->SetPoolOwner(this);
	UCanvasPanelSlot* CanvasSlot = Host->AddChildToCanvas(Widget);
	if (!CanvasSlot)
	{
		Widget->SetPoolOwner(nullptr);
		return nullptr;
	}
	CanvasSlot->SetAutoSize(true);
	CanvasSlot->SetAlignment(FVector2D(0.5f, 0.5f));
	Widget->SetVisibility(ESlateVisibility::Collapsed);
	AllDamageNumberWidgets.Add(Widget);
	return Widget;
}

UDamageNumberWidget* UScreenVisualFeedbackComponent::AcquireDamageNumberWidget()
{
	while (!AvailableDamageNumberWidgets.IsEmpty())
	{
		UDamageNumberWidget* Widget = AvailableDamageNumberWidgets.Pop(EAllowShrinking::No);
		if (IsValid(Widget) && Widget->GetParent() == WidgetHost.Get())
		{
			return Widget;
		}
	}

	return CreateDamageNumberWidget();
}

void UScreenVisualFeedbackComponent::ReturnDamageNumberToPool(UDamageNumberWidget* Widget)
{
	if (!IsValid(Widget) || !AllDamageNumberWidgets.Contains(Widget))
	{
		return;
	}

	Widget->SetVisibility(ESlateVisibility::Collapsed);
	AvailableDamageNumberWidgets.AddUnique(Widget);
}
