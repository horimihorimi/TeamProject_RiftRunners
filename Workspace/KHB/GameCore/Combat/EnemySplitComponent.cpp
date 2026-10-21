#include "EnemySplitComponent.h"
#include "AbilitySystemComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "TimerManager.h"
#include "GameCore/Attribute/CharacterAttributeSet.h"
#include "GameCore/Character/CommonEnemyCharacter.h"
#include "GameCore/Enemy/Spawn/EnemyPoolSubsystem.h"
#include "GameCore/Enemy/Spawn/SpawnPoint.h"
#include "GameCore/Movement/MonsterMovementComponent.h"
#include "GameCore/Tags/GameplayTagSystem.h"
#include "GameFramework/Controller.h"
#include "GameCore/AI/MonsterAIController.h"
#include "GameCore/BaseGameState.h"
#include "PCGMap/MapBuild/SectorBuild/Event/PlacedEvent/BattleSector/ConstBattleSector.h"

UEnemySplitComponent::UEnemySplitComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UEnemySplitComponent::ResetSplit()
{
	LeafBudget = 0;
	bSplitChild = false;
	bSplitUsed = false;
	ReadyTime = -1.;
}

void UEnemySplitComponent::StartSplitDelay()
{
	InitializeSharedHealth();
	ReadyTime = GetWorld()->GetTimeSeconds() + FMath::Max(0.1f, SplitDelay);
}

void UEnemySplitComponent::InitializeChild(const UEnemySplitComponent* Parent, int32 InLeafBudget)
{
	bShareHealth = Parent->bShareHealth;
	MaxSplitCount = Parent->MaxSplitCount;
	SplitDelay = Parent->SplitDelay;
	SplitRatio = Parent->SplitRatio;
	SplitOffset = Parent->SplitOffset;
	SplitSearchRadius = Parent->SplitSearchRadius;
	SplitSearchRings = Parent->SplitSearchRings;
	SplitSearchHeight = Parent->SplitSearchHeight;
	LeafBudget = InLeafBudget;
	bSplitChild = true;
	bSplitUsed = false;
	ReadyTime = GetWorld()->GetTimeSeconds() + FMath::Max(0.1f, SplitDelay);
}

bool UEnemySplitComponent::IsSplitReady() const
{
	const auto* Character = Cast<ACommonEnemyCharacter>(GetOwner());
	const UAbilitySystemComponent* ASC = Character ? Character->GetAbilitySystemComponent() : nullptr;
	if (!Character || !Character->HasAuthority() || Character->HasStartedDeath() || bSplitUsed ||
	    (LeafBudget == 0 ? MaxSplitCount : LeafBudget) < 2 || !ASC ||
	    !ASC->HasMatchingGameplayTag(GameplayTagSystem::Enemy_Trait_Split) ||
	    !ASC->HasMatchingGameplayTag(GameplayTagSystem::State_AI_Encountered))
	{
		return false;
	}
	// 풀 사전 생성 중이 아니라 최초 전투 패턴 선택 시점부터 분열 대기 시간을 계산합니다.
	if (ReadyTime < 0.)
	{
		ReadyTime = GetWorld()->GetTimeSeconds() + FMath::Max(0.1f, SplitDelay);
	}
	return GetWorld()->GetTimeSeconds() >= ReadyTime;
}

