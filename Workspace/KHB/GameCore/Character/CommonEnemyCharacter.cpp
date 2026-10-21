#include "CommonEnemyCharacter.h"
#include "Serialization/CustomVersion.h"
#include "GameCore/BaseGameState.h"
#include "TimerManager.h"
#include "GameCore/Combat/EnemySplitComponent.h"

#include "AbilitySystemComponent.h"
#include "Abilities/GameplayAbility.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameplayEffect.h"
#include "GameCore/AI/CommonEnemyAIController.h"
#include "GameCore/Attribute/CharacterAttributeSet.h"
#include "GameCore/Combat/EnemyPatternComponent.h"
#include "GameCore/Combat/EnemyBeamAimReplicationComponent.h"
#include "GameCore/Enemy/Data/EnemyDataAssets.h"
#include "GameCore/Movement/EnemyMovementProfileReceiver.h"
#include "GameCore/Tags/GameplayTagSystem.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"

ACommonEnemyCharacter::ACommonEnemyCharacter(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	EnemySplitComponent = CreateDefaultSubobject<UEnemySplitComponent>(TEXT("EnemySplitComponent"));
	EnemyPatternComponent = CreateDefaultSubobject<UEnemyPatternComponent>(TEXT("EnemyPatternComponent"));
	EnemyBeamAimReplicationComponent =
	    CreateDefaultSubobject<UEnemyBeamAimReplicationComponent>(TEXT("EnemyBeamAimReplicationComponent"));
}

FText ACommonEnemyCharacter::GetEnemyDisplayName() const
{
	return IsValid(EnemyDefinition) ? EnemyDefinition->DisplayName : FText::GetEmpty();
}

void ACommonEnemyCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ACommonEnemyCharacter, SplitScale);
	DOREPLIFETIME(ACommonEnemyCharacter, ActiveTraitNames);
	DOREPLIFETIME(ACommonEnemyCharacter, ActiveTraitDisplayName);
	DOREPLIFETIME(ACommonEnemyCharacter, ActiveTraitOverlayMaterial);
	DOREPLIFETIME(ACommonEnemyCharacter, ActiveTraitOverlayColor);
}

UEnemyMovementProfileDataAsset* ACommonEnemyCharacter::GetActiveMovementProfile() const
{
	if (IsValid(ActiveMovementProfile))
	{
		return ActiveMovementProfile;
	}

	return IsValid(EnemyDefinition) ? EnemyDefinition->MovementProfile : nullptr;
}

void ACommonEnemyCharacter::ApplyMovementProfile(UEnemyMovementProfileDataAsset* InMovementProfile)
{
	// 오버라이드가 비어 있으면 Definition의 기본 프로필로 되돌린다.
	if (!IsValid(InMovementProfile))
	{
		InMovementProfile = IsValid(EnemyDefinition) ? EnemyDefinition->MovementProfile : nullptr;
	}

	ActiveMovementProfile = InMovementProfile;
	if (!IsValid(ActiveMovementProfile))
	{
		UE_LOG(LogTemp, Warning,
		       TEXT("[EnemyMovement] MovementProfile 이 없습니다. 이동 컴포넌트 기본값으로 동작합니다. | Owner=%s"),
		       *GetNameSafe(this));
	}

	// 캐릭터는 어떤 이동 컴포넌트인지 모른다. 프로필을 받을 줄 아는 컴포넌트에만 알린다.
	if (IEnemyMovementProfileReceiver* Receiver = Cast<IEnemyMovementProfileReceiver>(GetCharacterMovement()))
	{
		Receiver->HandleMovementProfileChanged(ActiveMovementProfile);
	}
}

