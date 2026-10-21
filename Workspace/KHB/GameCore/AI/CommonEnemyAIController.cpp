#include "CommonEnemyAIController.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "Engine/World.h"
#include "GameCore/Tags/GameplayTagSystem.h"
#include "GameFramework/PlayerController.h"
#include "Perception/AIPerceptionComponent.h"
#include "Components/StateTreeAIComponent.h"

namespace CommonEnemyAIControllerPrivate
{
	/** 부모 AMonsterAIController가 시야 갱신에 묶어 두는 UFUNCTION 이름이다. */
	static const FName PerceptionCallbackName(TEXT("OnPerceptionUpdated"));
}

ACommonEnemyAIController::ACommonEnemyAIController()
{
	/*
	 * 일반 적의 평소 이동은 기존과 같이 컨트롤러 추격이 맡는다.
	 * 행동 중에는 GA가 거는 ActionCommitted 태그로 추격이 스스로 멈춘다.
	 * 패턴 준비 이동이 추격과 겹치는 적(보스)은 파생 컨트롤러에서 끈다.
	 */
	bEnablePursuitMovement = true;
}

void ACommonEnemyAIController::BeginPlay()
{
	Super::BeginPlay();
	UpdatePerceptionCallbackBinding();
}

void ACommonEnemyAIController::Tick(const float DeltaSeconds)
{
	UpdatePerceptionCallbackBinding();

	if (IsTargetEvaluationSuspended())
	{
		/*
		 * 부모 Tick은 주기 타깃 평가와 레거시 이동을 돈다. 휴면 중에는 둘 다 필요 없으므로
		 * 그 단계를 건너뛰고 AIController 기본 Tick만 돌린다.
		 */
		ABaseAIController::Tick(DeltaSeconds);
		return;
	}

	Super::Tick(DeltaSeconds);
}

bool ACommonEnemyAIController::IsTargetEvaluationSuspended() const
{
	if (bStationaryDamageTestActive)
	{
		return true;
	}

	const IAbilitySystemInterface* AbilitySystemInterface = Cast<IAbilitySystemInterface>(GetPawn());
	const UAbilitySystemComponent* ASC = AbilitySystemInterface != nullptr
		? AbilitySystemInterface->GetAbilitySystemComponent()
		: nullptr;
	return IsValid(ASC) && ASC->HasMatchingGameplayTag(GameplayTagSystem::State_AI_Dormant);
}

void ACommonEnemyAIController::StopAIForStationaryDamageTest()
{
	bStationaryDamageTestActive = true;
	SetMovementLocked(true);
	StopMovement();
	ClearFocus(EAIFocusPriority::Gameplay);

	if (UAIPerceptionComponent* Perception = GetAIPerceptionComponent())
	{
		Perception->ForgetAll();
		Perception->Deactivate();
	}

	if (StateTreeAIComponent)
	{
		StateTreeAIComponent->StopLogic(TEXT("Stationary damage test"));
	}
}

bool ACommonEnemyAIController::IsSelectableCombatPlayer(const AActor* Candidate) const
{
	return IsValidCombatTarget(Candidate);
}

AActor* ACommonEnemyAIController::FindNearestCombatPlayer() const
{
	const UWorld* World = GetWorld();
	const APawn* MyPawn = GetPawn();
	if (!IsValid(World) || !IsValid(MyPawn))
	{
		return nullptr;
	}

	AActor* NearestPlayer = nullptr;
	double NearestDistanceSq = TNumericLimits<double>::Max();
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		const APlayerController* PlayerController = It->Get();
		APawn* CandidatePawn = IsValid(PlayerController) ? PlayerController->GetPawn() : nullptr;
		if (!IsSelectableCombatPlayer(CandidatePawn))
		{
			continue;
		}

		const double DistanceSq = FVector::DistSquared(MyPawn->GetActorLocation(), CandidatePawn->GetActorLocation());
		if (DistanceSq < NearestDistanceSq)
		{
			NearestDistanceSq = DistanceSq;
			NearestPlayer = CandidatePawn;
		}
	}

	return NearestPlayer;
}

void ACommonEnemyAIController::UpdatePerceptionCallbackBinding()
{
	UAIPerceptionComponent* Perception = GetAIPerceptionComponent();
	if (!IsValid(Perception))
	{
		return;
	}

	const bool bShouldListen = !IsTargetEvaluationSuspended();
	const bool bIsListening = Perception->OnTargetPerceptionUpdated.Contains(
		this, CommonEnemyAIControllerPrivate::PerceptionCallbackName);
	if (bShouldListen == bIsListening)
	{
		return;
	}

	/*
	 * 시야 콜백을 떼도 인지 컴포넌트는 감지 목록을 계속 갱신한다. 휴면이 풀리는 순간
	 * 최초 평가가 이미 보이는 플레이어를 후보로 쓸 수 있는 이유다.
	 *
	 * 부모 콜백은 private 이라 함수 포인터로 다시 묶을 수 없어 UFUNCTION 이름으로 연결한다.
	 */
	if (bShouldListen)
	{
		FScriptDelegate Callback;
		Callback.BindUFunction(this, CommonEnemyAIControllerPrivate::PerceptionCallbackName);
		Perception->OnTargetPerceptionUpdated.AddUnique(Callback);
	}
	else
	{
		Perception->OnTargetPerceptionUpdated.Remove(this, CommonEnemyAIControllerPrivate::PerceptionCallbackName);
	}
}
