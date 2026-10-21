#include "SpawnPoint.h"

#include "Components/ArrowComponent.h"
#include "EngineUtils.h"
#include "PCGMap/MapBuild/SectorBuild/Event/PlacedEvent/BattleSector/ConstBattleSector.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "GameCore/AI/MonsterAIController.h"
#include "GameCore/Character/MonsterCharacter.h"
#include "GameCore/Enemy/Spawn/EmergeAppearance.h"
#include "GameCore/Enemy/Spawn/EnemyActivationSubsystem.h"
#include "GameCore/Enemy/Spawn/EnemyPoolSubsystem.h"
#include "GameCore/Enemy/Spawn/EnemySpawnSettings.h"
#include "GameCore/Enemy/Spawn/PatrolAppearance.h"
#include "GameCore/Enemy/Spawn/SpawnAppearance.h"

namespace
{
// 플레이어마다 자기 인보커 반경으로 잰다. Distance는 활성화 거리와 비활성화 거리 중 무엇을 쓸지 고른다
bool IsWithinAnyViewer(const FVector& From, const TArray<FActivationViewer>& Viewers,
                       float FActivationViewer::* Distance)
{
	for (const FActivationViewer& Viewer : Viewers)
	{
		const double DistSq = FMath::Square(static_cast<double>(Viewer.*Distance));
		if (FVector::DistSquared(From, Viewer.Location) <= DistSq)
		{
			return true;
		}
	}
	return false;
}
}

ASpawnPoint::ASpawnPoint()
{
	PrimaryActorTick.bCanEverTick = false;

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

#if WITH_EDITORONLY_DATA
	// 몹이 바라볼 방향. 에디터에서만 보인다
	UArrowComponent* Arrow = CreateEditorOnlyDefaultSubobject<UArrowComponent>(TEXT("Arrow"));
	if (Arrow)
	{
		Arrow->SetupAttachment(RootComponent);
		Arrow->ArrowColor = FColor::Red;
	}
#endif

	// 가장 흔한 쓰임이 순찰이라 기본으로 채워 둔다. 드롭다운에서 바꾼다
	Appearance = CreateDefaultSubobject<UPatrolAppearance>(TEXT("Appearance"));
}

bool ASpawnPoint::UsesEmerge() const
{
	return IsValid(Appearance) && Appearance->IsA<UEmergeAppearance>();
}

void ASpawnPoint::BeginPlay()
{
	Super::BeginPlay();

	// 조각 BP의 ChildActor는 클라에도 생긴다. 스폰과 수요 계산, 거리 판정은 서버 몫이다
	if (!HasAuthority())
	{
		return;
	}

	UWorld* World = GetWorld();
	if (UEnemyPoolSubsystem* Pool = World->GetSubsystem<UEnemyPoolSubsystem>())
	{
		Pool->RegisterDemand(this);

		if (IsValid(Appearance) && Appearance->ShouldAutoExecute())
		{
			PrewarmHandle = Pool->OnPrewarmCompleted.AddUObject(this, &ASpawnPoint::HandlePrewarmCompleted);
		}
	}

	if (UEnemyActivationSubsystem* Activation = World->GetSubsystem<UEnemyActivationSubsystem>())
	{
		Activation->Register(this);
	}
}

void ASpawnPoint::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(ReleaseTimer);

	UWorld* World = GetWorld();
	if (UEnemyActivationSubsystem* Activation = World ? World->GetSubsystem<UEnemyActivationSubsystem>() : nullptr)
	{
		Activation->Unregister(this);
	}

	if (UEnemyPoolSubsystem* Pool = World ? World->GetSubsystem<UEnemyPoolSubsystem>() : nullptr)
	{
		Pool->OnPrewarmCompleted.Remove(PrewarmHandle);
		Pool->UnregisterDemand(this);

		/*
		 * 섹터가 사라지는 경우만 돌려준다. 조각 파괴(Destroyed), 스트리밍 언로드(RemovedFromWorld).
		 * 월드 전환·종료면 풀도 같이 사라지는 중이라 건드리지 않는다
		 */
		if (EndPlayReason == EEndPlayReason::Destroyed || EndPlayReason == EEndPlayReason::RemovedFromWorld)
		{
			ReleaseMonster();

			// 반납 대기 중인 시체도 함께 돌려준다. 이 포인트가 사라지면 예약된 반납이 불리지 않는다
			TArray<FMultiSpawnEntry> Remaining = MoveTemp(MultiSpawns);
			for (const FMultiSpawnEntry& Entry : Remaining)
			{
				ReleaseMultiMonster(Entry.Monster.Get());
			}
		}
	}

	Super::EndPlay(EndPlayReason);
}