void ACommonEnemyCharacter::ApplyDefinitionInitialStats()
{
	/*
	 * BaseCharacter의 기존 InitStatsRow 초기화는 그대로 끝난 뒤 호출된다.
	 * 새 Enemy Framework 적만 Definition DA의 행을 추가로 적용해 최종값을 DA로 맞춘다.
	 */
	if (!HasAuthority())
	{
		return;
	}

	if (IsValid(EnemyDefinition) && EnemyDefinition->StatTableRowHandle.DataTable)
	{
		// Definition은 기존 DT_MonsterInitState의 FMonsterInitStats 행을 가리킨다.
		// BP의 MonsterStatsRow가 먼저 적용된 뒤 이 값이 최종 스탯이 된다.
		ApplyMonsterInitialStatsFromRow(EnemyDefinition->StatTableRowHandle);
	}
}

void ACommonEnemyCharacter::BeginPlay()
{
	Super::BeginPlay();
	ApplyDefinitionInitialStats();
	ApplyInitialMultipliers();
	EnsureEnemyPatternComponent();
	InitializeEnemyDefinition();
	ApplyMovementProfile(IsValid(EnemyDefinition) ? EnemyDefinition->MovementProfile.Get() : nullptr);
	// Definition의 기본 스탯과 GA 구성이 끝난 뒤에 적용해야 Trait GE가 초기화 값에 덮이지 않는다.
	ApplyRandomEnemyTrait();
	if (auto* ASC = GetAbilitySystemComponent())
	{
		ASC->RegisterGameplayTagEvent(GameplayTagSystem::State_AI_Encountered, EGameplayTagEventType::NewOrRemoved)
		    .AddUObject(this, &ACommonEnemyCharacter::OnEncounteredTagChanged);
	}

	if (const ACommonEnemyAIController* AICon = Cast<ACommonEnemyAIController>(GetController());
	    IsValid(AICon) && AICon->IsStationaryDamageTestEnabled())
	{
		StartStationaryDamageTest();
	}
	else
	{
		ApplyStartMode();
	}
}

void ACommonEnemyCharacter::StartStationaryDamageTest()
{
	if (HasAuthority())
	{
		if (UAbilitySystemComponent* ASC = GetAbilitySystemComponent())
		{
			ASC->RemoveLooseGameplayTag(GameplayTagSystem::State_AI_Dormant);
		}
	}

	SetEncounterPresentationVisibleForFrameworkTest(true);
	if (ACommonEnemyAIController* AICon = Cast<ACommonEnemyAIController>(GetController()))
	{
		AICon->StopAIForStationaryDamageTest();
	}
}

void ACommonEnemyCharacter::HandleDamageHitReact(float Damage, AActor* InstigatorActor,
                                                 const FGameplayEffectContextHandle& EffectContext)
{
	if (const ACommonEnemyAIController* AICon = Cast<ACommonEnemyAIController>(GetController());
	    IsValid(AICon) && AICon->IsStationaryDamageTestEnabled())
	{
		return;
	}

	Super::HandleDamageHitReact(Damage, InstigatorActor, EffectContext);
}

void ACommonEnemyCharacter::ApplyRandomEnemyTrait()
{
	if (!HasAuthority() || bSkipRandomTraitForAcquire || (EnemySplitComponent && EnemySplitComponent->IsSplitChild()) ||
	    !IsValid(TraitPool))
	{
		return;
	}

	const bool bForceTrait = TraitPool->Effects.IsValidIndex(ForcedTraitIndex);
	if (!bForceTrait && TraitPool->MutationChance <= 0.0f)
	{
		return;
	}

	UAbilitySystemComponent* ASC = GetAbilitySystemComponent();
	if (!IsValid(ASC) || (!bForceTrait && FMath::FRand() > TraitPool->MutationChance))
	{
		return;
	}

	float TotalWeight = 0.0f;
	for (const FEnemyTraitEffectDefinition& Entry : TraitPool->Effects)
	{
		if (!Entry.EffectClasses.IsEmpty())
		{
			TotalWeight += FMath::Max(Entry.Weight, 0.0f);
		}
	}

	if (!bForceTrait && TotalWeight <= 0.0f)
	{
		UE_LOG(LogTemp, Warning, TEXT("[EnemyTrait] RollSkipped | Owner=%s | Reason=NoWeightedEffect"),
		       *GetNameSafe(this));
		return;
	}

	const FEnemyTraitEffectDefinition* SelectedEntry = bForceTrait ? &TraitPool->Effects[ForcedTraitIndex] : nullptr;
	float Roll = FMath::FRandRange(0.0f, TotalWeight);
	for (const FEnemyTraitEffectDefinition& Entry : TraitPool->Effects)
	{
		if (bForceTrait)
		{
			break;
		}

		if (Entry.EffectClasses.IsEmpty())
		{
			continue;
		}

		Roll -= FMath::Max(Entry.Weight, 0.0f);
		if (Roll <= 0.0f)
		{
			SelectedEntry = &Entry;
			break;
		}
	}

	if (SelectedEntry == nullptr && !bForceTrait)
	{
		for (int32 Index = TraitPool->Effects.Num() - 1; Index >= 0; --Index)
		{
			if (!TraitPool->Effects[Index].EffectClasses.IsEmpty() && TraitPool->Effects[Index].Weight > 0.0f)
			{
				SelectedEntry = &TraitPool->Effects[Index];
				break;
			}
		}
	}

	if (SelectedEntry == nullptr)
	{
		return;
	}

	ApplyAdditionalTrait(*SelectedEntry);
}

