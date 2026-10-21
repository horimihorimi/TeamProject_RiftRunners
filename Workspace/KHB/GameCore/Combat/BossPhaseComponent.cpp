#include "BossPhaseComponent.h"

#include "GameCore/Enemy/Data/EnemyDataAssets.h"
#include "Net/UnrealNetwork.h"

UBossPhaseComponent::UBossPhaseComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UBossPhaseComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UBossPhaseComponent, CurrentPhaseIndex);
	DOREPLIFETIME(UBossPhaseComponent, PendingPhaseIndex);
	DOREPLIFETIME(UBossPhaseComponent, bTransitionInProgress);
}

void UBossPhaseComponent::OnRep_CurrentPhaseIndex()
{
	// 클라이언트도 같은 Definition DA를 BeginPlay에서 초기화한다. 복제된 인덱스가
	// 도착하면 Form만 갱신하고, StateTree/패턴 선택은 서버에서만 계속 수행한다.
	OnPhaseChanged.Broadcast(CurrentPhaseIndex, GetCurrentPhaseId());
}

bool UBossPhaseComponent::InitializePhaseSet(UBossPhaseSetDataAsset* InPhaseSet)
{
	PhaseSet = InPhaseSet;
	CurrentPhaseIndex = INDEX_NONE;
	PendingPhaseIndex = INDEX_NONE;
	bTransitionInProgress = false;

	if (!IsValid(PhaseSet) || PhaseSet->Phases.IsEmpty())
	{
		return false;
	}

	// 첫 행은 전투 시작 페이즈다. 이후 행은 체력 임계값 순서로 진입한다.
	CurrentPhaseIndex = 0;
	OnPhaseChanged.Broadcast(CurrentPhaseIndex, GetCurrentPhaseId());
	return true;
}

bool UBossPhaseComponent::TryRequestPhaseTransition(float HealthRatio)
{
	if (!IsValid(PhaseSet) || bTransitionInProgress)
	{
		return false;
	}

	const int32 NextPhaseIndex = CurrentPhaseIndex + 1;
	if (!PhaseSet->Phases.IsValidIndex(NextPhaseIndex))
	{
		return false;
	}

	const FEnemyBossPhaseDefinition& NextPhase = PhaseSet->Phases[NextPhaseIndex];
	if (HealthRatio > NextPhase.HealthThreshold)
	{
		return false;
	}

	PendingPhaseIndex = NextPhaseIndex;
	bTransitionInProgress = true;
	OnPhaseTransitionRequested.Broadcast(PendingPhaseIndex, GetPendingPhaseId());
	return true;
}

bool UBossPhaseComponent::CompletePhaseTransition()
{
	if (!IsValid(PhaseSet) || !bTransitionInProgress || !PhaseSet->Phases.IsValidIndex(PendingPhaseIndex))
	{
		return false;
	}

	CurrentPhaseIndex = PendingPhaseIndex;
	PendingPhaseIndex = INDEX_NONE;
	bTransitionInProgress = false;
	OnPhaseChanged.Broadcast(CurrentPhaseIndex, GetCurrentPhaseId());
	return true;
}

bool UBossPhaseComponent::ShouldPreemptTransitionForHealth(const float HealthRatio) const
{
	if (!IsValid(PhaseSet) || !bTransitionInProgress)
	{
		return false;
	}

	// 현재 Pending(P2)을 확정한 다음 진입할 페이즈(P3)의 임계값을 본다.
	const int32 FollowingPhaseIndex = PendingPhaseIndex + 1;
	return PhaseSet->Phases.IsValidIndex(FollowingPhaseIndex)
		&& HealthRatio <= PhaseSet->Phases[FollowingPhaseIndex].HealthThreshold;
}

FGameplayTag UBossPhaseComponent::GetCurrentPhaseId() const
{
	return IsValid(PhaseSet) && PhaseSet->Phases.IsValidIndex(CurrentPhaseIndex)
		? PhaseSet->Phases[CurrentPhaseIndex].PhaseId
		: FGameplayTag();
}

FGameplayTag UBossPhaseComponent::GetPendingPhaseId() const
{
	return IsValid(PhaseSet) && PhaseSet->Phases.IsValidIndex(PendingPhaseIndex)
		? PhaseSet->Phases[PendingPhaseIndex].PhaseId
		: FGameplayTag();
}

FGameplayTag UBossPhaseComponent::GetPendingTransitionAbilityTag() const
{
	return IsValid(PhaseSet) && PhaseSet->Phases.IsValidIndex(PendingPhaseIndex)
		? PhaseSet->Phases[PendingPhaseIndex].TransitionAbilityTag
		: FGameplayTag();
}

UEnemyPatternSetDataAsset* UBossPhaseComponent::GetCurrentPatternSet() const
{
	return IsValid(PhaseSet) && PhaseSet->Phases.IsValidIndex(CurrentPhaseIndex)
		? PhaseSet->Phases[CurrentPhaseIndex].PatternSet
		: nullptr;
}

const FEnemyBossPhaseDefinition* UBossPhaseComponent::GetCurrentPhaseDefinition() const
{
	return IsValid(PhaseSet) && PhaseSet->Phases.IsValidIndex(CurrentPhaseIndex)
		? &PhaseSet->Phases[CurrentPhaseIndex]
		: nullptr;
}
