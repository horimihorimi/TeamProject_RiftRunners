#include "MonsterHealthBarVisibilitySubsystem.h"

#include "Camera/PlayerCameraManager.h"
#include "Components/CapsuleComponent.h"
#include "Components/WidgetComponent.h"
#include "Engine/World.h"
#include "GameCore/Character/MonsterCharacter.h"
#include "GameFramework/PlayerController.h"

bool UMonsterHealthBarVisibilitySubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return Super::ShouldCreateSubsystem(Outer) && World && World->IsGameWorld() &&
	       World->GetNetMode() != NM_DedicatedServer;
}

TStatId UMonsterHealthBarVisibilitySubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UMonsterHealthBarVisibilitySubsystem, STATGROUP_Tickables);
}

void UMonsterHealthBarVisibilitySubsystem::RegisterMonster(AMonsterCharacter* Monster)
{
	if (!IsValid(Monster))
	{
		return;
	}
	for (const FEntry& Entry : Entries)
	{
		if (Entry.Monster.Get() == Monster)
		{
			return;
		}
	}
	FEntry& Entry = Entries.AddDefaulted_GetRef();
	Entry.Monster = Monster;
	Monster->MonsterHealthBarComponent->SetVisibility(false);
}

void UMonsterHealthBarVisibilitySubsystem::Tick(float DeltaTime)
{
	Entries.RemoveAllSwap(
	    [](const FEntry& Entry)
	    {
		    return !Entry.Monster.IsValid();
	    });
	if (Entries.IsEmpty())
	{
		NextEntry = 0;
		return;
	}
	UWorld* World = GetWorld();
	APlayerController* PC = World->GetFirstPlayerController();
	// 리슨 서버에서도 원격 플레이어의 카메라로 UI를 판단하지 않는다.
	if (!PC || !PC->IsLocalController() || !PC->PlayerCameraManager)
	{
		for (FEntry& Entry : Entries)
		{
			Entry.Monster->MonsterHealthBarComponent->SetVisibility(false);
			Entry.bWithinDistance = false;
			Entry.OccludedSince = -1.0;
		}
		return;
	}
	const FVector CameraLocation = PC->PlayerCameraManager->GetCameraLocation();
	const double Now = World->GetTimeSeconds();
	int32 ViewWidth = 0, ViewHeight = 0;
	PC->GetViewportSize(ViewWidth, ViewHeight);
	int32 TraceCount = 0;
	constexpr int32 MaxTracesPerFrame = 16;
	NextEntry %= Entries.Num();
	for (int32 Visited = 0; Visited < Entries.Num(); ++Visited)
	{
		FEntry& Entry = Entries[NextEntry];
		NextEntry = (NextEntry + 1) % Entries.Num();
		AMonsterCharacter* Monster = Entry.Monster.Get();
		UWidgetComponent* Bar = Monster->MonsterHealthBarComponent;
		if (!IsValid(Bar))
		{
			continue;
		}
		auto Hide = [&]()
		{
			Bar->SetVisibility(false);
			Entry.OccludedSince = -1.0;
		};
		if (!Monster->ShouldUseMonsterStatUI() || Monster->IsHidden() || Monster->bPossessionPresentationHidden)
		{
			Hide();
			Entry.bWithinDistance = false;
			continue;
		}
		if (Now < Entry.NextCheckTime)
		{
			continue;
		}
		const float ShowDistance = FMath::Max(0.0f, Monster->MaxHealthBarDistance);
		const float DistanceLimit = ShowDistance + (Entry.bWithinDistance ? 200.0f : 0.0f);
		Entry.bWithinDistance =
		    ShowDistance > 0.0f &&
		    FVector::DistSquared(CameraLocation, Monster->GetActorLocation()) <= FMath::Square(DistanceLimit);
		FVector2D ScreenPosition;
		const bool bOnScreen = PC->ProjectWorldLocationToScreen(Bar->GetComponentLocation(), ScreenPosition) &&
		                       ScreenPosition.X >= 0 && ScreenPosition.X <= ViewWidth && ScreenPosition.Y >= 0 &&
		                       ScreenPosition.Y <= ViewHeight;
		if (!Entry.bWithinDistance || !bOnScreen)
		{
			Hide();
			Entry.NextCheckTime = Now + FMath::Max(0.01f, Monster->HealthBarCheckInterval);
			continue;
		}
		// 두 번의 검사 예산을 확보한다. 다음 프레임은 미처 검사하지 못한 개체부터 시작한다.
		if (TraceCount + 2 > MaxTracesPerFrame)
		{
			NextEntry = (NextEntry + Entries.Num() - 1) % Entries.Num();
			break;
		}
		Entry.NextCheckTime = Now + FMath::Max(0.01f, Monster->HealthBarCheckInterval);
		FCollisionQueryParams Params(SCENE_QUERY_STAT(MonsterHealthBarOcclusion), false, Monster);
		Params.AddIgnoredActor(PC->GetPawn());
		const FVector BodyLocation = Monster->GetActorLocation();
		const FVector HeadLocation =
		    BodyLocation + FVector(0, 0, Monster->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() * 0.8f);
		++TraceCount;
		bool bVisible = !World->LineTraceTestByChannel(CameraLocation, BodyLocation, ECC_Visibility, Params);
		if (!bVisible)
		{
			++TraceCount;
			bVisible = !World->LineTraceTestByChannel(CameraLocation, HeadLocation, ECC_Visibility, Params);
		}
		if (bVisible)
		{
			Entry.OccludedSince = -1.0;
			Bar->SetVisibility(true);
		}
		else
		{
			if (Entry.OccludedSince < 0.0)
			{
				Entry.OccludedSince = Now;
			}
			if (Now - Entry.OccludedSince >= 0.2)
			{
				Bar->SetVisibility(false);
			}
		}
	}
}