void ACommonEnemyCharacter::SetTraitDisplayName(const FText& InDisplayName)
{
	ActiveTraitDisplayName = InDisplayName;
	OnTraitDisplayChanged.Broadcast();
}

void ACommonEnemyCharacter::OnRep_ActiveTraitDisplayName()
{
	UE_LOG(LogTemp, Log, TEXT("[EnemyTraitUI] Replicated | Owner=%s | Name=%s | NetMode=%d"), *GetNameSafe(this),
	       *ActiveTraitDisplayName.ToString(), static_cast<int32>(GetNetMode()));
	OnTraitDisplayChanged.Broadcast();
}

void ACommonEnemyCharacter::SetTraitOverlay(UMaterialInterface* InMaterial, const FLinearColor& InColor)
{
	ActiveTraitOverlayMaterial = InMaterial;
	ActiveTraitOverlayColor = InColor;
	ApplyTraitOverlay();
}

void ACommonEnemyCharacter::OnRep_ActiveTraitOverlay()
{
	ApplyTraitOverlay();
}

void ACommonEnemyCharacter::ApplyTraitOverlay()
{
	USkeletalMeshComponent* MeshComponent = GetMesh();
	if (!IsValid(MeshComponent))
	{
		return;
	}

	if (!IsValid(ActiveTraitOverlayMaterial))
	{
		ActiveTraitOverlayMID = nullptr;
		MeshComponent->SetOverlayMaterial(nullptr);
		return;
	}

	ActiveTraitOverlayMID = UMaterialInstanceDynamic::Create(ActiveTraitOverlayMaterial, this);
	if (!IsValid(ActiveTraitOverlayMID))
	{
		UE_LOG(LogTemp, Warning, TEXT("[EnemyTrait] OverlayCreateFailed | Owner=%s | Material=%s"), *GetNameSafe(this),
		       *GetNameSafe(ActiveTraitOverlayMaterial));
		return;
	}

	ActiveTraitOverlayMID->SetVectorParameterValue(TEXT("Color"), ActiveTraitOverlayColor);
	MeshComponent->SetOverlayMaterial(ActiveTraitOverlayMID);
}

void ACommonEnemyCharacter::InitializeSpawnPlacement(const TScriptInterface<IEnemyPatrolRoute>& InPatrolRoute,
                                                     const EEnemyStartMode InStartMode,
                                                     const int32 InPatrolStartPointIndex)
{
	PatrolRoute = InPatrolRoute;
	PatrolStartPointIndex = FMath::Max(InPatrolStartPointIndex, 0);
	StartMode = InStartMode;

	// BeginPlay 이후에 불릴 수도 있으므로 여기서도 시작 방식을 반영한다.
	ApplyStartMode();

	UE_LOG(LogTemp, Warning, TEXT("[EnemySpawn] Placement | Monster=%s | StartMode=%s | Route=%s | StartPoint=%d"),
	       *GetNameSafe(this), StartMode == EEnemyStartMode::Dormant ? TEXT("Dormant") : TEXT("Visible"),
	       *GetNameSafe(PatrolRoute.GetObject()), PatrolStartPointIndex);
}

