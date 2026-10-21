#include "GameCore/UI/SmartTrackingVisualComponent.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "GameCore/Tags/GameplayTagSystem.h"
#include "SmartTrackingTargetVisualManager.h"
#include "GameCore/UI/SmartTrackingReticleWidget.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

USmartTrackingVisualComponent::USmartTrackingVisualComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	TargetVisualManager = CreateDefaultSubobject<USmartTrackingTargetVisualManager>(TEXT("TargetVisualManager"));
}

void USmartTrackingVisualComponent::BeginPlay()
{
	Super::BeginPlay();

	APlayerController* InPlayerController = Cast<APlayerController>(GetOwner());
	if (!InPlayerController || !InPlayerController->IsLocalController())
	{
		return;
	}
	LocalPlayerController = InPlayerController;
	TargetVisualManager->Initialize(this, InPlayerController);
	PawnChangedHandle = InPlayerController->GetOnNewPawnNotifier().AddUObject(
		this, &USmartTrackingVisualComponent::RebindToPawn);
	RebindToPawn(InPlayerController->GetPawn());
}

void USmartTrackingVisualComponent::RebindToPawn(APawn* NewPawn)
{
	Unbind();
	UAbilitySystemComponent* ASC = NewPawn
		? UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(NewPawn) : nullptr;
	if (!ASC)
	{
		return;
	}

	TagHandle = ASC->RegisterGameplayTagEvent(
		GameplayTagSystem::State_Weapon_SmartTracking,
		EGameplayTagEventType::NewOrRemoved).AddUObject(
			this, &USmartTrackingVisualComponent::OnSmartTrackingTagChanged);
	LockStateChangedHandle = ASC->GenericGameplayEventCallbacks.FindOrAdd(
		GameplayTagSystem::Event_Weapon_SmartTracking_LockStateChanged).AddUObject(
			this, &USmartTrackingVisualComponent::OnLockStateChanged);
	TargetLockedHandle = ASC->GenericGameplayEventCallbacks.FindOrAdd(
		GameplayTagSystem::Event_Weapon_SmartTracking_TargetLocked).AddUObject(
			this, &USmartTrackingVisualComponent::OnTargetLocked);
	TargetLostHandle = ASC->GenericGameplayEventCallbacks.FindOrAdd(
		GameplayTagSystem::Event_Weapon_SmartTracking_TargetLost).AddUObject(
			this, &USmartTrackingVisualComponent::OnTargetLost);
	TargetStackChangedHandle = ASC->GenericGameplayEventCallbacks.FindOrAdd(
		GameplayTagSystem::Event_Weapon_SmartTracking_TargetStackChanged).AddUObject(
			this, &USmartTrackingVisualComponent::OnTargetStackChanged);
	AbilitySystemComponent = ASC;
	OnSmartTrackingTagChanged(
		GameplayTagSystem::State_Weapon_SmartTracking,
		ASC->GetTagCount(GameplayTagSystem::State_Weapon_SmartTracking));
}

void USmartTrackingVisualComponent::Unbind()
{
	if (UAbilitySystemComponent* ASC = AbilitySystemComponent.Get())
	{
		ASC->RegisterGameplayTagEvent(
			GameplayTagSystem::State_Weapon_SmartTracking,
			EGameplayTagEventType::NewOrRemoved).Remove(TagHandle);
		auto RemoveEvent = [ASC](const FGameplayTag& Tag, FDelegateHandle Handle)
		{
			if (FGameplayEventMulticastDelegate* Delegate = ASC->GenericGameplayEventCallbacks.Find(Tag))
			{
				Delegate->Remove(Handle);
			}
		};
		RemoveEvent(GameplayTagSystem::Event_Weapon_SmartTracking_LockStateChanged, LockStateChangedHandle);
		RemoveEvent(GameplayTagSystem::Event_Weapon_SmartTracking_TargetLocked, TargetLockedHandle);
		RemoveEvent(GameplayTagSystem::Event_Weapon_SmartTracking_TargetLost, TargetLostHandle);
		RemoveEvent(GameplayTagSystem::Event_Weapon_SmartTracking_TargetStackChanged, TargetStackChangedHandle);
	}
	AbilitySystemComponent.Reset();
	TagHandle.Reset();
	LockStateChangedHandle.Reset();
	TargetLockedHandle.Reset();
	TargetLostHandle.Reset();
	TargetStackChangedHandle.Reset();
}

void USmartTrackingVisualComponent::Shutdown()
{
	TargetVisualManager->Shutdown();
	RemoveSmartTrackingReticleWidget();
	Unbind();
	if (APlayerController* PC = LocalPlayerController.Get())
	{
		PC->GetOnNewPawnNotifier().Remove(PawnChangedHandle);
	}
	PawnChangedHandle.Reset();
	LocalPlayerController.Reset();
	WidgetHost.Reset();
}

void USmartTrackingVisualComponent::SetWidgetHost(UCanvasPanel* InWidgetHost)
{
	if (WidgetHost.Get() == InWidgetHost) return;
	TargetVisualManager->Clear();
	RemoveSmartTrackingReticleWidget();
	WidgetHost = InWidgetHost;
	TargetVisualManager->SetWidgetHost(InWidgetHost);
	if (InWidgetHost) CreateSmartTrackingReticleWidget();
}

void USmartTrackingVisualComponent::NotifyLockLostAnimationFinished(USmartTrackingLockIndicatorWidget* LockWidget)
{
	TargetVisualManager->NotifyLockLostAnimationFinished(LockWidget);
}

void USmartTrackingVisualComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	TargetVisualManager->Update();
}

void USmartTrackingVisualComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Shutdown();
	Super::EndPlay(EndPlayReason);
}
void USmartTrackingVisualComponent::OnSmartTrackingTagChanged(const FGameplayTag Tag, int32 NewCount)
{
	if (Tag != GameplayTagSystem::State_Weapon_SmartTracking)
	{
		return;
	}

	USmartTrackingReticleWidget* ReticleWidget = SmartTrackingReticleWidget
		? SmartTrackingReticleWidget.Get()
		: CreateSmartTrackingReticleWidget();
	if (!ReticleWidget)
	{
		return;
	}

	if (NewCount > 0)
	{
		ReticleWidget->SetNeutralColor();
		ReticleWidget->ShowReticle();
		if (!bSmartTrackingActive)
		{
			PlaySmartTrackingSound(VisualSettings.SmartTrackingStartSound);
		}
		bSmartTrackingActive = true;
	}
	else
	{
		ReticleWidget->HideReticle();
		TargetVisualManager->Clear();
		if (bSmartTrackingActive)
		{
			PlaySmartTrackingSound(VisualSettings.SmartTrackingEndSound);
		}
		bSmartTrackingActive = false;
	}
}

void USmartTrackingVisualComponent::OnLockStateChanged(const FGameplayEventData* Payload)
{
	if (!Payload || Payload->EventTag != GameplayTagSystem::Event_Weapon_SmartTracking_LockStateChanged)
	{
		return;
	}

	USmartTrackingReticleWidget* ReticleWidget = SmartTrackingReticleWidget.Get();
	if (!ReticleWidget)
	{
		return;
	}

	const ESmartTrackingLockState LockState = static_cast<ESmartTrackingLockState>(
		FMath::Clamp(FMath::RoundToInt(Payload->EventMagnitude), 0, 2));
	if (LockState == ESmartTrackingLockState::FullyLocked)
	{
		ReticleWidget->SetFullyLockedColor();
	}
	else if (LockState == ESmartTrackingLockState::Tracking)
	{
		ReticleWidget->SetLockedColor();
	}
	else
	{
		ReticleWidget->SetNeutralColor();
	}
}

void USmartTrackingVisualComponent::OnTargetLocked(const FGameplayEventData* Payload)
{
	if (!Payload || Payload->EventTag != GameplayTagSystem::Event_Weapon_SmartTracking_TargetLocked)
	{
		return;
	}

	TargetVisualManager->AddTarget(const_cast<AActor*>(Payload->Target.Get()), VisualSettings);
	PlaySmartTrackingSound(VisualSettings.SmartTrackingTargetLockedSound);
}

void USmartTrackingVisualComponent::OnTargetLost(const FGameplayEventData* Payload)
{
	if (!Payload || Payload->EventTag != GameplayTagSystem::Event_Weapon_SmartTracking_TargetLost)
	{
		return;
	}

	TargetVisualManager->RemoveTarget(const_cast<AActor*>(Payload->Target.Get()));
	PlaySmartTrackingSound(VisualSettings.SmartTrackingTargetLostSound);
}

void USmartTrackingVisualComponent::OnTargetStackChanged(const FGameplayEventData* Payload)
{
	if (!Payload || Payload->EventTag != GameplayTagSystem::Event_Weapon_SmartTracking_TargetStackChanged)
	{
		return;
	}

	AActor* TargetActor = const_cast<AActor*>(Payload->Target.Get());
	if (!TargetVisualManager->SetLockedBulletCount(TargetActor, FMath::RoundToInt(Payload->EventMagnitude)))
	{
		return;
	}

	if (Payload->EventMagnitude > 0.0f)
	{
		PlaySmartTrackingSound(VisualSettings.SmartTrackingBulletStackSound);
	}
}

USmartTrackingReticleWidget* USmartTrackingVisualComponent::CreateSmartTrackingReticleWidget()
{
	APlayerController* PlayerController = LocalPlayerController.Get();
	UCanvasPanel* Host = WidgetHost.Get();
	if (SmartTrackingReticleWidget || !PlayerController || !Host || !VisualSettings.SmartTrackingReticleWidgetClass)
	{
		return SmartTrackingReticleWidget.Get();
	}

	USmartTrackingReticleWidget* Widget = CreateWidget<USmartTrackingReticleWidget>(
		PlayerController,
		VisualSettings.SmartTrackingReticleWidgetClass);
	if (!Widget)
	{
		return nullptr;
	}

	UCanvasPanelSlot* CanvasSlot = Host->AddChildToCanvas(Widget);
	if (!CanvasSlot)
	{
		Widget->RemoveFromParent();
		return nullptr;
	}

	CanvasSlot->SetAnchors(FAnchors(0.5f, 0.5f));
	CanvasSlot->SetAlignment(FVector2D(0.5f, 0.5f));
	CanvasSlot->SetPosition(FVector2D::ZeroVector);
	CanvasSlot->SetAutoSize(true);
	Widget->SetVisibility(ESlateVisibility::Collapsed);
	SmartTrackingReticleWidget = Widget;
	return SmartTrackingReticleWidget.Get();
}

void USmartTrackingVisualComponent::RemoveSmartTrackingReticleWidget()
{
	if (SmartTrackingReticleWidget)
	{
		SmartTrackingReticleWidget->RemoveFromParent();
		SmartTrackingReticleWidget = nullptr;
	}
}

void USmartTrackingVisualComponent::PlaySmartTrackingSound(USoundBase* Sound) const
{
	if (Sound)
	{
		UGameplayStatics::PlaySound2D(GetWorld(), Sound, VisualSettings.SmartTrackingSoundVolume);
	}
}