bool UEnemySplitComponent::TrySplit()
{
	auto* Parent = Cast<ACommonEnemyCharacter>(GetOwner());
	if (!IsSplitReady() || !Parent)
	{
		return false;
	}
	UWorld* World = GetWorld();
	auto* Pool = World->GetSubsystem<UEnemyPoolSubsystem>();
	if (!Pool)
	{
		UE_LOG(LogTemp, Warning, TEXT("[EnemySplit] Failed | Parent=%s | Reason=PoolSubsystemMissing"),
		       *GetNameSafe(Parent));
		ReadyTime = World->GetTimeSeconds() + 1.;
		return false;
	}
	UAbilitySystemComponent* ASC = Parent->GetAbilitySystemComponent();
	const int32 Budget = LeafBudget == 0 ? FMath::Max(1, MaxSplitCount) : LeafBudget;
	const float Ratio = FMath::Clamp(SplitRatio, 0.01f, 1.f);
	const float HealthRatio = IsValid(Parent->GetSharedHealthPool()) ? 1.f : Ratio;
	const float Health = ASC->GetNumericAttribute(UCharacterAttributeSet::GetHealthAttribute()) * HealthRatio;
	const FVector Scale = Parent->GetActorScale3D() * Ratio;
	const UCapsuleComponent* Capsule = Parent->GetCapsuleComponent();
	const float Radius = Capsule->GetScaledCapsuleRadius() * Ratio;
	const float HalfHeight = Capsule->GetScaledCapsuleHalfHeight() * Ratio;
	// 크기가 줄어들어도 자식 캡슐의 바닥 높이는 부모와 일치시킵니다.
	const FVector Center =
	    Parent->GetActorLocation() - FVector(0, 0, Capsule->GetScaledCapsuleHalfHeight() - HalfHeight);
	const float Offset = FMath::Max(SplitOffset, Radius + 2.f);
	FVector Locations[2];
	TArray<FVector> ValidLocations;
	const bool bGround = !Parent->GetCharacterMovement()->IsFlying();
	const float SearchHeight = FMath::Max(1.f, SplitSearchHeight);
	const float PairSpacing = 2.f * Radius + 4.f;
	FCollisionQueryParams Query(SCENE_QUERY_STAT(EnemySplit), false, Parent);
	auto TryCandidate = [&](FVector Location)
	{
		if (bGround)
		{
			FHitResult Floor;
			const FVector Bottom = Location - FVector(0, 0, HalfHeight);
			if (!World->LineTraceSingleByChannel(Floor, Bottom + FVector(0, 0, SearchHeight),
			                                     Bottom - FVector(0, 0, SearchHeight), ECC_Visibility, Query) ||
			    Floor.ImpactNormal.Z < Parent->GetCharacterMovement()->GetWalkableFloorZ())
			{
				return false;
			}
			Location.Z = Floor.ImpactPoint.Z + HalfHeight + 2.f;
		}
		if (World->OverlapBlockingTestByChannel(Location, Parent->GetActorQuat(), Capsule->GetCollisionObjectType(),
		                                        FCollisionShape::MakeCapsule(Radius, HalfHeight), Query))
		{
			return false;
		}
		// 부모와 후보 위치 사이에 벽이 있으면 해당 후보를 제외합니다.
		FHitResult Obstacle;
		if (World->LineTraceSingleByChannel(Obstacle, Parent->GetActorLocation(), Location, ECC_Visibility, Query))
		{
			return false;
		}
		for (const FVector& Existing : ValidLocations)
		{
			if (FVector::DistSquared2D(Existing, Location) >= FMath::Square(PairSpacing))
			{
				Locations[0] = Existing;
				Locations[1] = Location;
				return true;
			}
		}
		ValidLocations.Add(Location);
		return false;
	};
	// 양옆 위치를 우선 검사하고, 실패하면 제한된 반경 안에서 주변 후보를 탐색합니다.
	const FVector Side = Parent->GetActorRightVector() * Offset;
	bool bFoundPair = TryCandidate(Center - Side);
	if (!bFoundPair)
	{
		bFoundPair = TryCandidate(Center + Side);
	}
	const int32 Rings = FMath::Clamp(SplitSearchRings, 1, 8);
	const float MaxRadius = FMath::Max(Offset, SplitSearchRadius);
	constexpr int32 Directions = 16;
	for (int32 Ring = 0; Ring < Rings && !bFoundPair; ++Ring)
	{
		const float Distance =
		    FMath::Lerp(Offset, MaxRadius, Rings == 1 ? 1.f : static_cast<float>(Ring) / (Rings - 1));
		for (int32 Direction = 0; Direction < Directions && !bFoundPair; ++Direction)
		{
			const float Angle = 2.f * PI * Direction / Directions;
			const FVector Delta =
			    Parent->GetActorRightVector() * FMath::Cos(Angle) + Parent->GetActorForwardVector() * FMath::Sin(Angle);
			bFoundPair = TryCandidate(Center + Delta * Distance);
		}
	}
	if (!bFoundPair)
	{
		ReadyTime = World->GetTimeSeconds() + 1.;
		return false;
	}

	ACommonEnemyCharacter* Children[2] = {nullptr, nullptr};
	auto Rollback = [&]()
	{
		for (auto* Child : Children)
		{
			if (IsValid(Child))
			{
				Pool->Release(Child);
			}
		}
		ReadyTime = World->GetTimeSeconds() + 1.;
	};
	const int32 Budgets[2] = {Budget / 2, Budget - Budget / 2};
	for (int32 Index = 0; Index < 2; ++Index)
	{
		const FTransform Transform(Parent->GetActorRotation(), Locations[Index], Scale);
		Children[Index] = Pool->AcquireForSplit(Parent, Transform);
		auto* Child = Children[Index];
		if (!Child || !Child->EnemySplitComponent)
		{
			Rollback();
			return false;
		}
		// 분열용 풀 획득은 최초 생성과 재사용 초기화 모두에서 무작위 특성을 차단합니다.
		Child->SetUseMonsterStatUI(Parent->ShouldUseMonsterStatUI());
		Child->bTrackAllPlayersDuringEncounter = Parent->bTrackAllPlayersDuringEncounter;
		Child->EnemySplitComponent->InitializeChild(this, Budgets[Index]);
		Child->EnemyDefinition = Parent->EnemyDefinition;
		Child->InitializeEnemyDefinition();
		Child->StartMode = EEnemyStartMode::Dormant;
		if (!Child->GetController())
		{
			Child->SpawnDefaultController();
		}
		if (Child->bTrackAllPlayersDuringEncounter)
		{
			if (auto* AC = Cast<AMonsterAIController>(Child->GetController()))
			{
				AC->SetTrackAllPlayers(true);
			}
		}
		auto* ChildASC = Child->GetAbilitySystemComponent();
		if (!ChildASC || !Child->GetController())
		{
			Rollback();
			return false;
		}
		Child->SplitScale = Scale;
		Child->OnRep_SplitScale();
		if (!Child->InheritSplitPresentation(Parent, Budgets[Index] > 1))
		{
			Rollback();
			return false;
		}
		// 특성 GE를 복제한 뒤 부모의 기본 체력을 사용해 최대 체력 배율의 중복 적용을 방지합니다.
		const float ParentBaseMax = ASC->GetNumericAttributeBase(UCharacterAttributeSet::GetMaxHealthAttribute());
		ChildASC->SetNumericAttributeBase(UCharacterAttributeSet::GetMaxHealthAttribute(), ParentBaseMax * HealthRatio);
		const float ChildMax = ChildASC->GetNumericAttribute(UCharacterAttributeSet::GetMaxHealthAttribute());
		ChildASC->SetNumericAttributeBase(UCharacterAttributeSet::GetHealthAttribute(), FMath::Min(Health, ChildMax));
	}
	// 자식 등록과 부모 교체 사이에는 사망 이벤트나 클리어 판정을 실행하지 않습니다.
	Parent->OnSplitCommitted.Broadcast(Parent, Children[0], Children[1]);
	for (TActorIterator<AConstBattleSector> It(World); It; ++It)
	{
		if (It->TracksMonster(Parent))
		{
			It->ReplaceTrackedMonster(Parent, Children[0], Children[1]);
		}
	}
	for (TActorIterator<ASpawnPoint> It(World); It; ++It)
	{
		if (It->TracksMonster(Parent))
		{
			It->ReplaceTrackedMonster(Parent, Children[0], Children[1]);
		}
	}
	for (auto* Child : Children)
	{
		Child->StartPatrol(Parent->GetPatrolArea());
		Child->ForceNetUpdate();
	}
	bSplitUsed = true;
	Parent->SetActorEnableCollision(false);
	Parent->SetActorHiddenInGame(true);
	// 어빌리티 종료와 상태 트리 콜백이 완료된 뒤 부모를 풀에 반환합니다.
	const TWeakObjectPtr<ACommonEnemyCharacter> WeakParent = Parent;
	World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(
	    Parent,
	    [WeakParent]()
	    {
		    if (auto* Actor = WeakParent.Get())
		    {
			    auto* PoolSubsystem = Actor->GetWorld()->GetSubsystem<UEnemyPoolSubsystem>();
			    if (PoolSubsystem && PoolSubsystem->IsActive(Actor))
			    {
				    PoolSubsystem->Release(Actor);
			    }
			    else
			    {
				    AController* Controller = Actor->GetController();
				    Actor->Destroy();
				    if (Controller)
				    {
					    Controller->Destroy();
				    }
			    }
		    }
	    }));
	return true;
}