void ACommonEnemyCharacter::StartPatrol(const TScriptInterface<IPatrolArea>& Area)
{
	SetPatrolArea(Area);
	Super::StartPatrol(Area);
}

void ACommonEnemyCharacter::ResetForReuse()
{
	bTrackAllPlayersDuringEncounter =
	    GetClass()->GetDefaultObject<ACommonEnemyCharacter>()->bTrackAllPlayersDuringEncounter;
	if (auto* AC = Cast<AMonsterAIController>(GetController()))
	{
		AC->RestoreDefaultTargetTracking();
	}
	if (IsValid(GetSharedHealthPool()))
	{
		GetSharedHealthPool()->RemoveMember(this);
	}
	EnemySplitComponent->SharedHealthPool = nullptr;
	GetWorldTimerManager().ClearTimer(PlacedIntroRetryTimer);
	bPlacedEncounterFlow = false;
	bPlacedIntroPending = false;
	PlacedEncounterInstigator.Reset();
	ActiveTraitDefinitions.Reset();
	ActiveTraitNames.Reset();
	SplitTraitPresentation = FEnemyTraitEffectDefinition();
	EnemySplitComponent->ResetSplit();
	if (UAbilitySystemComponent* SplitASC = GetAbilitySystemComponent())
	{
		SplitASC->SetLooseGameplayTagCount(GameplayTagSystem::Enemy_Trait_Split, 0);
	}
	SplitScale = GetClass()->GetDefaultObject<ACommonEnemyCharacter>()->GetActorScale3D();
	SplitScale *= FMath::Max(0.01f, InitialScaleMultiplier);
	AppliedPreviewScaleMultiplier = FMath::Max(0.01f, InitialScaleMultiplier);
	OnRep_SplitScale();
	PatrolArea = nullptr;

	// 지난번 특성 GE를 먼저 떼어야 스탯을 되돌린 값 위에 새 특성이 한 번만 얹힌다
	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponent())
	{
		for (const FActiveGameplayEffectHandle& Handle : ActiveTraitEffectHandles)
		{
			ASC->RemoveActiveGameplayEffect(Handle);
		}
	}
	ActiveTraitEffectHandles.Reset();
	if (HasAuthority())
	{
		SetTraitDisplayName(FText::GetEmpty());
	}
	SetTraitOverlay(nullptr, FLinearColor::White);

	// BeginPlay와 같은 순서: 기본 스탯, Definition 스탯, 이동 프로필, 특성
	Super::ResetForReuse();
	ApplyDefinitionInitialStats();
	ApplyInitialMultipliers();
	ApplyMovementProfile(nullptr);
	ApplyRandomEnemyTrait();
}

bool ACommonEnemyCharacter::HasPatrolRoute() const
{
	return PatrolRoute.GetObject() != nullptr;
}

bool ACommonEnemyCharacter::HasPatrolArea() const
{
	return IsValid(PatrolArea.GetObject());
}

void ACommonEnemyCharacter::ApplyStartMode()
{
	if (StartMode != EEnemyStartMode::Visible || !HasAuthority())
	{
		// 휴면은 부모 BeginPlay가 이미 걸어 둔 상태가 정답이라 건드리지 않는다.
		return;
	}

	/*
	 * 처음부터 보이는 적은 조우를 기다리지 않는다. 휴면을 풀어 인지와 순찰을 곧바로 시작한다.
	 * Encountered는 "조우 연출을 마친 적"이라는 뜻이라, Emerge 없이 나타난 적도 같은 상태로 둔다.
	 */
	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponent())
	{
		ASC->RemoveLooseGameplayTag(GameplayTagSystem::State_AI_Dormant);
		ASC->AddLooseGameplayTag(GameplayTagSystem::State_AI_Encountered);
	}

	SetEncounterPresentationVisibleForFrameworkTest(true);
	if (ACommonEnemyAIController* AICon = Cast<ACommonEnemyAIController>(GetController()))
	{
		AICon->SetMovementLocked(false);
	}
}

