#include "BossMonsterCharacter.h"

#include "AbilitySystemComponent.h"
#include "Components/CapsuleComponent.h"
#include "Curves/CurveFloat.h"
#include "EngineUtils.h"
#include "GameCore/AI/BossEnemyAIController.h"
#include "GameCore/AI/CommonEnemyAIController.h"
#include "GameCore/Attribute/CharacterAttributeSet.h"
#include "GameCore/Combat/BossPhaseComponent.h"
#include "GameCore/Combat/EnemyPatternComponent.h"
#include "GameCore/Enemy/Data/EnemyDataAssets.h"
#include "GameCore/Movement/BossMovementComponent.h"
#include "GameCore/Tags/GameplayTagSystem.h"
#include "GameFramework/Character.h"
#include "TimerManager.h"

ABossMonsterCharacter::ABossMonsterCharacter(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer.SetDefaultSubobjectClass<UBossMovementComponent>(
          ACharacter::CharacterMovementComponentName))
{
	bTrackAllPlayersDuringEncounter = true;
	CharacterRoleTag = GameplayTagSystem::Character_Enemy_Boss;
	BossPhaseComponent = CreateDefaultSubobject<UBossPhaseComponent>(TEXT("BossPhaseComponent"));
}

void ABossMonsterCharacter::BeginPlay()
{
	Super::BeginPlay();
	CacheBaseFormState();

	/*
	 * 아레나 중심은 페이즈 여부와 무관하게 필요하다.
	 *
	 * 이동 컴포넌트가 앵커를 여기서 읽으므로, 페이즈 초기화에 실패해도 먼저 찾아 둔다.
	 * Definition의 기본 이동 프로필은 부모 BeginPlay에서 이미 적용됐다.
	 */
	ResolveArenaCenter();

	UBossEnemyDefinitionDataAsset* BossDefinition = GetBossDefinition();
	if (!IsValid(BossDefinition) || !IsValid(BossPhaseComponent))
	{
		return;
	}

	// Definition은 콘텐츠 에셋이므로 서버/클라이언트 모두 같은 PhaseSet을 읽을 수 있다.
	// 현재 인덱스와 전환 상태만 서버에서 복제한다.
	BossPhaseComponent->OnPhaseChanged.AddUniqueDynamic(this, &ThisClass::HandleBossPhaseChanged);
	if (!BossPhaseComponent->InitializePhaseSet(BossDefinition->BossPhaseSet))
	{
		UE_LOG(LogTemp, Warning, TEXT("[BossPhase] Initialization failed | Boss=%s | Definition=%s"),
		       *GetNameSafe(this), *GetNameSafe(BossDefinition));
		return;
	}

	if (!HasAuthority())
	{
		return;
	}

	if (UEnemyPatternComponent* PatternComponent = GetEnemyPatternComponent())
	{
		PatternComponent->SetActivePatternSet(BossPhaseComponent->GetCurrentPatternSet());
	}

	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponent())
	{
		ASC->GetGameplayAttributeValueChangeDelegate(UCharacterAttributeSet::GetHealthAttribute())
		    .AddUObject(this, &ABossMonsterCharacter::HandleBossHealthChanged);
	}

	/*
	 * 보스 Controller는 휴면 중 패턴이 실행되는 것을 막기 위해 StateTree 자동 시작을 끈다.
	 * 따라서 Common의 Visible StartMode가 휴면·표시·이동 잠금을 해제하더라도,
	 * 여기서 StartBattle을 호출하지 않으면 StateTree Status가 Unset인 채로 남는다.
	 * 페이즈 초기화와 Pattern Set 적용이 끝난 뒤에 시작해야 첫 틱부터 P1 데이터를 사용한다.
	 */
	if (StartMode == EEnemyStartMode::Visible)
	{
		const ABossEnemyAIController* BossController = Cast<ABossEnemyAIController>(GetController());
		if (IsValid(BossController) && !BossController->IsStationaryDamageTestEnabled())
		{
			TryStartVisibleBattle();
		}
	}
}