void ASpawnPoint::HandlePrewarmCompleted()
{
	if (UEnemyPoolSubsystem* Pool = GetWorld()->GetSubsystem<UEnemyPoolSubsystem>())
	{
		Pool->OnPrewarmCompleted.Remove(PrewarmHandle);
	}
	PrewarmHandle.Reset();

	// 등장은 플레이어가 활성화 거리 안에 들어올 때 UpdateActivation이 한다
	bActivationReady = true;
}

void ASpawnPoint::Execute(const UObject* WorldContext)
{
	if (!HasAuthority() || !CanSpawn())
	{
		return;
	}

	if (!IsValid(Appearance))
	{
		UE_LOG(LogTemp, Error, TEXT("[SpawnPoint] Appearance 가 비어 있다: %s"), *GetNameSafe(this));
		return;
	}

	AMonsterCharacter* Monster = AcquireMonster();
	if (!Monster)
	{
		return;
	}

	if (bAllowMultipleSpawns)
	{
		FMultiSpawnEntry& Entry = MultiSpawns.AddDefaulted_GetRef();
		Entry.Monster = Monster;
	}
	else
	{
		bSpawned = true;
		bMonsterDied = false;
		FarTime = 0.0f;
		SpawnedMonster = Monster;
	}

	Appearance->Apply(Monster, const_cast<AActor*>(Cast<AActor>(WorldContext)));
}

AMonsterCharacter* ASpawnPoint::AcquireMonster()
{
	const EEnemyType SpawnEnemyType = ResolveSpawnEnemyType();
	UEnemyPoolSubsystem* Pool = GetWorld()->GetSubsystem<UEnemyPoolSubsystem>();
	AMonsterCharacter* Monster =
	    Pool ? Cast<AMonsterCharacter>(Pool->Acquire(SpawnEnemyType, GetActorTransform())) : nullptr;
	if (!Monster)
	{
		UE_LOG(LogTemp, Error, TEXT("[SpawnPoint] 몹을 받지 못했다: %s"), *GetNameSafe(this));
		return nullptr;
	}

	Monster->OnMonsterDeath.AddUniqueDynamic(this, &ASpawnPoint::HandleMonsterDeath);

	// 등장보다 먼저 넘긴다. Emerge의 첫 타깃 평가부터 적용돼야 조우 타깃이 지워지지 않는다
	if (AMonsterAIController* AC = Cast<AMonsterAIController>(Monster->GetController()))
	{
		AC->SetTrackAllPlayers(bTrackAllPlayers, TrackScoringRange);
	}

	return Monster;
}

bool ASpawnPoint::HasAliveMonster() const
{
	if (SpawnedMonster.IsValid() && !bMonsterDied)
	{
		return true;
	}

	return GetAliveMultiSpawnCount() > 0;
}

int32 ASpawnPoint::GetAliveMultiSpawnCount() const
{
	int32 Count = 0;
	for (const FMultiSpawnEntry& Entry : MultiSpawns)
	{
		if (Entry.Monster.IsValid() && !Entry.bDead)
		{
			++Count;
		}
	}
	return Count;
}

AMonsterCharacter* ASpawnPoint::GetSpawnedMonster() const
{
	if (AMonsterCharacter* Monster = SpawnedMonster.Get())
	{
		return Monster;
	}

	for (const FMultiSpawnEntry& Entry : MultiSpawns)
	{
		if (!Entry.bDead && Entry.Monster.IsValid())
		{
			return Entry.Monster.Get();
		}
	}
	return nullptr;
}