void ACommonEnemyCharacter::EnsureEnemyPatternComponent()
{
	if (IsValid(EnemyPatternComponent))
	{
		return;
	}

	/*
	 * C++ 부모의 기본 서브오브젝트를 추가한 뒤 핫리로드하면, BP 인스턴스에는
	 * 컴포넌트가 남아 있는데 UPROPERTY 포인터만 None으로 풀리는 경우가 있다.
	 * 먼저 그 인스턴스를 찾아 재연결해야 패턴 선택 상태가 둘로 갈리지 않는다.
	 */
	EnemyPatternComponent = FindComponentByClass<UEnemyPatternComponent>();
	if (IsValid(EnemyPatternComponent))
	{
		UE_LOG(
		    LogTemp, Warning,
		    TEXT(
		        "[EnemyPattern] Rebound existing component after stale default-subobject reference | Owner=%s | Component=%s"),
		    *GetNameSafe(this), *GetNameSafe(EnemyPatternComponent));
		return;
	}

	// 기존 인스턴스조차 없을 때만 서버에 런타임 컴포넌트를 보충한다.
	if (!HasAuthority())
	{
		return;
	}

	EnemyPatternComponent = NewObject<UEnemyPatternComponent>(this, TEXT("EnemyPatternComponent_RuntimeRecovery"));
	if (!IsValid(EnemyPatternComponent))
	{
		UE_LOG(LogTemp, Error, TEXT("[EnemyPattern] Runtime component recovery failed | Owner=%s"), *GetNameSafe(this));
		return;
	}

	AddInstanceComponent(EnemyPatternComponent);
	EnemyPatternComponent->RegisterComponent();

	UE_LOG(LogTemp, Warning, TEXT("[EnemyPattern] Created runtime recovery component | Owner=%s | Component=%s"),
	       *GetNameSafe(this), *GetNameSafe(EnemyPatternComponent));
}

void ACommonEnemyCharacter::InitializeEnemyDefinition()
{
	if (!HasAuthority() || !IsValid(EnemyDefinition))
	{
		return;
	}

	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponent())
	{
		if (const UEnemyAbilitySetDataAsset* AbilitySet = EnemyDefinition->AbilitySet)
		{
			for (const FEnemyAbilityGrantDefinition& Grant : AbilitySet->Abilities)
			{
				if (!Grant.AbilityClass || ASC->FindAbilitySpecFromClass(Grant.AbilityClass) != nullptr)
				{
					continue;
				}

				ASC->GiveAbility(FGameplayAbilitySpec(Grant.AbilityClass, Grant.GrantLevel, INDEX_NONE, this));
			}
		}
	}

	// 일반 적은 Definition의 기본 PatternSet을 바로 사용한다.
	// 보스는 ABossMonsterCharacter가 Phase 1 PatternSet으로 즉시 덮어쓴다.
	if (IsValid(EnemyPatternComponent) && IsValid(EnemyDefinition->PatternSet))
	{
		EnemyPatternComponent->SetActivePatternSet(EnemyDefinition->PatternSet);
	}
}

void ACommonEnemyCharacter::OnRep_SplitScale()
{
	SetActorScale3D(SplitScale);
}

void ACommonEnemyCharacter::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	const FVector BaseScale = GetActorScale3D() / FMath::Max(0.01f, AppliedPreviewScaleMultiplier);
	AppliedPreviewScaleMultiplier = FMath::Max(0.01f, InitialScaleMultiplier);
	SetActorScale3D(BaseScale * AppliedPreviewScaleMultiplier);
}

