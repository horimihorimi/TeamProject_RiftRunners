#include "EnemyPatternComponent.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "GameCore/Character/CommonEnemyCharacter.h"
#include "GameCore/Combat/EnemyPatternMovement.h"
#include "GameCore/Tags/GameplayTagSystem.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "GameCore/AI/MonsterAIController.h"
#include "Engine/World.h"

UEnemyPatternComponent::UEnemyPatternComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UEnemyPatternComponent::SetActivePatternSet(UEnemyPatternSetDataAsset* InPatternSet)
{
	ActivePatternSet = InPatternSet;
	ClearRuntimeState();
}

bool UEnemyPatternComponent::SelectPattern(AActor* TargetActor, const float HealthRatio, const bool bStoreAsPending)
{
	AActor* Owner = GetOwner();
	if (!IsValid(Owner) || !Owner->HasAuthority())
	{
		UE_LOG(LogTemp, Warning, TEXT("[EnemyPattern] SelectFailed | Reason=OwnerInvalidOrNotAuthority | Owner=%s"),
		       *GetNameSafe(Owner));
		return false;
	}
	if (!IsValid(ActivePatternSet))
	{
		UE_LOG(LogTemp, Warning, TEXT("[EnemyPattern] SelectFailed | Reason=ActivePatternSetMissing | Owner=%s"),
		       *GetNameSafe(Owner));
		return false;
	}

	const int32 SelectedIndex = ChooseWeightedEligiblePattern(TargetActor, HealthRatio);
	if (SelectedIndex == INDEX_NONE)
	{
		const double CurrentTimeSeconds = GetCurrentTimeSeconds();
		if (LastSelectionDebugLogTime < 0.0 || CurrentTimeSeconds - LastSelectionDebugLogTime >= 1.0)
		{
			LastSelectionDebugLogTime = CurrentTimeSeconds;
			FString CandidateReasons;
			for (const FEnemyPatternDefinition& Pattern : ActivePatternSet->Patterns)
			{
				CandidateReasons += FString::Printf(
				    TEXT(" [%s: %s]"), *Pattern.PatternId.ToString(),
				    *GetPatternIneligibilityReason(Pattern, TargetActor, HealthRatio, CurrentTimeSeconds));
			}

			UE_LOG(
			    LogTemp, Warning,
			    TEXT(
			        "[EnemyPattern] SelectFailed | Reason=NoEligiblePattern | Owner=%s | PatternSet=%s | PatternCount=%d | Target=%s | Health=%.2f | Candidates=%s"),
			    *GetNameSafe(Owner), *GetNameSafe(ActivePatternSet), ActivePatternSet->Patterns.Num(),
			    *GetNameSafe(TargetActor), HealthRatio, *CandidateReasons);
		}

		if (bStoreAsPending)
		{
			ClearPendingPattern();
		}
		else
		{
			ClearCurrentPattern();
		}
		return false;
	}

	if (bStoreAsPending)
	{
		PendingPatternIndex = SelectedIndex;
	}
	else
	{
		CurrentPatternIndex = SelectedIndex;
		ResolveCurrentMovementOption();
	}

	const FEnemyPatternDefinition* SelectedPattern = GetPatternAt(SelectedIndex);
	UE_LOG(LogTemp, Log, TEXT("[EnemyPattern] Selected | Owner=%s | Slot=%s | Pattern=%s | Ability=%s"),
	       *GetNameSafe(Owner), bStoreAsPending ? TEXT("Pending") : TEXT("Current"),
	       SelectedPattern ? *SelectedPattern->PatternId.ToString() : TEXT("None"),
	       SelectedPattern ? *SelectedPattern->AbilityTag.ToString() : TEXT("None"));

	return true;
}

bool UEnemyPatternComponent::ValidatePendingPattern(AActor* TargetActor, const float HealthRatio) const
{
	const FEnemyPatternDefinition* PendingPattern = GetPatternAt(PendingPatternIndex);
	return PendingPattern && IsPatternEligible(*PendingPattern, TargetActor, HealthRatio, GetCurrentTimeSeconds());
}

