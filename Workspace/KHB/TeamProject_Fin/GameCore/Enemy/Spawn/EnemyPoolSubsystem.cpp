#include "EnemyPoolSubsystem.h"

#include "Engine/GameInstance.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "GameCore/Character/MonsterCharacter.h"
#include "GameCore/Character/CommonEnemyCharacter.h"
#include "GameCore/Enemy/Spawn/EnemySpawnSettings.h"
#include "GameCore/Enemy/Spawn/EnemySpawnTable.h"
#include "GameCore/Enemy/Spawn/SpawnPoint.h"
#include "GameCore/Interface/Poolable.h"
#include "PCGMap/MapBuild/SectorBuild/SectorSubsystem.h"

namespace
{
/*
	 * 풀링 효과를 재기 위한 비교 스위치. 0이면 풀 없이 매번 SpawnActor로 만들고 반환 때 Destroy 한다.
	 * 스폰 포인트·등장 방식은 그대로라 생성·파괴 방식만 달라진다. 플레이 중 콘솔에서 바꿔도 된다
	 */
TAutoConsoleVariable<int32> CVarEnemyPoolEnabled(
    TEXT("enemy.Pool.Enabled"), 1, TEXT("1: 오브젝트 풀링(기본). 0: 비교용 — 매번 새로 스폰하고 반환 시 파괴"));

bool IsPoolingEnabled()
{
	return CVarEnemyPoolEnabled.GetValueOnGameThread() != 0;
}
}

bool UEnemyPoolSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UEnemyPoolSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	if (UGameInstance* GI = InWorld.GetGameInstance())
	{
		if (USectorSubsystem* SectorSub = GI->GetSubsystem<USectorSubsystem>())
		{
			SectorReadyHandle = SectorSub->OnSectorReady.AddUObject(this, &UEnemyPoolSubsystem::HandleSectorReady);
		}
	}
}

void UEnemyPoolSubsystem::Deinitialize()
{
	// 섹터 서브시스템은 게임 인스턴스에 살아서 이 월드보다 오래 간다. 끊지 않으면 죽은 풀을 부른다
	if (UWorld* World = GetWorld())
	{
		if (UGameInstance* GI = World->GetGameInstance())
		{
			if (USectorSubsystem* SectorSub = GI->GetSubsystem<USectorSubsystem>())
			{
				SectorSub->OnSectorReady.Remove(SectorReadyHandle);
			}
		}
	}

	// 액터는 월드와 함께 엔진이 지운다. 참조만 놓는다
	IdlePool.Reset();
	ActiveTypes.Reset();
	Demands.Reset();

	Super::Deinitialize();
}

bool UEnemyPoolSubsystem::IsServer() const
{
	const UWorld* World = GetWorld();
	return World && World->GetNetMode() != NM_Client;
}

void UEnemyPoolSubsystem::RegisterDemand(ASpawnPoint* SpawnPoint)
{
	if (IsServer() && IsValid(SpawnPoint))
	{
		Demands.AddUnique(SpawnPoint);
	}
}

void UEnemyPoolSubsystem::UnregisterDemand(ASpawnPoint* SpawnPoint)
{
	Demands.Remove(SpawnPoint);
}

void UEnemyPoolSubsystem::HandleSectorReady()
{
	if (!IsServer())
	{
		return;
	}

	// 비교 모드는 미리 만들지 않는다. 자동 실행 포인트는 신호를 받고 그 자리에서 새로 만든다
	if (IsPoolingEnabled())
	{
		Prewarm();
	}
	OnPrewarmCompleted.Broadcast();
}

void UEnemyPoolSubsystem::Prewarm()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(EnemyPool_Prewarm);
	const double StartTime = FPlatformTime::Seconds();

	Demands.RemoveAll(
	    [](const TWeakObjectPtr<ASpawnPoint>& Point)
	    {
		    return !Point.IsValid();
	    });

	TMap<EEnemyType, int32> Needed;
	for (const TWeakObjectPtr<ASpawnPoint>& Point : Demands)
	{
		// 이미 몹을 꺼낸 포인트는 그 몹을 쥐고 있으니 새로 만들 몫이 아니다
		if (!Point->HasSpawned())
		{
			Needed.FindOrAdd(Point->GetEnemyType())++;
		}
	}

	for (const TPair<EEnemyType, int32>& Pair : Needed)
	{
		const FEnemyPoolBucket* Bucket = IdlePool.Find(Pair.Key);
		const int32 Missing = Pair.Value - (Bucket ? Bucket->Idle.Num() : 0);

		for (int32 i = 0; i < Missing; ++i)
		{
			if (AActor* Created = CreatePooled(Pair.Key))
			{
				IdlePool.FindOrAdd(Pair.Key).Idle.Add(Created);
			}
		}

		UE_LOG(LogTemp, Log, TEXT("[EnemyPool] Prewarm | Type=%s | Demand=%d | Created=%d"),
		       *UEnum::GetValueAsString(Pair.Key), Pair.Value, FMath::Max(Missing, 0));
	}

	UE_LOG(LogTemp, Log, TEXT("[EnemyPool] Prewarm Done | Time=%.2fms"),
	       (FPlatformTime::Seconds() - StartTime) * 1000.0);
}