void ACommonEnemyCharacter::ApplyInitialMultipliers()
{
	if (!HasAuthority())
	{
		return;
	}
	SplitScale = GetActorScale3D();
	if (bSkipRandomTraitForAcquire || (EnemySplitComponent && EnemySplitComponent->IsSplitChild()))
	{
		return;
	}
	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponent())
	{
		const float Health = ASC->GetNumericAttribute(UCharacterAttributeSet::GetMaxHealthAttribute()) *
		                     FMath::Max(0.01f, InitialHealthMultiplier);
		ASC->SetNumericAttributeBase(UCharacterAttributeSet::GetMaxHealthAttribute(), Health);
		ASC->SetNumericAttributeBase(UCharacterAttributeSet::GetHealthAttribute(), Health);
	}
}

bool ACommonEnemyCharacter::ApplyAdditionalTrait(const FEnemyTraitEffectDefinition& Trait)
{
	if (!HasAuthority() || (!bApplyingInheritedTraits && EnemySplitComponent && EnemySplitComponent->IsSplitChild()))
	{
		return false;
	}
	UAbilitySystemComponent* ASC = GetAbilitySystemComponent();
	if (!ASC || Trait.EffectClasses.IsEmpty())
	{
		return false;
	}
	for (const auto& Existing : ActiveTraitDefinitions)
	{
		if (Existing.EffectClasses == Trait.EffectClasses)
		{
			return true;
		}
	}
	const bool bHadSplit = ASC->HasMatchingGameplayTag(GameplayTagSystem::Enemy_Trait_Split);
	TArray<FActiveGameplayEffectHandle> AppliedHandles;
	auto Context = ASC->MakeEffectContext();
	Context.AddSourceObject(this);
	for (const auto& Effect : Trait.EffectClasses)
	{
		auto Spec = Effect ? ASC->MakeOutgoingSpec(Effect, 1.f, Context) : FGameplayEffectSpecHandle();
		const auto Handle =
		    Spec.IsValid() ? ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get()) : FActiveGameplayEffectHandle();
		if (!Handle.WasSuccessfullyApplied())
		{
			for (const auto& Applied : AppliedHandles)
			{
				ASC->RemoveActiveGameplayEffect(Applied);
			}
			UE_LOG(LogTemp, Warning, TEXT("[EnemyTrait] AdditionalTraitFailed | Owner=%s | Trait=%s"), *GetName(),
			       *Trait.DisplayName.ToString());
			return false;
		}
		AppliedHandles.Add(Handle);
	}
	ActiveTraitEffectHandles.Append(AppliedHandles);
	ActiveTraitDefinitions.Add(Trait);
	ActiveTraitNames.Add(Trait.DisplayName);
	TArray<FString> Names;
	for (const auto& Name : ActiveTraitNames)
	{
		if (!Name.IsEmpty())
		{
			Names.Add(Name.ToString());
		}
	}
	SetTraitDisplayName(FText::FromString(FString::Join(Names, TEXT(" · "))));
	const bool bHasSplit = ASC->HasMatchingGameplayTag(GameplayTagSystem::Enemy_Trait_Split);
	if (bHasSplit && !bHadSplit)
	{
		SplitTraitPresentation = Trait;
		if (!bApplyingInheritedTraits)
		{
			EnemySplitComponent->InitializeSharedHealth();
		}
	}
	UMaterialInterface* OverlayMaterial = nullptr;
	FLinearColor ColorSum(0.f, 0.f, 0.f, 0.f);
	int32 ColorCount = 0;
	for (const auto& ActiveTrait : ActiveTraitDefinitions)
	{
		if (!IsValid(ActiveTrait.OverlayMaterial))
		{
			continue;
		}
		// 모든 특성은 같은 오버레이 머티리얼을 사용하며, 첫 번째 유효한 머티리얼을 유지합니다.
		if (!OverlayMaterial)
		{
			OverlayMaterial = ActiveTrait.OverlayMaterial;
		}
		ColorSum += ActiveTrait.OverlayColor;
		++ColorCount;
	}
	SetTraitOverlay(OverlayMaterial, ColorCount > 0 ? ColorSum * (1.f / ColorCount) : FLinearColor::White);
	if (!bApplyingInheritedTraits && bHasSplit && !bHadSplit &&
	    ASC->HasMatchingGameplayTag(GameplayTagSystem::State_AI_Encountered))
	{
		EnemySplitComponent->StartSplitDelay();
	}
	if (auto* State = GetSharedHealthPool())
	{
		if (auto* GS = GetWorld()->GetGameState<ABaseGameState>())
		{
			GS->RefreshSharedEncounterHealth(State);
		}
	}
	ForceNetUpdate();
	UE_LOG(LogTemp, Log, TEXT("[EnemyTrait] Added | Owner=%s | Names=%s"), *GetName(),
	       *ActiveTraitDisplayName.ToString());
	return true;
}