void ABossMonsterCharacter::TryStartVisibleBattle()
{
	ABossEnemyAIController* BossController = Cast<ABossEnemyAIController>(GetController());
	const UAbilitySystemComponent* ASC = GetAbilitySystemComponent();
	if (!IsValid(BossController) || BossController->HasBattleStarted() ||
	    (IsValid(ASC) && ASC->HasMatchingGameplayTag(GameplayTagSystem::State_Dead)))
	{
		GetWorldTimerManager().ClearTimer(EncounterStartRetryTimerHandle);
		return;
	}

	/*
	 * BeginPlay 시점에는 플레이어 폰이 아직 없거나, 있어도 ASC 초기화 전이라
	 * Character.Player 태그가 없어 후보에서 빠질 수 있다. 한 번에 포기하면 타깃 없이
	 * 전투가 시작되고 이후엔 시야에만 의존하므로, 찾을 때까지 다시 시도한다.
	 */
	AActor* InitialTarget = BossController->FindNearestCombatPlayer();
	if (!IsValid(InitialTarget))
	{
		if (!GetWorldTimerManager().IsTimerActive(EncounterStartRetryTimerHandle))
		{
			GetWorldTimerManager().SetTimer(EncounterStartRetryTimerHandle, this, &ThisClass::TryStartVisibleBattle,
			                                EncounterStartRetryInterval, true);
		}
		return;
	}

	GetWorldTimerManager().ClearTimer(EncounterStartRetryTimerHandle);
	BossController->RequestEncounter(InitialTarget);
	BossController->StartBattle();

	UE_LOG(LogTemp, Warning,
	       TEXT("[BossEncounter] VisibleCombatStarted | Boss=%s | InitialTarget=%s | SelectedTarget=%s"),
	       *GetNameSafe(this), *GetNameSafe(InitialTarget), *GetNameSafe(BossController->GetPerceivedTarget()));
}

UBossEnemyDefinitionDataAsset* ABossMonsterCharacter::GetBossDefinition() const
{
	return Cast<UBossEnemyDefinitionDataAsset>(GetEnemyDefinition());
}

bool ABossMonsterCharacter::ShouldFaceArenaCenter() const
{
	const FEnemyBossPhaseDefinition* PhaseDefinition =
	    IsValid(BossPhaseComponent) ? BossPhaseComponent->GetCurrentPhaseDefinition() : nullptr;
	return PhaseDefinition && PhaseDefinition->FacingMode == EEnemyBossFacingMode::ArenaCenter;
}

bool ABossMonsterCharacter::RequestEncounter_Implementation(AActor* EncounterInstigator)
{
	const UAbilitySystemComponent* ASC = GetAbilitySystemComponent();
	if (!HasAuthority() || bEncounterIntroPending || !IsValid(ASC) ||
	    !ASC->HasMatchingGameplayTag(GameplayTagSystem::State_AI_Dormant) ||
	    ASC->HasMatchingGameplayTag(GameplayTagSystem::State_Dead))
	{
		UE_LOG(LogTemp, Warning, TEXT("[BossEncounter] Rejected | Boss=%s | IntroPending=%s | Dormant=%s"),
		       *GetNameSafe(this), bEncounterIntroPending ? TEXT("True") : TEXT("False"),
		       IsValid(ASC) && ASC->HasMatchingGameplayTag(GameplayTagSystem::State_AI_Dormant) ? TEXT("True")
		                                                                                        : TEXT("False"));
		return false;
	}

	/*
	 * 일반 몬스터는 이 시점에 곧바로 전투를 시작한다. 보스는 휴면을 유지한 채
	 * 등장 연출만 요청하고, 연출이 끝나면 FinishEncounterIntro에서 전투를 시작한다.
	 */
	bEncounterIntroPending = true;
	PendingEncounterInstigator = EncounterInstigator;
	UE_LOG(LogTemp, Warning, TEXT("[BossEncounter] IntroRequested | Boss=%s | Instigator=%s | SkipIntro=%s"),
	       *GetNameSafe(this), *GetNameSafe(EncounterInstigator), bSkipEncounterIntro ? TEXT("True") : TEXT("False"));
	OnEncounterIntroRequested.Broadcast(EncounterInstigator);

	// 시네마틱이 아직 없으면 연출이 곧바로 끝난 것으로 본다.
	if (bSkipEncounterIntro)
	{
		IEncounterIntroTarget::Execute_FinishEncounterIntro(this);
	}

	return true;
}

