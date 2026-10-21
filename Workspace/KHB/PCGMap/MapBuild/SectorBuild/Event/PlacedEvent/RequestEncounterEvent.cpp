#include "RequestEncounterEvent.h"
#include "GameCore/Character/CommonEnemyCharacter.h"
#include "GameCore/BaseGameState.h"
#include "GameCore/Interface/EncounterTargetInterface.h"
#include "GameCore/Enemy/Spawn/EnemyPoolSubsystem.h"
#include "GameCore/Enemy/Spawn/SpawnPoint.h"
#include "EngineUtils.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

ARequestEncounterEvent::ARequestEncounterEvent()
{
	bReplicates = true;
	bAlwaysRelevant = true;
}

void ARequestEncounterEvent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ARequestEncounterEvent, TrackedEnemies);
	DOREPLIFETIME(ARequestEncounterEvent, EncounterName);
}

void ARequestEncounterEvent::Execute(const UObject* WorldContext)
{
	if (!HasAuthority() || bRequested || !IsValid(EncounterTarget))
	{
		return;
	}
	// 레벨 진입 이벤트가 대상의 스탯과 AI 초기화보다 먼저 실행될 수 있습니다.
	if (bExecutionDeferred)
	{
		return;
	}
	if (!EncounterTarget->HasActorBegunPlay())
	{
		bExecutionDeferred = true;
		const TWeakObjectPtr<UObject> Context = const_cast<UObject*>(WorldContext);
		GetWorldTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this,
		                                                                            [this, Context]()
		                                                                            {
			                                                                            bExecutionDeferred = false;
			                                                                            Execute(Context.Get());
		                                                                            }));
		return;
	}
	auto* Enemy = Cast<ACommonEnemyCharacter>(EncounterTarget);
	if (!Enemy || Enemy->HasStartedDeath())
	{
		return;
	}
	bRequested = true;
	EncounterName = Enemy->GetEnemyDisplayName();
	Track(Enemy);
	Enemy->EnablePlacedEncounterFlow();
	if (!IEncounterTargetInterface::Execute_RequestEncounter(Enemy, const_cast<AActor*>(Cast<AActor>(WorldContext))))
	{
		Enemy->OnDeathStarted.RemoveAll(this);
		Enemy->OnSplitCommitted.RemoveAll(this);
		Enemy->OnMonsterDeath.RemoveDynamic(this, &ThisClass::OnDeathFinished);
		TrackedEnemies.Reset();
		bRequested = false;
		UE_LOG(LogTemp, Warning, TEXT("[Encounter] RequestRejected | Event=%s | Enemy=%s"), *GetName(),
		       *Enemy->GetName());
		ForceNetUpdate();
		return;
	}
	// 시퀀스가 없으면 등장 연출 종료 이벤트가 즉시 실행될 수 있습니다.
	// 시작 이벤트를 실행하기 전에 조우의 연출 대기 상태를 먼저 설정해야 합니다.
	Super::Execute(WorldContext);
	ForceNetUpdate();
}

void ARequestEncounterEvent::Track(ACommonEnemyCharacter* Enemy)
{
	if (!IsValid(Enemy) || TrackedEnemies.Contains(Enemy))
	{
		return;
	}
	TrackedEnemies.Add(Enemy);
	Enemy->OnDeathStarted.AddUObject(this, &ThisClass::OnDeathStarted);
	Enemy->OnSplitCommitted.AddUObject(this, &ThisClass::OnSplit);
	Enemy->OnMonsterDeath.AddUniqueDynamic(this, &ThisClass::OnDeathFinished);
}

void ARequestEncounterEvent::OnSplit(ACommonEnemyCharacter* Parent, ACommonEnemyCharacter* Left,
                                     ACommonEnemyCharacter* Right)
{
	if (!HasAuthority() || !TracksEnemy(Parent))
	{
		return;
	}
	// 자식을 먼저 등록하며 부모 교체를 처치로 처리하지 않습니다.
	Track(Left);
	Track(Right);
	TrackedEnemies.Remove(Parent);
	Parent->OnDeathStarted.RemoveAll(this);
	Parent->OnSplitCommitted.RemoveAll(this);
	Parent->OnMonsterDeath.RemoveDynamic(this, &ThisClass::OnDeathFinished);
	ForceNetUpdate();
	UE_LOG(LogTemp, Log, TEXT("[Encounter] Replaced | Parent=%s | Remaining=%d"), *Parent->GetName(),
	       TrackedEnemies.Num());
}