bool UEnemyPatternComponent::PromotePendingPattern()
{
	if (!GetPatternAt(PendingPatternIndex))
	{
		return false;
	}

	CurrentPatternIndex = PendingPatternIndex;
	PendingPatternIndex = INDEX_NONE;

	// 이동 후보는 사전 선택 시점이 아니라 실제 실행 직전에 뽑는다.
	ResolveCurrentMovementOption();
	return true;
}

bool UEnemyPatternComponent::CommitCurrentPattern()
{
	const FEnemyPatternDefinition* CurrentPattern = GetPatternAt(CurrentPatternIndex);
	if (!CurrentPattern || !CurrentPattern->PatternId.IsValid())
	{
		return false;
	}

	const double CurrentTimeSeconds = GetCurrentTimeSeconds();
	if (CurrentPattern->Cooldown > 0.0f)
	{
		CooldownEndTimes.Add(CurrentPattern->PatternId, CurrentTimeSeconds + CurrentPattern->Cooldown);
	}

	RecentPatternHistory.Add(CurrentPattern->PatternId);
	return true;
}

void UEnemyPatternComponent::ClearCurrentPattern()
{
	CurrentPatternIndex = INDEX_NONE;
	CurrentMovementOption = FEnemyPatternMovementOption();
	bHasCurrentMovementOption = false;
}

void UEnemyPatternComponent::ClearPendingPattern()
{
	PendingPatternIndex = INDEX_NONE;
}

void UEnemyPatternComponent::ClearRuntimeState()
{
	CurrentPatternIndex = INDEX_NONE;
	PendingPatternIndex = INDEX_NONE;
	CurrentMovementOption = FEnemyPatternMovementOption();
	bHasCurrentMovementOption = false;
	CooldownEndTimes.Reset();
	RecentPatternHistory.Reset();
}

bool UEnemyPatternComponent::HasCurrentPattern() const
{
	return GetPatternAt(CurrentPatternIndex) != nullptr;
}

bool UEnemyPatternComponent::HasPendingPattern() const
{
	return GetPatternAt(PendingPatternIndex) != nullptr;
}

bool UEnemyPatternComponent::GetCurrentPattern(FEnemyPatternDefinition& OutPattern) const
{
	const FEnemyPatternDefinition* Pattern = GetPatternAt(CurrentPatternIndex);
	if (!Pattern)
	{
		return false;
	}

	OutPattern = *Pattern;
	return true;
}

bool UEnemyPatternComponent::GetPendingPattern(FEnemyPatternDefinition& OutPattern) const
{
	const FEnemyPatternDefinition* Pattern = GetPatternAt(PendingPatternIndex);
	if (!Pattern)
	{
		return false;
	}

	OutPattern = *Pattern;
	return true;
}

bool UEnemyPatternComponent::GetCurrentMovementOption(FEnemyPatternMovementOption& OutOption) const
{
	if (!bHasCurrentMovementOption)
	{
		return false;
	}

	OutOption = CurrentMovementOption;
	return true;
}