void ABossMonsterCharacter::FinishEncounterIntro_Implementation()
{
	if (!HasAuthority() || !bEncounterIntroPending)
	{
		return;
	}

	const UAbilitySystemComponent* ASC = GetAbilitySystemComponent();
	if (IsValid(ASC) && ASC->HasMatchingGameplayTag(GameplayTagSystem::State_Dead))
	{
		GetWorldTimerManager().ClearTimer(EncounterStartRetryTimerHandle);
		bEncounterIntroPending = false;
		PendingEncounterInstigator = nullptr;
		return;
	}

	/*
	 * 최초 타깃의 1순위는 조우를 일으킨 대상이다. 그 대상이 없거나(테스트 시작) 쓸 수 없으면
	 * 가장 가까운 플레이어로 대신한다.
	 */
	ACommonEnemyAIController* EnemyController = Cast<ACommonEnemyAIController>(GetController());
	AActor* InitialTarget = nullptr;
	if (IsValid(EnemyController))
	{
		InitialTarget = EnemyController->IsSelectableCombatPlayer(PendingEncounterInstigator)
		                    ? PendingEncounterInstigator.Get()
		                    : EnemyController->FindNearestCombatPlayer();
	}

	// PIE 시작 직후처럼 컨트롤러나 플레이어가 아직 없으면 생길 때까지 다시 시도한다.
	if (!IsValid(InitialTarget))
	{
		if (!GetWorldTimerManager().IsTimerActive(EncounterStartRetryTimerHandle))
		{
			GetWorldTimerManager().SetTimer(EncounterStartRetryTimerHandle, this, &ThisClass::RetryFinishEncounterIntro,
			                                EncounterStartRetryInterval, true);
		}
		return;
	}

	GetWorldTimerManager().ClearTimer(EncounterStartRetryTimerHandle);
	bEncounterIntroPending = false;
	PendingEncounterInstigator = nullptr;

	/*
	 * 전투 시작은 일반 몬스터와 같은 경로를 쓴다. 휴면 해제, 표시, 이동 잠금 해제,
	 * 그리고 컨트롤러의 최초 타깃 선정 1회(임시 타깃 지정 후 즉시 재평가)가 여기서 일어난다.
	 */
	if (!Super::RequestEncounter_Implementation(InitialTarget))
	{
		UE_LOG(LogTemp, Warning, TEXT("[BossEncounter] StartFailed | Boss=%s | InitialTarget=%s"), *GetNameSafe(this),
		       *GetNameSafe(InitialTarget));
		return;
	}

	// 휴면이 풀리고 모습이 드러난 뒤에야 StateTree를 시작한다. 그 전에는 패턴이 나가지 않는다.
	if (ABossEnemyAIController* BossController = Cast<ABossEnemyAIController>(GetController()))
	{
		BossController->StartBattle();
	}

	// 일반 몬스터는 등장 GA가 끝날 때 무적을 풀고 등장 완료를 알린다. 보스는 그 GA를 타지 않으므로 여기서 처리한다.
	ReleasePendingEmergeInvincibility();
	if (UAbilitySystemComponent* MutableASC = GetAbilitySystemComponent())
	{
		if (!MutableASC->HasMatchingGameplayTag(GameplayTagSystem::State_AI_Encountered))
		{
			MutableASC->AddLooseGameplayTag(GameplayTagSystem::State_AI_Encountered);
		}
	}
	NotifyEmergeFinished();

	UE_LOG(LogTemp, Warning, TEXT("[BossEncounter] CombatStarted | Boss=%s | Fallback=%s | SelectedTarget=%s"),
	       *GetNameSafe(this), *GetNameSafe(InitialTarget),
	       IsValid(EnemyController) ? *GetNameSafe(EnemyController->GetPerceivedTarget()) : TEXT("None"));
}

void ABossMonsterCharacter::RetryFinishEncounterIntro()
{
	IEncounterIntroTarget::Execute_FinishEncounterIntro(this);
}

void ABossMonsterCharacter::ApplyCurrentPhaseMovementProfile()
{
	const FEnemyBossPhaseDefinition* PhaseDefinition =
	    IsValid(BossPhaseComponent) ? BossPhaseComponent->GetCurrentPhaseDefinition() : nullptr;

	// 페이즈에 오버라이드가 없으면 널을 넘겨 Definition의 기본 프로필로 되돌린다.
	ApplyMovementProfile(PhaseDefinition ? PhaseDefinition->MovementProfileOverride.Get() : nullptr);

	// 제자리 전투 여부도 페이즈가 정한다. 이동 컴포넌트는 값만 받아 공전과 준비 이동을 건너뛴다.
	if (UBossMovementComponent* BossMovement = Cast<UBossMovementComponent>(GetCharacterMovement()))
	{
		BossMovement->SetStationary(PhaseDefinition && PhaseDefinition->bStationary);
	}
}

