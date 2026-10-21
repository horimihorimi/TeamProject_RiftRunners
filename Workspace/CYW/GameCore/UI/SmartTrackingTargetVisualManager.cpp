#include "SmartTrackingTargetVisualManager.h"

#include "SmartTrackingVisualComponent.h"
#include "SmartTrackingLockIndicatorWidget.h"
#include "GameCore/Weapon/Core/WeaponBase.h"
#include "GameCore/Weapon/Core/WeaponComponent.h"
#include "GameCore/Weapon/Mods/SmartTracking/SmartTrackingSplineVisualActor.h"
#include "GameCore/Weapon/Mods/SmartTracking/SmartTrackingPathLibrary.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

UWorld* USmartTrackingTargetVisualManager::GetWorld() const
{
	const USmartTrackingVisualComponent* Owner = OwnerComponent.Get();
	return Owner ? Owner->GetWorld() : nullptr;
}

void USmartTrackingTargetVisualManager::Initialize(
	USmartTrackingVisualComponent* InOwnerComponent,
	APlayerController* PlayerController)
{
	OwnerComponent = InOwnerComponent;
	LocalPlayerController = PlayerController;
}

void USmartTrackingTargetVisualManager::SetWidgetHost(UCanvasPanel* Host)
{
	WidgetHost = Host;
}

void USmartTrackingTargetVisualManager::Shutdown()
{
	Clear();
	LocalPlayerController.Reset();
	OwnerComponent.Reset();
	WidgetHost.Reset();
}

void USmartTrackingTargetVisualManager::RefreshTickEnabled()
{
	if (USmartTrackingVisualComponent* Owner = OwnerComponent.Get())
	{
		Owner->SetComponentTickEnabled(!TargetVisuals.IsEmpty());
	}
}

bool USmartTrackingTargetVisualManager::SetLockedBulletCount(AActor* TargetActor, int32 Count)
{
	const int32 Index = FindTargetIndex(TargetActor);
	if (Index == INDEX_NONE) return false;
	if (USmartTrackingLockIndicatorWidget* Widget = TargetVisuals[Index].LockWidget)
	{
		Widget->SetLockedBulletCount(Count);
	}
	return true;
}

void USmartTrackingTargetVisualManager::NotifyLockLostAnimationFinished(USmartTrackingLockIndicatorWidget* LockWidget)
{
	const int32 Index = TargetVisuals.IndexOfByPredicate(
		[LockWidget](const FSmartTrackingTargetVisual& Visual) { return Visual.LockWidget == LockWidget; });
	if (Index != INDEX_NONE) FinishRemove(Index);
}

void USmartTrackingTargetVisualManager::DestroyVisual(FSmartTrackingTargetVisual& Visual, bool bPlayLockLost)
{
	if (IsValid(Visual.LockWidget))
	{
		Visual.LockWidget->SetVisualOwner(nullptr);
		if (bPlayLockLost) Visual.LockWidget->PlayLockLost();
		Visual.LockWidget->RemoveFromParent();
	}
	if (IsValid(Visual.SplineActor))
	{
		Visual.SplineActor->HideVisual();
		Visual.SplineActor->Destroy();
	}
}