void UEnemySplitComponent::InitializeSharedHealth()
{
	auto* Enemy = Cast<ACommonEnemyCharacter>(GetOwner());
	if (!Enemy || !Enemy->HasAuthority() || !bShareHealth || IsValid(Enemy->GetEnemySplitComponent()->SharedHealthPool))
	{
		return;
	}
	auto* ASC = Enemy->GetAbilitySystemComponent();
	if (!ASC || !ASC->HasMatchingGameplayTag(GameplayTagSystem::Enemy_Trait_Split))
	{
		return;
	}
	auto* State = NewObject<UEnemySplitHealthState>(GetWorld());
	State->Initialize(Enemy);
}

UWorld* UEnemySplitHealthState::GetWorld() const
{
	return Cast<UWorld>(GetOuter());
}

void UEnemySplitHealthState::Initialize(ACommonEnemyCharacter* Root)
{
	if (!IsValid(Root))
	{
		return;
	}
	if (auto* ASC = Root->GetAbilitySystemComponent())
	{
		Health = ASC->GetNumericAttribute(UCharacterAttributeSet::GetHealthAttribute());
		MaxHealth = ASC->GetNumericAttribute(UCharacterAttributeSet::GetMaxHealthAttribute());
	}
	AddMember(Root);
}

void UEnemySplitHealthState::AddMember(ACommonEnemyCharacter* Enemy)
{
	if (!IsValid(Enemy) || Members.Contains(Enemy))
	{
		return;
	}
	Members.Add(Enemy);
	Enemy->GetEnemySplitComponent()->SharedHealthPool = this;
	Enemy->OnSplitCommitted.AddUObject(this, &ThisClass::OnSplit);
	Enemy->OnDeathStarted.AddUObject(this, &ThisClass::OnDeathStarted);
	Enemy->ForceNetUpdate();
}