void ASpawnPoint::KillAllMonsters()
{
	if (!HasAuthority())
	{
		return;
	}

	// 사망 처리 중 콜백이 목록을 건드려도 안전하도록 먼저 모아 둔다
	TArray<TWeakObjectPtr<AMonsterCharacter>> Targets;
	if (SpawnedMonster.IsValid() && !bMonsterDied)
	{
		Targets.Add(SpawnedMonster);
	}
	for (const FMultiSpawnEntry& Entry : MultiSpawns)
	{
		if (Entry.Monster.IsValid() && !Entry.bDead)
		{
			Targets.Add(Entry.Monster);
		}
	}

	for (const TWeakObjectPtr<AMonsterCharacter>& Target : Targets)
	{
		if (AMonsterCharacter* Monster = Target.Get())
		{
			Monster->KillWithoutReward();
		}
	}
}

void ASpawnPoint::SetTrackAllPlayers(const bool bEnable, const float ScoringRange)
{
	bTrackAllPlayers = bEnable;
	TrackScoringRange = ScoringRange;
}

EEnemyType ASpawnPoint::ResolveSpawnEnemyType() const
{
	if (!bUseRandomEnemyType || RandomEnemyTypes.IsEmpty())
	{
		return EnemyType;
	}

	return RandomEnemyTypes[FMath::RandRange(0, RandomEnemyTypes.Num() - 1)];
}

void ASpawnPoint::UpdateActivation(const TArray<FActivationViewer>& Viewers, const float Interval)
{
	if (!HasAuthority() || !IsValid(Appearance))
	{
		return;
	}

	if (bAllowMultipleSpawns || !MultiSpawns.IsEmpty())
	{
		UpdateMultiActivation(Viewers, Interval);
		return;
	}

	const bool bAutoActivate = Appearance->ShouldAutoExecute();
	const UEnemySpawnSettings* Settings = GetDefault<UEnemySpawnSettings>();

	// 몹이 없다. 자동 활성화 포인트만 플레이어가 가까우면 꺼낸다
	if (!bSpawned)
	{
		if (bAutoActivate && bActivationReady &&
		    IsWithinAnyViewer(GetActorLocation(), Viewers, &FActivationViewer::ActivationDistance))
		{
			Execute(nullptr);
		}
		return;
	}

	// 죽은 몹은 사망 연출 뒤 반납을 기다리는 중이다
	AMonsterCharacter* Monster = SpawnedMonster.Get();
	if (!IsValid(Monster) || bMonsterDied)
	{
		return;
	}

	// 전원 추적 몹(보스 애드)은 멀어도 쫓아와야 한다. 거리로 재우거나 돌려보내지 않는다
	if (bTrackAllPlayers)
	{
		return;
	}

	if (IsWithinAnyViewer(Monster->GetActorLocation(), Viewers, &FActivationViewer::DeactivationDistance))
	{
		FarTime = 0.0f;
		Monster->SetDistanceSleep(false);
		return;
	}

	// 경계에서 들락날락해도 깜빡이지 않게 유예 시간을 둔다
	FarTime += Interval;
	if (FarTime < Settings->DeactivationGraceTime)
	{
		return;
	}

	if (bAutoActivate)
	{
		// 순찰 몹은 싸우는 중이 아니면 돌려보낸다. 싸우는 중이면 StateTree가 놓치고 순찰로 돌아올 때까지 기다린다
		if (!Monster->IsEngagedInCombat())
		{
			ReleaseForDistance();
		}
	}
	else
	{
		// Emerge 등 전투 섹터에 묶인 몹은 사라지면 전멸 판정이 막힌다. 그 자리에 재운다
		Monster->SetDistanceSleep(true);
	}
}