bool ACommonEnemyCharacter::InheritSplitPresentation(const ACommonEnemyCharacter* Parent, bool bCanSplit)
{
	if (!HasAuthority() || !IsValid(Parent))
	{
		return false;
	}
	TGuardValue<bool> InheritanceGuard(bApplyingInheritedTraits, true);
	for (const auto& Trait : Parent->ActiveTraitDefinitions)
	{
		// 최종 개체에서는 분열 특성만 제외하고 기존 특성의 효과는 유지합니다.
		const bool bSplitTrait = Trait.EffectClasses == Parent->SplitTraitPresentation.EffectClasses;
		if (!bCanSplit && bSplitTrait)
		{
			continue;
		}
		if (!ApplyAdditionalTrait(Trait))
		{
			return false;
		}
	}
	SplitTraitPresentation = Parent->SplitTraitPresentation;
	if (bCanSplit)
	{
		// 활성 효과를 중복 계산하지 않고 부모의 혼합 외형을 그대로 이어받습니다.
		SetTraitOverlay(Parent->ActiveTraitOverlayMaterial, Parent->ActiveTraitOverlayColor);
	}
	else
	{
		SetTraitOverlay(nullptr, FLinearColor::White);
	}
	return true;
}

void ACommonEnemyCharacter::StartPlacedBattle()
{
	if (!HasAuthority() || HasStartedDeath())
	{
		return;
	}
	if (!GetController())
	{
		SpawnDefaultController();
	}
	if (auto* AC = Cast<AMonsterAIController>(GetController()))
	{
		if (bTrackAllPlayersDuringEncounter)
		{
			AC->SetTrackAllPlayers(true);
		}
		else
		{
			AC->RestoreDefaultTargetTracking();
		}
	}
	StartPatrol(PatrolArea);
	EnemySplitComponent->StartSplitDelay();
}

void ACommonEnemyCharacter::PreparePlacedEncounter()
{
	if (!HasAuthority() || HasStartedDeath())
	{
		return;
	}
	if (auto* ASC = GetAbilitySystemComponent())
	{
		ASC->SetLooseGameplayTagCount(GameplayTagSystem::State_AI_Encountered, 0);
		ASC->SetLooseGameplayTagCount(GameplayTagSystem::State_AI_Dormant, 1);
	}
	SetEncounterPresentationVisibleForFrameworkTest(GetEncounterEntryMode() != EEncounterEntryMode::Emerge);
	if (auto* AC = Cast<AMonsterAIController>(GetController()))
	{
		AC->SuspendAI(TEXT("Waiting for placed encounter"));
	}
}

void ACommonEnemyCharacter::OnEncounteredTagChanged(FGameplayTag Tag, int32 Count)
{
	if (HasAuthority() && Count > 0)
	{
		EnemySplitComponent->StartSplitDelay();
	}
}

bool ACommonEnemyCharacter::RequestEncounter_Implementation(AActor* InstigatorActor)
{
	if (!bPlacedEncounterFlow)
	{
		return Super::RequestEncounter_Implementation(InstigatorActor);
	}
	if (!HasAuthority() || HasStartedDeath() || bPlacedIntroPending)
	{
		return false;
	}
	PreparePlacedEncounter();
	bPlacedIntroPending = true;
	PlacedEncounterInstigator = InstigatorActor;
	if (GetEncounterEntryMode() != EEncounterEntryMode::Cinematic)
	{
		FinishEncounterIntro_Implementation();
	}
	return true;
}