AActor* UEnemyPoolSubsystem::CreatePooled(EEnemyType Type, UClass* ExactClass, bool bSkipRandomTrait)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(EnemyPool_Create);

	UWorld* World = GetWorld();
	const UEnemySpawnSettings* Settings = GetDefault<UEnemySpawnSettings>();
	const UEnemySpawnTable* Table = Settings->SpawnTable.LoadSynchronous();
	if (!World || (!ExactClass && !Table))
	{
		UE_LOG(LogTemp, Error, TEXT("[EnemyPool] Project Settings > Enemy Spawn 에 SpawnTable 이 없다"));
		return nullptr;
	}

	UClass* EnemyClass = ExactClass ? ExactClass : Table->FindEnemyClass(Type).LoadSynchronous();
	if (!EnemyClass)
	{
		UE_LOG(LogTemp, Error, TEXT("[EnemyPool] SpawnTable 에 등록되지 않은 종류: %s"),
		       *UEnum::GetValueAsString(Type));
		return nullptr;
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	// 스폰 포인트가 스트리밍 레벨에 있어도 몹은 영구 레벨에 둔다. 레벨이 내려갈 때 휩쓸리지 않게
	Params.OverrideLevel = World->PersistentLevel;

	Params.bDeferConstruction = true;
	const FTransform StagingTransform(Settings->StagingLocation);
	AActor* Created = World->SpawnActor<AActor>(EnemyClass, StagingTransform, Params);
	if (!Created)
	{
		return nullptr;
	}
	if (ACommonEnemyCharacter* Common = Cast<ACommonEnemyCharacter>(Created))
	{
		Common->bSkipRandomTraitForAcquire = bSkipRandomTrait;
	}
	Created->FinishSpawning(StagingTransform);

	// BP가 AutoPossessAI를 PlacedInWorld로 두었으면 스폰된 폰은 컨트롤러 없이 남는다
	if (APawn* Pawn = Cast<APawn>(Created); Pawn && !Pawn->GetController())
	{
		Pawn->SpawnDefaultController();
	}

	if (IPoolable* Poolable = Cast<IPoolable>(Created))
	{
		Poolable->OnReleased();
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[EnemyPool] IPoolable 을 구현하지 않은 클래스: %s"), *EnemyClass->GetName());
	}

	return Created;
}

AActor* UEnemyPoolSubsystem::Acquire(EEnemyType Type, const FTransform& SpawnTransform)
{
	if (!IsServer())
	{
		return nullptr;
	}

	TRACE_CPUPROFILER_EVENT_SCOPE(EnemyPool_Acquire);
	const double StartTime = FPlatformTime::Seconds();
	const bool bPooling = IsPoolingEnabled();

	AActor* Actor = nullptr;
	FEnemyPoolBucket* Bucket = bPooling ? IdlePool.Find(Type) : nullptr;
	if (Bucket)
	{
		// 월드 정리 중에 지워진 것이 섞여 있을 수 있다
		while (!Actor && Bucket->Idle.Num() > 0)
		{
			AActor* Candidate = Bucket->Idle.Pop(EAllowShrinking::No);
			Actor = IsValid(Candidate) ? Candidate : nullptr;
		}
	}

	const bool bReused = Actor != nullptr;
	if (!Actor)
	{
		if (bPooling)
		{
			UE_LOG(LogTemp, Warning, TEXT("[EnemyPool] 쉬는 몹이 없어 즉시 생성 | Type=%s — 준비 단계 수량 확인"),
			       *UEnum::GetValueAsString(Type));
		}
		Actor = CreatePooled(Type);
	}

	if (!Actor)
	{
		return nullptr;
	}

	ActiveTypes.Add(Actor, Type);
	if (IPoolable* Poolable = Cast<IPoolable>(Actor))
	{
		Poolable->OnAcquired(SpawnTransform);
	}

	UE_LOG(LogTemp, Log, TEXT("[EnemyPool] Acquire | Mode=%s | Type=%s | Actor=%s | Reused=%s | Time=%.2fms | Idle=%d"),
	       bPooling ? TEXT("Pool") : TEXT("Spawn"), *UEnum::GetValueAsString(Type), *GetNameSafe(Actor),
	       bReused ? TEXT("true") : TEXT("false"), (FPlatformTime::Seconds() - StartTime) * 1000.0,
	       Bucket ? Bucket->Idle.Num() : 0);

	return Actor;
}