void ASpawnPoint::UpdateMultiActivation(const TArray<FActivationViewer>& Viewers, const float Interval)
{
	const bool bAutoActivate = Appearance->ShouldAutoExecute();
	const UEnemySpawnSettings* Settings = GetDefault<UEnemySpawnSettings>();

	// 자동 활성화(순찰)는 쥔 몹이 하나도 없을 때만 새로 꺼낸다. 여러 마리 추가는 Execute를 부르는 쪽이 정한다
	if (MultiSpawns.IsEmpty())
	{
		if (bAutoActivate && bActivationReady &&
		    IsWithinAnyViewer(GetActorLocation(), Viewers, &FActivationViewer::ActivationDistance))
		{
			Execute(nullptr);
		}
		return;
	}

	// 전원 추적 몹(보스 애드)은 멀어도 쫓아와야 한다. 거리로 재우거나 돌려보내지 않는다
	if (bTrackAllPlayers)
	{
		return;
	}

	TArray<AMonsterCharacter*> ToRelease;

	for (FMultiSpawnEntry& Entry : MultiSpawns)
	{
		AMonsterCharacter* Monster = Entry.Monster.Get();
		if (!IsValid(Monster) || Entry.bDead)
		{
			continue;
		}

		if (IsWithinAnyViewer(Monster->GetActorLocation(), Viewers, &FActivationViewer::DeactivationDistance))
		{
			Entry.FarTime = 0.0f;
			Monster->SetDistanceSleep(false);
			continue;
		}

		Entry.FarTime += Interval;
		if (Entry.FarTime < Settings->DeactivationGraceTime)
		{
			continue;
		}

		// 전투가 추적하는 분열 자식은 실제 사망할 때까지 등록을 유지합니다.
		bool bBattleTracked = false;
		for (TActorIterator<AConstBattleSector> It(GetWorld()); It; ++It)
		{
			if (It->TracksMonster(Monster))
			{
				bBattleTracked = true;
				break;
			}
		}
		if (bAutoActivate && !bBattleTracked)
		{
			if (!Monster->IsEngagedInCombat())
			{
				ToRelease.Add(Monster);
			}
		}
		else
		{
			Monster->SetDistanceSleep(true);
		}
	}

	// 순회 중에 목록을 바꾸지 않도록 모았다가 돌려준다
	for (AMonsterCharacter* Monster : ToRelease)
	{
		MultiSpawns.RemoveAll(
		    [Monster](const FMultiSpawnEntry& Entry)
		    {
			    return Entry.Monster.Get() == Monster;
		    });
		ReleaseMultiMonster(Monster);
	}
	if (!ToRelease.IsEmpty() && MultiSpawns.IsEmpty() && !SpawnedMonster.IsValid())
	{
		bSpawned = false;
		bMonsterDied = false;
	}
}

void ASpawnPoint::ReleaseMultiMonster(AMonsterCharacter* Monster)
{
	if (!IsValid(Monster))
	{
		return;
	}

	Monster->OnMonsterDeath.RemoveDynamic(this, &ASpawnPoint::HandleMonsterDeath);

	if (UEnemyPoolSubsystem* Pool = GetWorld() ? GetWorld()->GetSubsystem<UEnemyPoolSubsystem>() : nullptr)
	{
		if (Pool->IsActive(Monster))
		{
			Pool->Release(Monster);
		}
		else
		{
			AController* Controller = Monster->GetController();
			Monster->Destroy();
			if (Controller)
			{
				Controller->Destroy();
			}
		}
	}
}

void ASpawnPoint::HandleMonsterDeath(AMonsterCharacter* DeadMonster)
{
	// 여러 마리 모드: 죽은 몹만 표시해 두고 시체 시간 뒤 그 몹만 돌려준다
	if (FMultiSpawnEntry* Entry = MultiSpawns.FindByPredicate(
	        [DeadMonster](const FMultiSpawnEntry& Candidate)
	        {
		        return Candidate.Monster.Get() == DeadMonster;
	        }))
	{
		if (Entry->bDead)
		{
			return;
		}
		Entry->bDead = true;

		const TWeakObjectPtr<AMonsterCharacter> WeakMonster = DeadMonster;
		const FTimerDelegate ReleaseDelegate = FTimerDelegate::CreateWeakLambda(
		    this,
		    [this, WeakMonster]()
		    {
			    AMonsterCharacter* Monster = WeakMonster.Get();
			    MultiSpawns.RemoveAll(
			        [&WeakMonster](const FMultiSpawnEntry& Candidate)
			        {
				        return Candidate.Monster == WeakMonster;
			        });
			    ReleaseMultiMonster(Monster);
			    if (MultiSpawns.IsEmpty() && !SpawnedMonster.IsValid() && bReusableAfterDeath)
			    {
				    bSpawned = false;
				    bMonsterDied = false;
			    }
		    });

		const float Delay = DeadMonster->GetCorpseLifeSpan();
		if (Delay > KINDA_SMALL_NUMBER)
		{
			FTimerHandle Handle;
			GetWorldTimerManager().SetTimer(Handle, ReleaseDelegate, Delay, false);
		}
		else
		{
			// FinishDeath 안에서 바로 되돌리면 호출 도중 상태가 바뀐다. 다음 틱으로 미룬다
			GetWorldTimerManager().SetTimerForNextTick(ReleaseDelegate);
		}
		return;
	}

	if (DeadMonster != SpawnedMonster.Get() || bMonsterDied)
	{
		return;
	}

	bMonsterDied = true;

	// OnMonsterDeath는 사망 연출이 끝난 뒤에 온다. 시체를 잠깐 남기는 몹이면 그만큼 기다렸다 돌려준다
	const float Delay = DeadMonster->GetCorpseLifeSpan();
	if (Delay > KINDA_SMALL_NUMBER)
	{
		GetWorldTimerManager().SetTimer(ReleaseTimer, this, &ASpawnPoint::ReleaseMonster, Delay, false);
	}
	else
	{
		// FinishDeath 안에서 바로 되돌리면 호출 도중 상태가 바뀐다. 다음 틱으로 미룬다
		GetWorldTimerManager().SetTimerForNextTick(this, &ASpawnPoint::ReleaseMonster);
	}
}