void ABossMonsterCharacter::ApplyCurrentPhaseForm()
{
	if (!IsValid(BossPhaseComponent))
	{
		return;
	}

	if (const FEnemyBossPhaseDefinition* PhaseDefinition = BossPhaseComponent->GetCurrentPhaseDefinition())
	{
		ApplyBossPhaseForm(*PhaseDefinition);
	}
}

void ABossMonsterCharacter::BeginBossFormScaleTransition(UCurveFloat* ScaleCurve, const float NotifyDuration)
{
	if (!IsValid(ScaleCurve))
	{
		return;
	}

	CacheBaseFormState();
	ActiveFormScaleCurve = ScaleCurve;
	ActiveFormScaleDuration = FMath::Max(NotifyDuration, KINDA_SMALL_NUMBER);
	ActiveFormScaleElapsed = 0.0f;
	ApplyBossFormScaleCurveValue();
}

void ABossMonsterCharacter::UpdateBossFormScaleTransition(const float DeltaSeconds)
{
	if (!IsValid(ActiveFormScaleCurve))
	{
		return;
	}

	ActiveFormScaleElapsed =
	    FMath::Min(ActiveFormScaleElapsed + FMath::Max(DeltaSeconds, 0.0f), ActiveFormScaleDuration);
	ApplyBossFormScaleCurveValue();
}

void ABossMonsterCharacter::EndBossFormScaleTransition()
{
	if (!IsValid(ActiveFormScaleCurve))
	{
		return;
	}

	ActiveFormScaleElapsed = ActiveFormScaleDuration;
	ApplyBossFormScaleCurveValue();
	ActiveFormScaleCurve = nullptr;
}

void ABossMonsterCharacter::ApplyBossFormUniformScale(const float ScaleMultiplier)
{
	CacheBaseFormState();
	const float SafeMultiplier = FMath::Max(ScaleMultiplier, 0.01f);

	if (USkeletalMeshComponent* MeshComponent = GetMesh())
	{
		MeshComponent->SetRelativeScale3D(BaseMeshRelativeScale * SafeMultiplier);
	}

	if (UCapsuleComponent* Capsule = GetCapsuleComponent())
	{
		const float Radius = FMath::Max(BaseCapsuleRadius * SafeMultiplier, 0.01f);
		const float HalfHeight = FMath::Max(BaseCapsuleHalfHeight * SafeMultiplier, Radius);
		Capsule->SetCapsuleSize(Radius, HalfHeight, true);
	}
}

void ABossMonsterCharacter::CacheBaseFormState()
{
	if (bBaseFormStateCached)
	{
		return;
	}

	if (const USkeletalMeshComponent* MeshComponent = GetMesh())
	{
		BaseMeshRelativeScale = MeshComponent->GetRelativeScale3D();
	}

	if (const UCapsuleComponent* Capsule = GetCapsuleComponent())
	{
		BaseCapsuleRadius = Capsule->GetUnscaledCapsuleRadius();
		BaseCapsuleHalfHeight = Capsule->GetUnscaledCapsuleHalfHeight();
	}

	bBaseFormStateCached = BaseCapsuleRadius > 0.0f && BaseCapsuleHalfHeight > 0.0f;
}

void ABossMonsterCharacter::ApplyBossPhaseForm(const FEnemyBossPhaseDefinition& PhaseDefinition)
{
	CacheBaseFormState();
	if (!bBaseFormStateCached)
	{
		return;
	}

	if (USkeletalMeshComponent* MeshComponent = GetMesh())
	{
		MeshComponent->SetRelativeScale3D(BaseMeshRelativeScale * PhaseDefinition.MeshScaleMultiplier);
	}

	if (UCapsuleComponent* Capsule = GetCapsuleComponent())
	{
		const float Radius = FMath::Max(BaseCapsuleRadius * PhaseDefinition.CapsuleRadiusMultiplier, 0.01f);
		const float HalfHeight =
		    FMath::Max(BaseCapsuleHalfHeight * PhaseDefinition.CapsuleHalfHeightMultiplier, Radius);
		Capsule->SetCapsuleSize(Radius, HalfHeight, true);
	}
}