void ACommonEnemyCharacter::FinishEncounterIntro_Implementation()
{
	if (!HasAuthority() || !bPlacedIntroPending)
	{
		return;
	}
	if (HasStartedDeath())
	{
		bPlacedIntroPending = false;
		GetWorldTimerManager().ClearTimer(PlacedIntroRetryTimer);
		return;
	}
	if (!GetController())
	{
		SpawnDefaultController();
	}
	auto* EnemyController = Cast<ACommonEnemyAIController>(GetController());
	AActor* Target = EnemyController ? (EnemyController->IsSelectableCombatPlayer(PlacedEncounterInstigator.Get())
	                                        ? PlacedEncounterInstigator.Get()
	                                        : EnemyController->FindNearestCombatPlayer())
	                                 : nullptr;
	if (!Target)
	{
		if (!GetWorldTimerManager().IsTimerActive(PlacedIntroRetryTimer))
		{
			GetWorldTimerManager().SetTimer(PlacedIntroRetryTimer, this,
			                                &ThisClass::FinishEncounterIntro_Implementation, 0.1f, true);
		}
		return;
	}
	GetWorldTimerManager().ClearTimer(PlacedIntroRetryTimer);
	bPlacedIntroPending = false;
	if (GetEncounterEntryMode() == EEncounterEntryMode::Emerge)
	{
		EnemyController->RestoreDefaultTargetTracking();
		if (bTrackAllPlayersDuringEncounter)
		{
			EnemyController->SetTrackAllPlayers(true);
		}
		ConfigureBattleEmerge(EncounterEmergeSettings.SpawnType, EncounterEmergeSettings.Montage,
		                      EncounterEmergeSettings.bUseTurnSection, EncounterEmergeSettings.bUseTurnRotationWarp);
		SetWaitForBattleCombatRelease(false);
		EnemyController->ResumeAI();
		// 일반 몬스터의 ST와 GA가 등장 완료, 무적 해제, 이동 잠금 해제를 처리합니다.
		Super::RequestEncounter_Implementation(Target);
		return;
	}
	StartPlacedBattle();
	EnemyController->RequestEncounter(Target);
	UE_LOG(LogTemp, Log, TEXT("[Encounter] CombatStarted | Enemy=%s | Target=%s"), *GetName(), *GetNameSafe(Target));
}

void ACommonEnemyCharacter::OnReleased()
{
	if (auto* AC = Cast<AMonsterAIController>(GetController()))
	{
		AC->RestoreDefaultTargetTracking();
	}
	if (IsValid(GetSharedHealthPool()))
	{
		GetSharedHealthPool()->RemoveMember(this);
	}
	EnemySplitComponent->SharedHealthPool = nullptr;
	Super::OnReleased();
}

UEnemySplitHealthState* ACommonEnemyCharacter::GetSharedHealthPool() const
{
	return EnemySplitComponent->SharedHealthPool;
}

EEncounterEntryMode ACommonEnemyCharacter::GetEncounterEntryMode() const
{
	return EncounterEntryMode;
}

namespace
{
const FGuid EncounterEntryVersionGuid(0x71C2AD04, 0xCA154E3E, 0xB07D9912, 0x83F246C9);
const FCustomVersionRegistration EncounterEntryVersion(EncounterEntryVersionGuid, 1, TEXT("EncounterEntryMode"));
}

void ACommonEnemyCharacter::Serialize(FArchive& Ar)
{
	Ar.UsingCustomVersion(EncounterEntryVersionGuid);
	const bool bNeedsMigration = Ar.IsLoading() && Ar.IsPersistent() && Ar.CustomVer(EncounterEntryVersionGuid) < 1;
	Super::Serialize(Ar);
	if (bNeedsMigration)
	{
		// 저장된 개체의 기존 값으로 이전하므로 BP 기본값과 레벨 개체의 오버라이드를 모두 보존합니다.
		EncounterEntryMode = bUseEncounterIntro ? EEncounterEntryMode::Cinematic : EEncounterEntryMode::Immediate;
	}
}