void ASpawnPoint::ReleaseForDistance()
{
	UE_LOG(LogTemp, Log, TEXT("[SpawnPoint] 플레이어가 멀어져 반환 | Point=%s | Monster=%s"), *GetNameSafe(this),
	       *GetNameSafe(SpawnedMonster.Get()));

	ReleaseMonster();

	// 죽은 게 아니라서 포인트를 비워 두고, 다시 가까워지면 새로 꺼낸다
	bSpawned = false;
	bMonsterDied = false;
	FarTime = 0.0f;
}

void ASpawnPoint::ReleaseMonster()
{
	AMonsterCharacter* Monster = SpawnedMonster.Get();
	SpawnedMonster.Reset();

	if (IsValid(Monster))
	{
		Monster->OnMonsterDeath.RemoveDynamic(this, &ASpawnPoint::HandleMonsterDeath);

		if (UEnemyPoolSubsystem* Pool = GetWorld() ? GetWorld()->GetSubsystem<UEnemyPoolSubsystem>() : nullptr)
		{
			Pool->Release(Monster);
		}
	}

	// 보스 애드 포인트는 사망 후 다음 주기에서 다시 쓸 수 있다.
	// 거리 해제는 ReleaseForDistance()가 별도로 상태를 비우므로 여기서는 건드리지 않는다.
	if (bReusableAfterDeath && bMonsterDied)
	{
		bSpawned = false;
		bMonsterDied = false;
		FarTime = 0.0f;
	}
}

bool ASpawnPoint::TracksMonster(AMonsterCharacter* Monster) const
{
	return SpawnedMonster.Get() == Monster || MultiSpawns.ContainsByPredicate(
	                                              [Monster](const FMultiSpawnEntry& E)
	                                              {
		                                              return E.Monster.Get() == Monster;
	                                              });
}

void ASpawnPoint::ReplaceTrackedMonster(AMonsterCharacter* Parent, AMonsterCharacter* Left, AMonsterCharacter* Right)
{
	if (!HasAuthority() || !TracksMonster(Parent) || !IsValid(Left) || !IsValid(Right))
	{
		return;
	}
	Parent->OnMonsterDeath.RemoveDynamic(this, &ASpawnPoint::HandleMonsterDeath);
	if (SpawnedMonster.Get() == Parent)
	{
		SpawnedMonster.Reset();
	}
	MultiSpawns.RemoveAll(
	    [Parent](const FMultiSpawnEntry& E)
	    {
		    return E.Monster.Get() == Parent;
	    });
	for (AMonsterCharacter* Child : {Left, Right})
	{
		FMultiSpawnEntry Entry;
		Entry.Monster = Child;
		MultiSpawns.Add(Entry);
		Child->OnMonsterDeath.AddUniqueDynamic(this, &ASpawnPoint::HandleMonsterDeath);
		if (auto* AC = Cast<AMonsterAIController>(Child->GetController()))
		{
			AC->SetTrackAllPlayers(bTrackAllPlayers, TrackScoringRange);
		}
	}
}