void USmartTrackingTargetVisualManager::AddTarget(AActor* TargetActor, const FSmartTrackingVisualSettings& VisualSettings)
{
	APlayerController* PlayerController = LocalPlayerController.Get();
	UCanvasPanel* Host = WidgetHost.Get();
	if (!TargetActor || FindTargetIndex(TargetActor) != INDEX_NONE)
	{
		return;
	}

	USmartTrackingLockIndicatorWidget* LockWidget = nullptr;
	if (PlayerController && Host && VisualSettings.SmartTrackingLockIndicatorWidgetClass)
	{
		LockWidget = CreateWidget<USmartTrackingLockIndicatorWidget>(
			PlayerController,
			VisualSettings.SmartTrackingLockIndicatorWidgetClass);
		if (LockWidget)
		{
			LockWidget->SetVisualOwner(OwnerComponent.Get());
			if (UCanvasPanelSlot* CanvasSlot = Host->AddChildToCanvas(LockWidget))
			{
				CanvasSlot->SetAutoSize(true);
				CanvasSlot->SetAlignment(FVector2D(0.5f, 0.5f));
				LockWidget->SetVisibility(ESlateVisibility::HitTestInvisible);
				if (const APawn* Pawn = PlayerController->GetPawn())
				{
					const UWeaponComponent* Weapons = Pawn->FindComponentByClass<UWeaponComponent>();
					const AWeaponBase* Weapon = Weapons ? Weapons->GetEquippedWeapon() : nullptr;
					if (Weapon && Weapon->GetWeaponData())
						LockWidget->SetLockPreparationTime(Weapon->GetWeaponData()->SmartTrackingTargeting.LockPreparationTime);
				}
				LockWidget->InitializeLockIndicator(TargetActor);
			}
			else
			{
				LockWidget->RemoveFromParent();
				LockWidget = nullptr;
			}
		}
	}

	ASmartTrackingSplineVisualActor* SplineActor = nullptr;
	if (VisualSettings.SmartTrackingSplineVisualActorClass)
	{
		if (UWorld* World = GetWorld())
		{
			SplineActor = World->SpawnActor<ASmartTrackingSplineVisualActor>(VisualSettings.SmartTrackingSplineVisualActorClass);
			if (SplineActor)
			{
				SplineActor->InitializeVisual(PlayerController ? PlayerController->GetPawn() : nullptr, TargetActor);
			}
		}
	}

	FSmartTrackingTargetVisual& Visual = TargetVisuals.AddDefaulted_GetRef();
	Visual.TargetActor = TargetActor;
	Visual.LockWidget = LockWidget;
	Visual.SplineActor = SplineActor;
	RefreshTickEnabled();
	Update();
}

void USmartTrackingTargetVisualManager::RemoveTarget(AActor* TargetActor)
{
	const int32 TargetIndex = FindTargetIndex(TargetActor);
	if (TargetIndex == INDEX_NONE)
	{
		return;
	}

	FSmartTrackingTargetVisual& Visual = TargetVisuals[TargetIndex];
	if (Visual.bPendingRemoval)
	{
		return;
	}

	Visual.bPendingRemoval = true;

	if (ASmartTrackingSplineVisualActor* SplineActor = Visual.SplineActor)
	{
		SplineActor->HideVisual();
		SplineActor->Destroy();
	}
	Visual.SplineActor = nullptr;

	// Blueprint may finish the animation synchronously and remove this entry.
	if (USmartTrackingLockIndicatorWidget* LockWidget = Visual.LockWidget)
	{
		LockWidget->PlayLockLost();
	}
	else
	{
		FinishRemove(TargetIndex);
	}
}

void USmartTrackingTargetVisualManager::FinishRemove(int32 TargetIndex)
{
	if (!TargetVisuals.IsValidIndex(TargetIndex))
	{
		return;
	}

	FSmartTrackingTargetVisual RemovedVisual = TargetVisuals[TargetIndex];
	TargetVisuals.RemoveAt(TargetIndex);
	RefreshTickEnabled();
	DestroyVisual(RemovedVisual, false);
}

void USmartTrackingTargetVisualManager::Clear()
{
	TArray<FSmartTrackingTargetVisual> RemovedVisuals = MoveTemp(TargetVisuals);
	TargetVisuals.Reset();
	RefreshTickEnabled();
	for (FSmartTrackingTargetVisual& Visual : RemovedVisuals)
	{
		DestroyVisual(Visual, true);
	}
}

