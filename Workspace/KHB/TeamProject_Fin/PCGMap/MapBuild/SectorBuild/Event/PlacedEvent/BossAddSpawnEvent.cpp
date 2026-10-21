#include "BossAddSpawnEvent.h"

#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameCore/Character/BaseCharacter.h"
#include "GameCore/Character/BossMonsterCharacter.h"
#include "GameCore/Combat/BossPhaseComponent.h"
#include "GameCore/Enemy/Spawn/SpawnGroupLinker.h"
#include "GameCore/Enemy/Spawn/SpawnPoint.h"
#include "TimerManager.h"

void ABossAddSpawnEvent::Execute(const UObject* WorldContext)
{
	Super::Execute(WorldContext);

	if (!HasAuthority() || bSpawning || !IsValid(Boss) || SpawnPointGroupId.IsNone())
	{
		return;
	}

	UBossPhaseComponent* BossPhaseComponent = Boss->GetBossPhaseComponent();
	if (!IsValid(BossPhaseComponent))
	{
		UE_LOG(LogTemp, Warning, TEXT("[BossAddSpawn] Start rejected | Event=%s | Reason=PhaseComponentMissing"), *GetNameSafe(this));
		return;
	}

	CacheSpawnPoints();
	if (BossAddSpawnPoints.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("[BossAddSpawn] Start rejected | Event=%s | Group=%s | Reason=NoSpawnPoints"),
			*GetNameSafe(this), *SpawnPointGroupId.ToString());
		return;
	}

	PhaseComponent = BossPhaseComponent;
	BossPhaseComponent->OnPhaseTransitionRequested.AddUniqueDynamic(this, &ThisClass::HandlePhaseTransitionRequested);
	BossPhaseComponent->OnPhaseChanged.AddUniqueDynamic(this, &ThisClass::HandlePhaseChanged);
	BossDeathStartedHandle = Boss->OnDeathStarted.AddUObject(this, &ThisClass::HandleBossDeathStarted);
	bSpawning = true;

	StartTimerForCurrentPhase();
}

void ABossAddSpawnEvent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopSpawning();
	Super::EndPlay(EndPlayReason);
}

void ABossAddSpawnEvent::HandlePhaseTransitionRequested(int32 NextPhaseIndex, FGameplayTag NextPhaseId)
{
	// 전환 연출이 끝나기 전에는 예약된 스폰도 실행하지 않는다.
	GetWorldTimerManager().ClearTimer(SpawnTimer);
}

void ABossAddSpawnEvent::HandlePhaseChanged(int32 CurrentPhaseIndex, FGameplayTag CurrentPhaseId)
{
	// CompletePhaseTransition 직후에만 새 페이즈의 주기로 타이머를 다시 건다.
	StartTimerForCurrentPhase();
}

void ABossAddSpawnEvent::HandleBossDeathStarted(ABaseCharacter* DeadCharacter)
{
	// 새 소환을 멈추는 것만으로는 이미 나온 애드가 남는다. 보스와 함께 죽인다
	for (const TWeakObjectPtr<ASpawnPoint>& WeakPoint : BossAddSpawnPoints)
	{
		if (ASpawnPoint* Point = WeakPoint.Get())
		{
			Point->KillAllMonsters();
		}
	}

	StopSpawning();
}

void ABossAddSpawnEvent::CacheSpawnPoints()
{
	BossAddSpawnPoints.Reset();
	for (ASpawnPoint* Point : FSpawnGroupLinker::FindSpawnPoints(this, SpawnPointGroupId))
	{
		if (IsValid(Point))
		{
			// 주기마다 쌓이도록 포인트가 여러 마리를 쥐게 하고, 애드는 시야와 무관하게 끝까지 쫓게 한다
			Point->SetAllowMultipleSpawns(true);
			Point->SetTrackAllPlayers(true, TrackScoringRange);
			BossAddSpawnPoints.Add(Point);
		}
	}
}