void ABossMonsterCharacter::ApplyBossFormScaleCurveValue()
{
	if (!IsValid(ActiveFormScaleCurve))
	{
		return;
	}

	float CurveStartTime = 0.0f;
	float CurveEndTime = 0.0f;
	ActiveFormScaleCurve->GetTimeRange(CurveStartTime, CurveEndTime);
	const float Alpha = FMath::Clamp(ActiveFormScaleElapsed / ActiveFormScaleDuration, 0.0f, 1.0f);
	const float CurveTime = FMath::Lerp(CurveStartTime, CurveEndTime, Alpha);
	ApplyBossFormUniformScale(ActiveFormScaleCurve->GetFloatValue(CurveTime));
}

void ABossMonsterCharacter::HandleBossPhaseChanged(const int32 NewPhaseIndex, const FGameplayTag NewPhaseId)
{
	ApplyCurrentPhaseForm();
	if (HasAuthority())
	{
		ApplyCurrentPhaseMovementProfile();
	}
	bPhaseTransitionPreemptionInProgress = false;

	// 전환 중 더 낮은 임계값까지 내려간 경우, Complete 이후의 현재 페이즈 기준으로
	// 다음 페이즈를 다시 검사한다. StateTree 완료 태스크가 끝난 뒤에 요청되게 다음 틱으로 보낸다.
	if (HasAuthority())
	{
		GetWorldTimerManager().SetTimerForNextTick(this, &ThisClass::RequestNextPhaseFromCurrentHealth);
	}
}

void ABossMonsterCharacter::ResolveArenaCenter()
{
	ArenaCenter = nullptr;

	UWorld* World = GetWorld();
	if (ArenaCenterTag.IsNone() || !IsValid(World))
	{
		return;
	}

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		if (IsValid(*It) && It->ActorHasTag(ArenaCenterTag))
		{
			ArenaCenter = *It;
			return;
		}
	}

	UE_LOG(LogTemp, Warning, TEXT("[BossMovement] 태그 %s 를 가진 아레나 중심 액터를 찾지 못했습니다. | Boss=%s"),
	       *ArenaCenterTag.ToString(), *GetNameSafe(this));
}

void ABossMonsterCharacter::HandleBossHealthChanged(const FOnAttributeChangeData& ChangeData)
{
	const UCharacterAttributeSet* Attributes = GetCharacterAttributeSet();
	if (!IsValid(Attributes) || Attributes->GetMaxHealth() <= 0.0f)
	{
		return;
	}

	const float HealthRatio = FMath::Clamp(ChangeData.NewValue / Attributes->GetMaxHealth(), 0.0f, 1.0f);
	if (BossPhaseComponent->ShouldPreemptTransitionForHealth(HealthRatio))
	{
		PreemptPhaseTransitionForHealth();
		return;
	}
	BossPhaseComponent->TryRequestPhaseTransition(HealthRatio);
}

void ABossMonsterCharacter::PreemptPhaseTransitionForHealth()
{
	if (bPhaseTransitionPreemptionInProgress || !IsValid(BossPhaseComponent))
	{
		return;
	}

	bPhaseTransitionPreemptionInProgress = true;

	// 완료 전에 취소해야 전환 GA의 EndAbility가 링, 몽타주, 전환 이동을 정리한다.
	// 페이즈 확정은 StateTree의 Complete Boss Phase Transition 태스크가 담당한다.
	const FGameplayTag ActiveTransitionTag = BossPhaseComponent->GetPendingTransitionAbilityTag();
	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponent(); ActiveTransitionTag.IsValid())
	{
		const FGameplayTagContainer TransitionTags(ActiveTransitionTag);
		ASC->CancelAbilities(&TransitionTags, nullptr, nullptr);
	}
}

void ABossMonsterCharacter::RequestNextPhaseFromCurrentHealth()
{
	const UCharacterAttributeSet* Attributes = GetCharacterAttributeSet();
	if (!IsValid(BossPhaseComponent) || !IsValid(Attributes) || Attributes->GetMaxHealth() <= 0.0f)
	{
		return;
	}

	const float HealthRatio = FMath::Clamp(Attributes->GetHealth() / Attributes->GetMaxHealth(), 0.0f, 1.0f);
	BossPhaseComponent->TryRequestPhaseTransition(HealthRatio);
}