void UEnemySplitHealthState::RemoveMember(ACommonEnemyCharacter* Enemy)
{
	if (!Enemy)
	{
		return;
	}
	Members.Remove(Enemy);
	Enemy->OnSplitCommitted.RemoveAll(this);
	Enemy->OnDeathStarted.RemoveAll(this);
	if (Enemy->GetEnemySplitComponent()->SharedHealthPool == this)
	{
		Enemy->GetEnemySplitComponent()->SharedHealthPool = nullptr;
	}
	Members.RemoveAll([](const auto& Member) { return !Member.IsValid(); });
	if (auto* World = GetWorld())
	{
		if (auto* GS = World->GetGameState<ABaseGameState>())
		{
			GS->RefreshSharedEncounterHealth(this);
		}
	}
}

void UEnemySplitHealthState::OnSplit(ACommonEnemyCharacter* Parent, ACommonEnemyCharacter* Left,
                                     ACommonEnemyCharacter* Right)
{
	if (!Members.Contains(Parent))
	{
		return;
	}
	AddMember(Left);
	AddMember(Right);
	RemoveMember(Parent);
	SyncHealth();
}

void UEnemySplitHealthState::SyncHealth()
{
	for (const auto& Member : Members)
	{
		if (auto* Enemy = Member.Get())
		{
			if (auto* ASC = Enemy->GetAbilitySystemComponent())
			{
				if (!FMath::IsNearlyEqual(ASC->GetNumericAttribute(UCharacterAttributeSet::GetMaxHealthAttribute()),
				                          MaxHealth))
				{
					ASC->SetNumericAttributeBase(UCharacterAttributeSet::GetMaxHealthAttribute(), MaxHealth);
				}
				if (!FMath::IsNearlyEqual(ASC->GetNumericAttribute(UCharacterAttributeSet::GetHealthAttribute()),
				                          Health))
				{
					ASC->SetNumericAttributeBase(UCharacterAttributeSet::GetHealthAttribute(), Health);
				}
			}
		}
	}
	if (auto* World = GetWorld())
	{
		if (auto* GS = World->GetGameState<ABaseGameState>())
		{
			GS->RefreshSharedEncounterHealth(this);
		}
	}
}