void USmartTrackingTargetVisualManager::Update()
{
	const FVector FirePoint = GetSmartTrackingFirePoint();
	float CurveHeight = 0.0f;
	float CurveSideOffset = 0.0f;
	APlayerController* PlayerController = LocalPlayerController.Get();
	APawn* Pawn = PlayerController ? PlayerController->GetPawn() : nullptr;
	UWeaponComponent* WeaponComponent = Pawn ? Pawn->FindComponentByClass<UWeaponComponent>() : nullptr;
	AWeaponBase* Weapon = WeaponComponent ? WeaponComponent->GetEquippedWeapon() : nullptr;
	if (const UWeaponDataAsset* WeaponData = Weapon ? Weapon->GetWeaponData() : nullptr)
	{
		CurveHeight = WeaponData->SmartTrackingPath.CurveHeight;
		CurveSideOffset = WeaponData->SmartTrackingPath.CurveSideOffset;
	}

	for (int32 Index = TargetVisuals.Num() - 1; Index >= 0; --Index)
	{
		FSmartTrackingTargetVisual& Visual = TargetVisuals[Index];
		if (Visual.bPendingRemoval)
		{
			continue;
		}

		AActor* TargetActor = Visual.TargetActor;
		if (!IsValid(TargetActor))
		{
			FinishRemove(Index);
			continue;
		}

		FVector2D TargetPosition;
		if (ProjectWorldToHostPosition(GetSmartTrackingTargetPoint(TargetActor), TargetPosition))
		{
			if (USmartTrackingLockIndicatorWidget* LockWidget = Visual.LockWidget)
			{
				if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(LockWidget->Slot))
				{
					CanvasSlot->SetPosition(TargetPosition);
				}
			}
		}

		if (ASmartTrackingSplineVisualActor* SplineActor = Visual.SplineActor)
		{
			SplineActor->UpdateVisual(
				FirePoint,
				GetSmartTrackingTargetPoint(TargetActor),
				CurveHeight,
				CurveSideOffset);
		}
	}
}

bool USmartTrackingTargetVisualManager::ProjectWorldToHostPosition(
	const FVector& WorldLocation,
	FVector2D& OutHostPosition) const
{
	APlayerController* PlayerController = LocalPlayerController.Get();
	UCanvasPanel* Host = WidgetHost.Get();
	if (!PlayerController || !Host)
	{
		return false;
	}

	FVector2D ScreenPosition = FVector2D::ZeroVector;
	if (!UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(
		PlayerController,
		WorldLocation,
		ScreenPosition,
		true))
	{
		return false;
	}

	const FVector2D PlayerScreenSize =
		UWidgetLayoutLibrary::GetPlayerScreenWidgetGeometry(PlayerController).GetLocalSize();
	const FVector2D HostSize = Host->GetCachedGeometry().GetLocalSize();
	if (PlayerScreenSize.X <= 0.0f || PlayerScreenSize.Y <= 0.0f || HostSize.X <= 0.0f || HostSize.Y <= 0.0f)
	{
		return false;
	}

	OutHostPosition = FVector2D(
		ScreenPosition.X * HostSize.X / PlayerScreenSize.X,
		ScreenPosition.Y * HostSize.Y / PlayerScreenSize.Y);
	return true;
}

FVector USmartTrackingTargetVisualManager::GetSmartTrackingFirePoint() const
{
	APlayerController* PlayerController = LocalPlayerController.Get();
	APawn* Pawn = PlayerController ? PlayerController->GetPawn() : nullptr;
	if (!Pawn)
	{
		return FVector::ZeroVector;
	}

	if (UWeaponComponent* WeaponComponent = Pawn->FindComponentByClass<UWeaponComponent>())
	{
		if (AWeaponBase* EquippedWeapon = WeaponComponent->GetEquippedWeapon())
		{
			return EquippedWeapon->GetFireTransform(Pawn).GetLocation();
		}
	}

	return Pawn->GetActorLocation();
}

FVector USmartTrackingTargetVisualManager::GetSmartTrackingTargetPoint(const AActor* TargetActor) const
{
	if (!TargetActor)
	{
		return FVector::ZeroVector;
	}

	return SmartTrackingPathLibrary::GetTargetPoint(TargetActor);
}

int32 USmartTrackingTargetVisualManager::FindTargetIndex(const AActor* TargetActor) const
{
	if (!TargetActor)
	{
		return INDEX_NONE;
	}

	for (int32 Index = 0; Index < TargetVisuals.Num(); ++Index)
	{
		if (TargetVisuals[Index].TargetActor == TargetActor)
		{
			return Index;
		}
	}

	return INDEX_NONE;
}