void ABossAddSpawnEvent::StartTimerForCurrentPhase()
{
	GetWorldTimerManager().ClearTimer(SpawnTimer);

	if (!bSpawning || !PhaseComponent.IsValid() || PhaseComponent->IsPhaseTransitionInProgress())
	{
		return;
	}

	const FBossAddPhaseSpawnRule* Rule = GetCurrentPhaseRule();
	if (!Rule || Rule->SpawnInterval <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	GetWorldTimerManager().SetTimer(SpawnTimer, this, &ThisClass::SpawnToCurrentTarget, Rule->SpawnInterval, false);
}

void ABossAddSpawnEvent::SpawnToCurrentTarget()
{
	if (!bSpawning || !PhaseComponent.IsValid() || PhaseComponent->IsPhaseTransitionInProgress())
	{
		return;
	}

	const FBossAddPhaseSpawnRule* Rule = GetCurrentPhaseRule();
	if (!Rule)
	{
		return;
	}

	TArray<ASpawnPoint*> AvailablePoints;
	int32 AliveAddCount = 0;
	for (const TWeakObjectPtr<ASpawnPoint>& WeakPoint : BossAddSpawnPoints)
	{
		ASpawnPoint* Point = WeakPoint.Get();
		if (!IsValid(Point))
		{
			continue;
		}

		AliveAddCount += Point->GetAliveMultiSpawnCount();
		if (Point->CanSpawn())
		{
			AvailablePoints.Add(Point);
		}
	}

	// 살아 있는 애드와 상관없이 이번 주기에 정해진 수만큼 소환한다. 한 포인트는 한 번 소환에 한 번만 쓴다
	int32 SpawnCount = FMath::Min(GetSpawnCountPerWave(*Rule, GetParticipatingPlayerCount()), AvailablePoints.Num());

	// 쌓이기만 하므로 전체 상한을 넘기지 않게 깎는다
	if (MaxAliveAdds > 0)
	{
		SpawnCount = FMath::Min(SpawnCount, FMath::Max(MaxAliveAdds - AliveAddCount, 0));
	}

	for (int32 i = 0; i < SpawnCount; ++i)
	{
		const int32 RandomIndex = FMath::RandRange(0, AvailablePoints.Num() - 1);
		ASpawnPoint* Point = AvailablePoints[RandomIndex];
		AvailablePoints.RemoveAtSwap(RandomIndex);
		Point->Execute(this);
	}

	StartTimerForCurrentPhase();
}

void ABossAddSpawnEvent::StopSpawning()
{
	GetWorldTimerManager().ClearTimer(SpawnTimer);

	if (PhaseComponent.IsValid())
	{
		PhaseComponent->OnPhaseTransitionRequested.RemoveDynamic(this, &ThisClass::HandlePhaseTransitionRequested);
		PhaseComponent->OnPhaseChanged.RemoveDynamic(this, &ThisClass::HandlePhaseChanged);
	}
	PhaseComponent.Reset();

	if (IsValid(Boss) && BossDeathStartedHandle.IsValid())
	{
		Boss->OnDeathStarted.Remove(BossDeathStartedHandle);
	}
	BossDeathStartedHandle.Reset();
	bSpawning = false;
}

const FBossAddPhaseSpawnRule* ABossAddSpawnEvent::GetCurrentPhaseRule() const
{
	const int32 PhaseIndex = PhaseComponent.IsValid() ? PhaseComponent->GetCurrentPhaseIndex() : INDEX_NONE;
	return PhaseRules.IsValidIndex(PhaseIndex) ? &PhaseRules[PhaseIndex] : nullptr;
}

int32 ABossAddSpawnEvent::GetParticipatingPlayerCount() const
{
	int32 Result = 0;
	if (const UWorld* World = GetWorld())
	{
		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			const APlayerController* PlayerController = It->Get();
			if (IsValid(PlayerController) && IsValid(PlayerController->GetPawn()))
			{
				++Result;
			}
		}
	}
	return Result;
}

int32 ABossAddSpawnEvent::GetSpawnCountPerWave(const FBossAddPhaseSpawnRule& Rule, const int32 PlayerCount) const
{
	if (PlayerCount <= 0 || Rule.MaxActiveSpawnPointsByPlayerCount.IsEmpty())
	{
		return 0;
	}

	// 포인트 수로 자르는 건 호출한 쪽이 비어 있는 포인트 수로 한다
	const int32 PlayerIndex = FMath::Clamp(PlayerCount - 1, 0, Rule.MaxActiveSpawnPointsByPlayerCount.Num() - 1);
	return FMath::Max(Rule.MaxActiveSpawnPointsByPlayerCount[PlayerIndex], 0);
}