void UEnemyPoolSubsystem::Release(AActor* Actor)
{
	if (!IsServer() || !IsValid(Actor))
	{
		return;
	}

	EEnemyType Type;
	if (!ActiveTypes.RemoveAndCopyValue(Actor, Type))
	{
		UE_LOG(LogTemp, Warning, TEXT("[EnemyPool] 이 풀이 내준 액터가 아니다: %s"), *GetNameSafe(Actor));
		return;
	}

	TRACE_CPUPROFILER_EVENT_SCOPE(EnemyPool_Release);
	const double StartTime = FPlatformTime::Seconds();
	const FString ActorName = GetNameSafe(Actor);

	if (IPoolable* Poolable = Cast<IPoolable>(Actor))
	{
		Poolable->OnReleased();
	}

	if (IsPoolingEnabled())
	{
		Actor->SetActorLocation(GetDefault<UEnemySpawnSettings>()->StagingLocation, false, nullptr,
		                        ETeleportType::TeleportPhysics);
		IdlePool.FindOrAdd(Type).Idle.Add(Actor);
	}
	else
	{
		// 비교 모드는 풀 없는 일반 스폰처럼 파괴한다. AI 컨트롤러도 따로 스폰됐으니 같이 지운다
		AController* Controller = nullptr;
		if (const APawn* Pawn = Cast<APawn>(Actor))
		{
			Controller = Pawn->GetController();
		}
		Actor->Destroy();
		if (IsValid(Controller) && !Controller->IsA<APlayerController>())
		{
			Controller->Destroy();
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[EnemyPool] Release | Mode=%s | Type=%s | Actor=%s | Time=%.2fms"),
	       IsPoolingEnabled() ? TEXT("Pool") : TEXT("Spawn"), *UEnum::GetValueAsString(Type), *ActorName,
	       (FPlatformTime::Seconds() - StartTime) * 1000.0);
}

ACommonEnemyCharacter* UEnemyPoolSubsystem::AcquireForSplit(ACommonEnemyCharacter* Parent,
                                                            const FTransform& SpawnTransform)
{
	if (!IsServer() || !IsValid(Parent))
	{
		return nullptr;
	}
	const EEnemyType* ParentType = ActiveTypes.Find(Parent);
	EEnemyType PlacedType = EEnemyType::Melee;
	if (!ParentType)
	{
		// 레벨에 배치된 원본은 풀에 등록되지 않으므로 스폰 테이블에서 동일한 BP 클래스를 찾습니다.
		const auto* Table = GetDefault<UEnemySpawnSettings>()->SpawnTable.LoadSynchronous();
		if (Table)
		{
			for (const auto& Entry : Table->Enemies)
			{
				if (Entry.Value.LoadSynchronous() == Parent->GetClass())
				{
					PlacedType = Entry.Key;
					ParentType = &PlacedType;
					break;
				}
			}
		}
		if (!ParentType)
		{
			UE_LOG(LogTemp, Warning, TEXT("[EnemyPool] SplitAcquireFailed | Parent=%s | Reason=ClassNotInSpawnTable"),
			       *GetNameSafe(Parent));
			return nullptr;
		}
	}
	// 맵에 항목을 추가하면 값의 포인터가 무효화될 수 있으므로 먼저 복사합니다.
	const EEnemyType Type = *ParentType;
	ACommonEnemyCharacter* Child = nullptr;
	if (IsPoolingEnabled())
	{
		if (FEnemyPoolBucket* Bucket = IdlePool.Find(Type))
		{
			for (int32 Index = Bucket->Idle.Num() - 1; Index >= 0; --Index)
			{
				AActor* Candidate = Bucket->Idle[Index];
				if (!IsValid(Candidate))
				{
					Bucket->Idle.RemoveAtSwap(Index);
					continue;
				}
				if (Candidate->GetClass() == Parent->GetClass())
				{
					Child = Cast<ACommonEnemyCharacter>(Candidate);
					if (Child)
					{
						Bucket->Idle.RemoveAtSwap(Index);
						break;
					}
				}
			}
		}
	}
	const bool bReused = Child != nullptr;
	if (!Child)
	{
		Child = Cast<ACommonEnemyCharacter>(CreatePooled(Type, Parent->GetClass(), true));
	}
	if (!IsValid(Child))
	{
		return nullptr;
	}
	Child->bSkipRandomTraitForAcquire = true;
	// 초기화 시 클래스 기본값을 복원하고, 호출자가 이후 분열 설정을 상속합니다.
	Child->EnemyDefinition = Child->GetClass()->GetDefaultObject<ACommonEnemyCharacter>()->EnemyDefinition;
	ActiveTypes.Add(Child, Type);
	Child->OnAcquired(SpawnTransform);
	Child->bSkipRandomTraitForAcquire = false;
	// 일반 획득은 바닥 좌표를 받고 크기를 복원하지만, 분열 배치 좌표는 캡슐 중심입니다.
	Child->SetActorTransform(SpawnTransform, false, nullptr, ETeleportType::TeleportPhysics);
	UE_LOG(LogTemp, Log, TEXT("[EnemyPool] AcquireForSplit | Parent=%s | Child=%s | Reused=%s"), *GetNameSafe(Parent),
	       *GetNameSafe(Child), bReused ? TEXT("true") : TEXT("false"));
	return Child;
}