float UEnemySplitHealthState::ApplySharedDamage(float Damage)
{
	Health = FMath::Clamp(Health - FMath::Max(0.f, Damage), 0.f, MaxHealth);
	SyncHealth();
	return Health;
}

void UEnemySplitHealthState::UpdateSharedVitals(float InHealth, float InMaxHealth)
{
	MaxHealth = FMath::Max(1.f, InMaxHealth);
	Health = FMath::Clamp(InHealth, 0.f, MaxHealth);
	SyncHealth();
}

void UEnemySplitHealthState::FinishSharedDeath(AActor* HitEnemy, AActor* InstigatorActor,
                                               const FGameplayEffectContextHandle& Context)
{
	if (Health > 0.f)
	{
		return;
	}
	const auto Snapshot = Members;
	for (const auto& Member : Snapshot)
	{
		auto* Enemy = Member.Get();
		if (!Enemy || Enemy == HitEnemy || Enemy->HasStartedDeath())
		{
			continue;
		}
		if (auto* ASC = Enemy->GetAbilitySystemComponent())
		{
			FGameplayEventData Payload;
			Payload.EventTag = GameplayTagSystem::Event_Death;
			Payload.Instigator = InstigatorActor;
			Payload.Target = Enemy;
			Payload.ContextHandle = Context;
			ASC->HandleGameplayEvent(Payload.EventTag, &Payload);
		}
	}
}

void UEnemySplitHealthState::OnDeathStarted(ABaseCharacter* Character)
{
	auto* Enemy = Cast<ACommonEnemyCharacter>(Character);
	if (!Enemy)
	{
		return;
	}
	const bool bPropagate = Health > 0.f;
	if (bPropagate)
	{
		Health = 0.f;
		SyncHealth();
	}
	// 생존한 형제들이 같은 체력 상태를 공유하므로 풀에 반환할 때까지 참조를 유지합니다.
	Members.Remove(Enemy);
	Enemy->OnSplitCommitted.RemoveAll(this);
	Enemy->OnDeathStarted.RemoveAll(this);
	if (bPropagate)
	{
		FinishSharedDeath(Enemy, nullptr, FGameplayEffectContextHandle());
	}
	if (auto* World = GetWorld())
	{
		if (auto* GS = World->GetGameState<ABaseGameState>())
		{
			GS->RefreshSharedEncounterHealth(this);
		}
	}
}

void UEnemySplitComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	if (IsValid(SharedHealthPool))
	{
		SharedHealthPool->RemoveMember(Cast<ACommonEnemyCharacter>(GetOwner()));
	}
	SharedHealthPool = nullptr;
	Super::EndPlay(Reason);
}

FText UEnemySplitHealthState::GetTraitDisplayName() const
{
	TArray<FString> Names;
	for (const auto& Member : Members)
	{
		if (const auto* Enemy = Member.Get())
		{
			for (const FText& Name : Enemy->GetActiveTraitNames())
			{
				if (!Name.IsEmpty())
				{
					Names.AddUnique(Name.ToString());
				}
			}
		}
	}
	return FText::FromString(FString::Join(Names, TEXT(" · ")));
}