void UEnemyPatternComponent::ResolveCurrentMovementOption()
{
	CurrentMovementOption = FEnemyPatternMovementOption();
	bHasCurrentMovementOption = false;

	const FEnemyPatternDefinition* Pattern = GetPatternAt(CurrentPatternIndex);
	if (!Pattern || Pattern->MovementOptions.IsEmpty())
	{
		return;
	}

	float TotalWeight = 0.0f;
	const IAbilitySystemInterface* AbilitySystemInterface = Cast<IAbilitySystemInterface>(GetOwner());
	const UAbilitySystemComponent* ASC =
	    AbilitySystemInterface ? AbilitySystemInterface->GetAbilitySystemComponent() : nullptr;
	for (const FEnemyPatternMovementOption& Option : Pattern->MovementOptions)
	{
		// Reposition은 Teleport 특성만 쓸 수 있다. 특성이 없으면 다른 이동 후보만 뽑는다.
		if (Option.Intent == EEnemyMovementIntent::RepositionForAction &&
		    (!IsValid(ASC) || !ASC->HasMatchingGameplayTag(GameplayTagSystem::Enemy_Trait_Teleport)))
		{
			continue;
		}
		TotalWeight += FMath::Max(Option.Weight, 0.0f);
	}

	/*
	 * 모든 후보의 Weight가 0이면 뽑을 수 없다.
	 *
	 * 이동을 아예 안 넣은 것과 구분해야 한다. 후보를 적어 두고 Weight를 0으로 둔 것은
	 * 대개 데이터 실수이므로, 조용히 이동을 건너뛰지 않고 경고를 남긴다.
	 */
	if (TotalWeight <= 0.0f)
	{
		UE_LOG(LogTemp, Verbose,
		       TEXT("[EnemyPattern] MovementOptionSkipped | Owner=%s | Pattern=%s | Reason=NoEligibleOption"),
		       *GetNameSafe(GetOwner()), *Pattern->PatternId.ToString());
		return;
	}

	float Roll = FMath::FRandRange(0.0f, TotalWeight);
	for (const FEnemyPatternMovementOption& Option : Pattern->MovementOptions)
	{
		if (Option.Intent == EEnemyMovementIntent::RepositionForAction &&
		    (!IsValid(ASC) || !ASC->HasMatchingGameplayTag(GameplayTagSystem::Enemy_Trait_Teleport)))
		{
			continue;
		}
		Roll -= FMath::Max(Option.Weight, 0.0f);
		if (Roll <= 0.0f)
		{
			CurrentMovementOption = Option;
			bHasCurrentMovementOption = true;
			break;
		}
	}

	// 부동소수 오차로 루프를 빠져나오지 못하면 마지막 후보로 확정한다.
	if (!bHasCurrentMovementOption)
	{
		for (int32 Index = Pattern->MovementOptions.Num() - 1; Index >= 0; --Index)
		{
			const FEnemyPatternMovementOption& Option = Pattern->MovementOptions[Index];
			if (Option.Weight > 0.0f &&
			    (Option.Intent != EEnemyMovementIntent::RepositionForAction ||
			     (IsValid(ASC) && ASC->HasMatchingGameplayTag(GameplayTagSystem::Enemy_Trait_Teleport))))
			{
				CurrentMovementOption = Option;
				bHasCurrentMovementOption = true;
				break;
			}
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[EnemyPattern] MovementOption | Owner=%s | Pattern=%s | Intent=%d | DuringAbility=%s"),
	       *GetNameSafe(GetOwner()), *Pattern->PatternId.ToString(), static_cast<int32>(CurrentMovementOption.Intent),
	       CurrentMovementOption.bMoveDuringAbility ? TEXT("True") : TEXT("False"));
}

const FEnemyPatternDefinition* UEnemyPatternComponent::GetPatternAt(const int32 PatternIndex) const
{
	return IsValid(ActivePatternSet) && ActivePatternSet->Patterns.IsValidIndex(PatternIndex)
	           ? &ActivePatternSet->Patterns[PatternIndex]
	           : nullptr;
}

bool UEnemyPatternComponent::IsPatternEligible(const FEnemyPatternDefinition& Pattern, AActor* TargetActor,
                                               const float HealthRatio, const double CurrentTimeSeconds) const
{
	return GetPatternIneligibilityReason(Pattern, TargetActor, HealthRatio, CurrentTimeSeconds).IsEmpty();
}

FString UEnemyPatternComponent::GetPatternIneligibilityReason(const FEnemyPatternDefinition& Pattern,
                                                              AActor* TargetActor, const float HealthRatio,
                                                              const double CurrentTimeSeconds) const
{
	if (!Pattern.PatternId.IsValid() || !Pattern.AbilityTag.IsValid() || Pattern.Weight <= 0.0f)
	{
		return FString::Printf(TEXT("InvalidDefinition(Id=%s Ability=%s Weight=%.2f)"), *Pattern.PatternId.ToString(),
		                       *Pattern.AbilityTag.ToString(), Pattern.Weight);
	}

	// 타깃을 향한 공격 패턴은 기억만으로 시작하지 않는다. 비대상 유틸리티 패턴은 유지한다.
	if (Pattern.bRequiresVisibleTarget)
	{
		if (!IsValid(TargetActor))
		{
			return TEXT("MissingTarget");
		}
		const APawn* Pawn = Cast<APawn>(GetOwner());
		const AMonsterAIController* Controller = Pawn ? Cast<AMonsterAIController>(Pawn->GetController()) : nullptr;
		if (Controller && !Controller->CanAttackTarget(TargetActor))
		{
			return TEXT("TargetOccluded");
		}
	}

	if (HealthRatio < Pattern.MinHealthRatio || HealthRatio > Pattern.MaxHealthRatio)
	{
		return FString::Printf(TEXT("HealthOutOfRange(Current=%.2f Required=%.2f~%.2f)"), HealthRatio,
		                       Pattern.MinHealthRatio, Pattern.MaxHealthRatio);
	}

	if (const double* CooldownEndTime = CooldownEndTimes.Find(Pattern.PatternId))
	{
		if (CurrentTimeSeconds < *CooldownEndTime)
		{
			return FString::Printf(TEXT("Cooldown(Remaining=%.2f)"), *CooldownEndTime - CurrentTimeSeconds);
		}
	}

	if (Pattern.RepeatLimit > 0 && GetConsecutiveUseCount(Pattern.PatternId) >= Pattern.RepeatLimit)
	{
		return FString::Printf(TEXT("RepeatLimit(Current=%d Limit=%d)"), GetConsecutiveUseCount(Pattern.PatternId),
		                       Pattern.RepeatLimit);
	}

	// 고도는 타깃과 무관한 조건이다. 거리 조건이 없는 패턴도 걸러야 하므로 거리 검사보다 먼저 본다.
	const bool bRequiresHeight = Pattern.MinHeightFromFloor > 0.0f || Pattern.MaxHeightFromFloor > 0.0f;
	if (bRequiresHeight)
	{
		const float Height = GetOwnerHeightFromFloor();
		if (Height < Pattern.MinHeightFromFloor)
		{
			return FString::Printf(TEXT("TooLow(Current=%.1f Min=%.1f)"), Height, Pattern.MinHeightFromFloor);
		}

		if (Pattern.MaxHeightFromFloor > 0.0f && Height > Pattern.MaxHeightFromFloor)
		{
			return FString::Printf(TEXT("TooHigh(Current=%.1f Max=%.1f)"), Height, Pattern.MaxHeightFromFloor);
		}
	}

	const bool bRequiresTargetDistance = Pattern.MinDistance > 0.0f || Pattern.MaxDistance > 0.0f;
	if (bRequiresTargetDistance)
	{
		const AActor* Owner = GetOwner();
		if (!IsValid(Owner) || !IsValid(TargetActor))
		{
			return TEXT("TargetRequiredForDistance");
		}

		/*
		 * 고도를 크게 쓰는 적은 바로 위에 있어도 멀다고 판정되므로 수평으로 재고 고도는 위에서 따로 본다.
		 * 기존 일반 몬스터의 사거리는 3D 기준으로 맞춰져 있어 프로필에서 고른다.
		 */
		const float Distance = GetOwnerDistanceMeasure() == EEnemyDistanceMeasure::Full3D
		                           ? FVector::Dist(Owner->GetActorLocation(), TargetActor->GetActorLocation())
		                           : FVector::Dist2D(Owner->GetActorLocation(), TargetActor->GetActorLocation());
		if (Distance < Pattern.MinDistance)
		{
			return FString::Printf(TEXT("TooClose(Current=%.1f Min=%.1f)"), Distance, Pattern.MinDistance);
		}

		if (Pattern.MaxDistance > 0.0f && Distance > Pattern.MaxDistance)
		{
			return FString::Printf(TEXT("TooFar(Current=%.1f Max=%.1f)"), Distance, Pattern.MaxDistance);
		}
	}

	// GA 순회 비용이 가장 크므로 값싼 조건을 모두 통과한 뒤에 본다.
	if (!CanActivatePatternAbility(Pattern.AbilityTag))
	{
		return TEXT("CannotActivateAbility");
	}

	return FString();
}

int32 UEnemyPatternComponent::ChooseWeightedEligiblePattern(AActor* TargetActor, const float HealthRatio) const
{
	if (!IsValid(ActivePatternSet))
	{
		return INDEX_NONE;
	}

	const double CurrentTimeSeconds = GetCurrentTimeSeconds();
	TArray<int32> CandidateIndices;
	float TotalWeight = 0.0f;

	for (int32 Index = 0; Index < ActivePatternSet->Patterns.Num(); ++Index)
	{
		const FEnemyPatternDefinition& Pattern = ActivePatternSet->Patterns[Index];
		if (!IsPatternEligible(Pattern, TargetActor, HealthRatio, CurrentTimeSeconds))
		{
			continue;
		}

		CandidateIndices.Add(Index);
		TotalWeight += Pattern.Weight;
	}

	if (CandidateIndices.IsEmpty() || TotalWeight <= 0.0f)
	{
		return INDEX_NONE;
	}

	float Roll = FMath::FRandRange(0.0f, TotalWeight);
	for (const int32 CandidateIndex : CandidateIndices)
	{
		const FEnemyPatternDefinition& Candidate = ActivePatternSet->Patterns[CandidateIndex];
		Roll -= Candidate.Weight;
		if (Roll <= 0.0f)
		{
			return CandidateIndex;
		}
	}

	return CandidateIndices.Last();
}

float UEnemyPatternComponent::GetOwnerHeightFromFloor() const
{
	const AActor* Owner = GetOwner();
	if (!IsValid(Owner))
	{
		return 0.0f;
	}

	TInlineComponentArray<UActorComponent*> Components(Owner);
	for (UActorComponent* Component : Components)
	{
		if (const IEnemyPatternMovement* Movement = Cast<IEnemyPatternMovement>(Component))
		{
			return Movement->GetHeightFromFloor();
		}
	}

	return 0.0f;
}

EEnemyDistanceMeasure UEnemyPatternComponent::GetOwnerDistanceMeasure() const
{
	const ACommonEnemyCharacter* Character = Cast<ACommonEnemyCharacter>(GetOwner());
	const UEnemyMovementProfileDataAsset* Profile =
	    IsValid(Character) ? Character->GetActiveMovementProfile() : nullptr;
	return IsValid(Profile) ? Profile->PatternDistanceMeasure : EEnemyDistanceMeasure::Horizontal;
}

bool UEnemyPatternComponent::CanActivatePatternAbility(const FGameplayTag& AbilityTag) const
{
	const IAbilitySystemInterface* AbilitySystemInterface = Cast<IAbilitySystemInterface>(GetOwner());
	const UAbilitySystemComponent* ASC =
	    AbilitySystemInterface != nullptr ? AbilitySystemInterface->GetAbilitySystemComponent() : nullptr;
	if (!IsValid(ASC))
	{
		return true;
	}

	for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
	{
		if (!IsValid(Spec.Ability) || !Spec.Ability->GetAssetTags().HasTag(AbilityTag))
		{
			continue;
		}

		// InstancedPerActor GA는 인스턴스가 상태를 들고 있으므로 CDO가 아니라 인스턴스에 묻는다.
		const UGameplayAbility* AbilityToAsk =
		    Spec.GetPrimaryInstance() ? Spec.GetPrimaryInstance() : Spec.Ability.Get();
		return IsValid(AbilityToAsk) && AbilityToAsk->CanActivateAbility(Spec.Handle, ASC->AbilityActorInfo.Get());
	}

	// 태그에 맞는 GA가 부여되지 않았다. AbilitySet 설정 누락이다.
	return false;
}

int32 UEnemyPatternComponent::GetConsecutiveUseCount(const FGameplayTag& PatternId) const
{
	int32 ConsecutiveUseCount = 0;
	for (int32 Index = RecentPatternHistory.Num() - 1; Index >= 0; --Index)
	{
		if (RecentPatternHistory[Index] != PatternId)
		{
			break;
		}

		++ConsecutiveUseCount;
	}

	return ConsecutiveUseCount;
}

double UEnemyPatternComponent::GetCurrentTimeSeconds() const
{
	const UWorld* World = GetWorld();
	return IsValid(World) ? World->GetTimeSeconds() : 0.0;
}

bool UEnemyPatternComponent::SelectPrioritySplitPattern(AActor* TargetActor, float HealthRatio)
{
	if (!GetOwner()->HasAuthority() || !ActivePatternSet)
	{
		return false;
	}
	for (int32 Index = 0; Index < ActivePatternSet->Patterns.Num(); ++Index)
	{
		const auto& Pattern = ActivePatternSet->Patterns[Index];
		if (Pattern.AbilityTag == GameplayTagSystem::Ability_Enemy_Split &&
		    IsPatternEligible(Pattern, TargetActor, HealthRatio, GetCurrentTimeSeconds()))
		{
			ClearPendingPattern();
			CurrentPatternIndex = Index;
			ResolveCurrentMovementOption();
			return true;
		}
	}
	return false;
}