void ARequestEncounterEvent::OnDeathStarted(ABaseCharacter* Character)
{
	auto* Enemy = Cast<ACommonEnemyCharacter>(Character);
	if (!Enemy || !TrackedEnemies.Contains(Enemy))
	{
		return;
	}
	TrackedEnemies.Remove(Enemy);
	RetiredEnemies.AddUnique(Enemy);
	Enemy->OnDeathStarted.RemoveAll(this);
	Enemy->OnSplitCommitted.RemoveAll(this);
	ForceNetUpdate();
	if (!TrackedEnemies.IsEmpty() || bCompleted)
	{
		return;
	}
	bCompleted = true;
	if (bFinalizeRunOnDefeat)
	{
		if (auto* GS = GetWorld()->GetGameState<ABaseGameState>())
		{
			GS->FinalizeRunStatistics();
		}
	}
	UE_LOG(LogTemp, Log, TEXT("[Encounter] Defeated | Event=%s"), *GetName());
	RunEventGroups(PostEvents);
}

bool ARequestEncounterEvent::HasSpawnOwner(ACommonEnemyCharacter* Enemy) const
{
	for (TActorIterator<ASpawnPoint> It(GetWorld()); It; ++It)
	{
		if (It->TracksMonster(Enemy))
		{
			return true;
		}
	}
	return false;
}

void ARequestEncounterEvent::ReturnOwnedEnemy(ACommonEnemyCharacter* Enemy)
{
	if (!IsValid(Enemy) || HasSpawnOwner(Enemy))
	{
		return;
	}
	if (auto* Pool = GetWorld()->GetSubsystem<UEnemyPoolSubsystem>(); Pool && Pool->IsActive(Enemy))
	{
		Pool->Release(Enemy);
	}
}

void ARequestEncounterEvent::OnDeathFinished(AMonsterCharacter* Character)
{
	auto* Enemy = Cast<ACommonEnemyCharacter>(Character);
	if (!Enemy)
	{
		return;
	}
	Enemy->OnMonsterDeath.RemoveDynamic(this, &ThisClass::OnDeathFinished);
	const TWeakObjectPtr<ACommonEnemyCharacter> WeakEnemy = Enemy;
	FTimerHandle Handle;
	GetWorldTimerManager().SetTimer(Handle,
	                                FTimerDelegate::CreateWeakLambda(this,
	                                                                 [this, WeakEnemy]()
	                                                                 {
		                                                                 if (!RetiredEnemies.Contains(WeakEnemy))
		                                                                 {
			                                                                 return;
		                                                                 }
		                                                                 RetiredEnemies.Remove(WeakEnemy);
		                                                                 ReturnOwnedEnemy(WeakEnemy.Get());
	                                                                 }),
	                                FMath::Max(0.01f, Enemy->GetCorpseLifeSpan()), false);
}

void ARequestEncounterEvent::EndPlay(const EEndPlayReason::Type Reason)
{
	for (ACommonEnemyCharacter* Enemy : TrackedEnemies)
	{
		if (IsValid(Enemy))
		{
			Enemy->OnDeathStarted.RemoveAll(this);
			Enemy->OnSplitCommitted.RemoveAll(this);
			Enemy->OnMonsterDeath.RemoveDynamic(this, &ThisClass::OnDeathFinished);
			if (HasAuthority() && (Reason == EEndPlayReason::Destroyed || Reason == EEndPlayReason::RemovedFromWorld))
			{
				ReturnOwnedEnemy(Enemy);
			}
		}
	}
	for (const auto& WeakEnemy : RetiredEnemies)
	{
		if (auto* Enemy = WeakEnemy.Get())
		{
			Enemy->OnMonsterDeath.RemoveDynamic(this, &ThisClass::OnDeathFinished);
			if (HasAuthority() && (Reason == EEndPlayReason::Destroyed || Reason == EEndPlayReason::RemovedFromWorld))
			{
				ReturnOwnedEnemy(Enemy);
			}
		}
	}
	RetiredEnemies.Reset();
	Super::EndPlay(Reason);
}
