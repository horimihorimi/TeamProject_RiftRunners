#include "EnemyActivationSubsystem.h"

#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "NavigationInvokerComponent.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "GameCore/Enemy/Spawn/EnemySpawnSettings.h"
#include "GameCore/Enemy/Spawn/SpawnPoint.h"

bool UEnemyActivationSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UEnemyActivationSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	if (InWorld.GetNetMode() == NM_Client)
	{
		return;
	}

	CheckInterval = FMath::Max(GetDefault<UEnemySpawnSettings>()->ActivationCheckInterval, 0.05f);
	InWorld.GetTimerManager().SetTimer(CheckTimer, this, &UEnemyActivationSubsystem::CheckActivation, CheckInterval, true);
}

void UEnemyActivationSubsystem::Deinitialize()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(CheckTimer);
	}
	SpawnPoints.Reset();

	Super::Deinitialize();
}

void UEnemyActivationSubsystem::Register(ASpawnPoint* SpawnPoint)
{
	if (IsValid(SpawnPoint))
	{
		SpawnPoints.AddUnique(SpawnPoint);
	}
}

void UEnemyActivationSubsystem::Unregister(ASpawnPoint* SpawnPoint)
{
	SpawnPoints.Remove(SpawnPoint);
}

void UEnemyActivationSubsystem::CheckActivation()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(EnemyActivation_Check);

	UWorld* World = GetWorld();
	if (!World || SpawnPoints.IsEmpty())
	{
		return;
	}

	const float Margin = GetDefault<UEnemySpawnSettings>()->ActivationMargin;

	Viewers.Reset();
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		const APawn* Pawn = It->IsValid() ? It->Get()->GetPawn() : nullptr;
		if (!IsValid(Pawn))
		{
			continue;
		}

		// 네비메시는 인보커가 있는 플레이어 주변에만 생긴다. 인보커가 없으면 이 플레이어 곁에는 몹이 움직일 땅이 없다
		const UNavigationInvokerComponent* Invoker = Pawn->FindComponentByClass<UNavigationInvokerComponent>();
		if (!Invoker)
		{
			if (!bWarnedMissingInvoker)
			{
				bWarnedMissingInvoker = true;
				UE_LOG(LogTemp, Warning, TEXT("[EnemyActivation] NavigationInvokerComponent 가 없는 플레이어 폰은 거리 판정에서 뺀다: %s"),
					*GetNameSafe(Pawn));
			}
			continue;
		}

		FActivationViewer& Viewer = Viewers.AddDefaulted_GetRef();
		Viewer.Location = Pawn->GetActorLocation();

		// 활성화는 생성 반경, 비활성화는 제거 반경 안쪽에서 정한다. 비활성화가 활성화보다 작아지면 경계에서 꺼냈다 돌려보냈다를 반복한다
		Viewer.ActivationDistance = FMath::Max(Invoker->GetGenerationRadius() - Margin, 0.0f);
		Viewer.DeactivationDistance = FMath::Max(Invoker->GetRemovalRadius() - Margin, Viewer.ActivationDistance);
	}

	SpawnPoints.RemoveAll([](const TWeakObjectPtr<ASpawnPoint>& Point) { return !Point.IsValid(); });
	CheckBuffer = SpawnPoints;

	for (const TWeakObjectPtr<ASpawnPoint>& Point : CheckBuffer)
	{
		if (ASpawnPoint* SpawnPoint = Point.Get())
		{
			SpawnPoint->UpdateActivation(Viewers, CheckInterval);
		}
	}
}
