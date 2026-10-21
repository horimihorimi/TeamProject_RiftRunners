#include "MonsterCombatPositioningComponent.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "Components/CapsuleComponent.h"
#include "DrawDebugHelpers.h"
#include "GameCore/Character/CommonEnemyCharacter.h"
#include "GameCore/Combat/EnemyPatternMovement.h"
#include "GameCore/Movement/MonsterMovementComponent.h"
#include "GameCore/Tags/GameplayTagSystem.h"
#include "GameFramework/Character.h"
#include "NavigationSystem.h"

UMonsterCombatPositioningComponent::UMonsterCombatPositioningComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

EEnemyPatternPreparationStatus UMonsterCombatPositioningComponent::BeginReposition(
	const FGameplayTag PatternAbilityTag,
	AActor* TargetActor)
{
	RepositionSpecHandle = FGameplayAbilitySpecHandle();

	// Reposition For Action은 Teleport 특성 전용 옵션이다. 패턴 선택도 같은 조건으로 거르지만 여기서 한 번 더 막는다.
	UAbilitySystemComponent* ASC = GetOwnerAbilitySystemComponent();
	if (!IsValid(ASC) || !ASC->HasMatchingGameplayTag(GameplayTagSystem::Enemy_Trait_Teleport))
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[EliteTeleport] RepositionRejected | Owner=%s | Action=%s | Reason=NoTeleportTrait"),
			*GetNameSafe(GetOwner()),
			*PatternAbilityTag.ToString());
		return EEnemyPatternPreparationStatus::Failed;
	}

	// 위치 프로필은 행동 태그로 짝지어져 있다. SelectedAction 자리에 패턴의 AbilityTag를 넘긴다.
	FGameplayTag RepositionAbilityTag;
	const EMonsterActionPositionPreparationResult Result =
		PrepareSelectedAction(TargetActor, PatternAbilityTag, RepositionAbilityTag);
	if (Result == EMonsterActionPositionPreparationResult::Ready)
	{
		return EEnemyPatternPreparationStatus::Ready;
	}

	if (Result == EMonsterActionPositionPreparationResult::Failed || !RepositionAbilityTag.IsValid())
	{
		return EEnemyPatternPreparationStatus::Failed;
	}

	if (!ASC->TryActivateAbilitiesByTag(FGameplayTagContainer(RepositionAbilityTag)))
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[EliteTeleport] RepositionRejected | Owner=%s | Action=%s | Reason=AbilityActivationFailed | RepositionAbility=%s"),
			*GetNameSafe(GetOwner()),
			*PatternAbilityTag.ToString(),
			*RepositionAbilityTag.ToString());
		CancelPendingReposition();
		return EEnemyPatternPreparationStatus::Failed;
	}

	for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
	{
		if (Spec.IsActive() && IsValid(Spec.Ability) && Spec.Ability->GetAssetTags().HasTag(RepositionAbilityTag))
		{
			RepositionSpecHandle = Spec.Handle;
			return EEnemyPatternPreparationStatus::Preparing;
		}
	}

	// 활성화와 동시에 끝난 GA(몽타주 없이 즉시 이동)는 기록된 결과로 판정한다.
	return ResolveRepositionResult();
}

EEnemyPatternPreparationStatus UMonsterCombatPositioningComponent::UpdateReposition()
{
	const UAbilitySystemComponent* ASC = GetOwnerAbilitySystemComponent();
	if (!RepositionSpecHandle.IsValid() || !IsValid(ASC))
	{
		return EEnemyPatternPreparationStatus::Failed;
	}

	const FGameplayAbilitySpec* Spec = ASC->FindAbilitySpecFromHandle(RepositionSpecHandle);
	if (Spec != nullptr && Spec->IsActive())
	{
		return EEnemyPatternPreparationStatus::Preparing;
	}

	RepositionSpecHandle = FGameplayAbilitySpecHandle();
	return ResolveRepositionResult();
}

void UMonsterCombatPositioningComponent::CancelReposition()
{
	/*
	 * 재배치 GA는 끊지 않는다. 예약된 도착지만 취소하고, 이미 시작된 순간이동은 GA가 스스로 마무리한다.
	 * 커밋 전이라면 GA는 예약이 사라진 것을 보고 제자리에서 은신을 풀며 끝난다.
	 */
	if (RepositionSpecHandle.IsValid() && HasPendingReposition())
	{
		CancelPendingReposition();
	}

	RepositionSpecHandle = FGameplayAbilitySpecHandle();
}

EMonsterActionPositionPreparationResult UMonsterCombatPositioningComponent::PrepareSelectedAction(
	AActor* TargetActor,
	const FGameplayTag SelectedAction,
	FGameplayTag& OutRepositionAbilityTag)
{
	OutRepositionAbilityTag = FGameplayTag();
	bLastRepositionSuccessful = false;
	CancelPendingReposition();

	const FMonsterActionPositionProfile* Profile = FindProfile(SelectedAction);
	if (Profile == nullptr)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[EliteTeleport] PrepareRejected | Owner=%s | Action=%s | Reason=ProfileNotFound"),
			*GetNameSafe(GetOwner()),
			*SelectedAction.ToString());
		// 프로필이 없는 기존 몬스터는 위치 준비 단계를 통과한다.
		return EMonsterActionPositionPreparationResult::Ready;
	}

	if (!IsValid(TargetActor))
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[EliteTeleport] PrepareRejected | Owner=%s | Action=%s | Reason=InvalidTarget"),
			*GetNameSafe(GetOwner()),
			*SelectedAction.ToString());
		return EMonsterActionPositionPreparationResult::Failed;
	}

	/*
	 * 은신 순간이동은 사거리를 벗어났을 때의 교정이 아니라 패턴마다 거치는 기본 단계다.
	 * 따라서 Always 정책에서는 현재 위치가 이미 유효해도 재배치한다. 현재 위치 판정은
	 * 후보를 하나도 찾지 못했을 때 제자리 시전을 허용할지 정하는 폴백으로만 쓴다.
	 */
	const bool bPositionValid = IsCurrentPositionValid(*Profile, TargetActor);
	const bool bWantsReposition =
		Profile->RepositionPolicy == EMonsterRepositionPolicy::Always || !bPositionValid;

	if (!bWantsReposition)
	{
		UE_LOG(LogTemp, Log,
			TEXT("[EliteTeleport] RepositionSkipped | Owner=%s | Action=%s | Reason=CurrentPositionValid"),
			*GetNameSafe(GetOwner()),
			*SelectedAction.ToString());
		return EMonsterActionPositionPreparationResult::Ready;
	}

	if (Profile->RepositionPolicy == EMonsterRepositionPolicy::Never
		|| !Profile->RepositionAbilityTag.IsValid())
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[EliteTeleport] PrepareRejected | Owner=%s | Action=%s | Reason=RepositionDisabledOrTagMissing | Policy=%d | RepositionAbility=%s"),
			*GetNameSafe(GetOwner()),
			*SelectedAction.ToString(),
			static_cast<int32>(Profile->RepositionPolicy),
			*Profile->RepositionAbilityTag.ToString());
		return EMonsterActionPositionPreparationResult::Failed;
	}

	FVector Candidate;
	if (!FindBestCandidate(*Profile, TargetActor, Candidate))
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[EliteTeleport] CandidateSearchFailed | Owner=%s | Action=%s | Target=%s | DistanceRange=%.1f~%.1f | Directions=%d | Distances=%d"),
			*GetNameSafe(GetOwner()),
			*SelectedAction.ToString(),
			*GetNameSafe(TargetActor),
			Profile->DesiredMinDistance,
			Profile->DesiredMaxDistance,
			Profile->DirectionSampleCount,
			Profile->DistanceSampleCount);
		// 갈 곳이 없어도 현재 위치가 유효하면 은신 없이 그 자리에서 시전한다.
		return bPositionValid
			? EMonsterActionPositionPreparationResult::Ready
			: EMonsterActionPositionPreparationResult::Failed;
	}

	PendingLocation = Candidate;
	PendingAction = SelectedAction;
	PendingTarget = TargetActor;
	bHasPendingReposition = true;
	OutRepositionAbilityTag = Profile->RepositionAbilityTag;

	UE_LOG(LogTemp, Log,
		TEXT("[EliteTeleport] DestinationPrepared | Owner=%s | Action=%s | Target=%s | Destination=%s | RepositionAbility=%s"),
		*GetNameSafe(GetOwner()),
		*SelectedAction.ToString(),
		*GetNameSafe(TargetActor),
		*PendingLocation.ToString(),
		*OutRepositionAbilityTag.ToString());
	return EMonsterActionPositionPreparationResult::NeedsReposition;
}

bool UMonsterCombatPositioningComponent::CommitPendingReposition()
{
	if (!bHasPendingReposition)
	{
		return false;
	}

	const FMonsterActionPositionProfile* Profile = FindProfile(PendingAction);
	AActor* OwnerActor = GetOwner();
	AActor* TargetActor = PendingTarget.Get();
	bool bHasLineOfSight = false;

	// 이펙트가 끝난 직후에도 충돌/지형/시야 상태를 다시 검사한다.
	if (Profile == nullptr
		|| !IsValid(OwnerActor)
		|| !IsValid(TargetActor)
		|| !ValidateCandidate(*Profile, PendingLocation, TargetActor, bHasLineOfSight))
	{
		CancelPendingReposition();
		return false;
	}

	const FRotator Facing = IsValid(TargetActor)
		? (TargetActor->GetActorLocation() - PendingLocation).Rotation()
		: OwnerActor->GetActorRotation();
	const FVector TeleportDestination = PendingLocation;

	const bool bTeleported = OwnerActor->SetActorLocationAndRotation(
		TeleportDestination,
		Facing,
		false,
		nullptr,
		ETeleportType::TeleportPhysics);

	if (bTeleported)
	{
		FinalizeTeleport();
		RememberTeleportLocation(TeleportDestination);
	}

	UE_LOG(LogTemp, Log,
		TEXT("[EliteTeleport] CommitResult | Owner=%s | Action=%s | Destination=%s | Success=%s"),
		*GetNameSafe(OwnerActor),
		*PendingAction.ToString(),
		*TeleportDestination.ToString(),
		bTeleported ? TEXT("True") : TEXT("False"));

	bLastRepositionSuccessful = bTeleported;
	bHasPendingReposition = false;
	PendingTarget.Reset();
	PendingAction = FGameplayTag();
	return bTeleported;
}

void UMonsterCombatPositioningComponent::CancelPendingReposition()
{
	bHasPendingReposition = false;
	PendingTarget.Reset();
	PendingAction = FGameplayTag();
}

const FMonsterActionPositionProfile* UMonsterCombatPositioningComponent::FindProfile(const FGameplayTag ActionTag) const
{
	return ActionProfiles.FindByPredicate(
		[ActionTag](const FMonsterActionPositionProfile& Profile)
		{
			return Profile.ActionTag == ActionTag;
		});
}

bool UMonsterCombatPositioningComponent::IsCurrentPositionValid(
	const FMonsterActionPositionProfile& Profile,
	const AActor* TargetActor) const
{
	AActor* OwnerActor = GetOwner();
	if (!IsValid(OwnerActor) || !IsValid(TargetActor))
	{
		return false;
	}

	const float Distance = FVector::Dist2D(OwnerActor->GetActorLocation(), TargetActor->GetActorLocation());
	const bool bInRange = (Profile.DesiredMinDistance <= 0.0f || Distance >= Profile.DesiredMinDistance)
		&& (Profile.DesiredMaxDistance <= 0.0f || Distance <= Profile.DesiredMaxDistance);
	if (!bInRange)
	{
		return false;
	}

	bool bHasLineOfSight = false;
	return ValidateCandidate(Profile, OwnerActor->GetActorLocation(), TargetActor, bHasLineOfSight)
		&& (!Profile.bRequireLineOfSight || bHasLineOfSight);
}

bool UMonsterCombatPositioningComponent::FindBestCandidate(
	const FMonsterActionPositionProfile& Profile,
	AActor* TargetActor,
	FVector& OutLocation)
{
	AActor* OwnerActor = GetOwner();
	if (!IsValid(OwnerActor) || !IsValid(TargetActor))
	{
		return false;
	}

	const float MinDistance = FMath::Max(0.0f, Profile.DesiredMinDistance);
	const float MaxDistance = FMath::Max(MinDistance, Profile.DesiredMaxDistance);
	if (MaxDistance <= KINDA_SMALL_NUMBER)
	{
		return false;
	}

	const FVector TargetLocation = TargetActor->GetActorLocation();

	FVector TargetForward = TargetActor->GetActorForwardVector().GetSafeNormal2D();
	if (TargetForward.IsNearlyZero())
	{
		TargetForward = FVector::ForwardVector;
	}

	float BestScore = -TNumericLimits<float>::Max();
	bool bFoundCandidate = false;
	int32 RejectedByResolve = 0;
	int32 RejectedByValidation = 0;
	const int32 DirectionCount = FMath::Max(4, Profile.DirectionSampleCount);
	const int32 DistanceCount = FMath::Max(1, Profile.DistanceSampleCount);

	for (int32 DirectionIndex = 0; DirectionIndex < DirectionCount; ++DirectionIndex)
	{
		const float AngleDegrees = 360.0f * static_cast<float>(DirectionIndex) / static_cast<float>(DirectionCount);
		const FVector Direction = TargetForward.RotateAngleAxis(AngleDegrees, FVector::UpVector).GetSafeNormal2D();

		for (int32 DistanceIndex = 0; DistanceIndex < DistanceCount; ++DistanceIndex)
		{
			const float Alpha = DistanceCount == 1
				? 0.5f
				: static_cast<float>(DistanceIndex) / static_cast<float>(DistanceCount - 1);
			const float Radius = FMath::Lerp(MinDistance, MaxDistance, Alpha);
			const FVector RawLocation = TargetLocation + Direction * Radius;

			FVector CandidateLocation;
			if (!ResolveCandidateLocation(RawLocation, CandidateLocation))
			{
				++RejectedByResolve;
				continue;
			}

			bool bHasLineOfSight = false;
			if (!ValidateCandidate(Profile, CandidateLocation, TargetActor, bHasLineOfSight))
			{
				++RejectedByValidation;
				continue;
			}

			const float Score = CalculateCandidateScore(Profile, CandidateLocation, TargetActor, bHasLineOfSight);
			if (Score > BestScore)
			{
				BestScore = Score;
				OutLocation = CandidateLocation;
				bFoundCandidate = true;
			}
		}
	}

	if (!bFoundCandidate)
	{
		// 어느 단계에서 후보가 전멸했는지 알아야 프로필 수치를 고칠 수 있다.
		UE_LOG(LogTemp, Warning,
			TEXT("[EliteTeleport] CandidateRejection | Owner=%s | NavProjection=%d | Validation=%d | Total=%d"),
			*GetNameSafe(OwnerActor),
			RejectedByResolve,
			RejectedByValidation,
			DirectionCount * DistanceCount);
	}

	return bFoundCandidate;
}

bool UMonsterCombatPositioningComponent::ResolveCandidateLocation(
	const FVector& RawLocation,
	FVector& OutLocation) const
{
	UWorld* World = GetWorld();
	UNavigationSystemV1* NavigationSystem =
		IsValid(World) ? FNavigationSystem::GetCurrent<UNavigationSystemV1>(World) : nullptr;
	if (!IsValid(NavigationSystem))
	{
		return false;
	}

	FNavLocation ProjectedLocation;
	// Z 범위가 넓으면 위아래 층 NavMesh로 스냅되므로 좁게 잡는다.
	const FVector QueryExtent(150.0f, 150.0f, 150.0f);
	if (!NavigationSystem->ProjectPointToNavigation(RawLocation, ProjectedLocation, QueryExtent))
	{
		return false;
	}

	/*
	 * NavMesh 점은 바닥 높이다. 액터 위치는 캡슐 중심이므로 반높이만큼 올려야 파묻히지 않는다.
	 * 비행형은 여기에 비행 높이를 더해 공중에 나타나게 한다. 지상형은 0이라 결과가 같다.
	 */
	OutLocation = ProjectedLocation.Location
		+ FVector::UpVector * (GetOwnerCapsuleHalfHeight() + GroundPlacementPadding + GetOwnerHoverHeight());
	return true;
}

bool UMonsterCombatPositioningComponent::ValidateCandidate(
	const FMonsterActionPositionProfile& Profile,
	const FVector& Location,
	const AActor* TargetActor,
	bool& bOutHasLineOfSight) const
{
	bOutHasLineOfSight = false;
	AActor* OwnerActor = GetOwner();
	UWorld* World = GetWorld();
	const ACharacter* OwnerCharacter = Cast<ACharacter>(OwnerActor);
	const UCapsuleComponent* Capsule = IsValid(OwnerCharacter) ? OwnerCharacter->GetCapsuleComponent() : nullptr;
	if (!IsValid(OwnerActor) || !IsValid(World) || !IsValid(Capsule))
	{
		return false;
	}

	// 충돌 응답이나 은신 상태와 무관하게 타깃의 캡슐 주변에는 배치하지 않는다.
	const ACharacter* TargetCharacter = Cast<ACharacter>(TargetActor);
	const UCapsuleComponent* TargetCapsule = IsValid(TargetCharacter) ? TargetCharacter->GetCapsuleComponent() : nullptr;
	if (IsValid(TargetCapsule))
	{
		constexpr float TargetSeparationPadding = 20.0f;
		const float MinimumTargetDistance = Capsule->GetScaledCapsuleRadius()
			+ TargetCapsule->GetScaledCapsuleRadius() + TargetSeparationPadding;
		if (FVector::DistSquared2D(Location, TargetCapsule->GetComponentLocation())
			<= FMath::Square(MinimumTargetDistance))
		{
			return false;
		}
	}

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(MonsterTeleportValidation), false, OwnerActor);

	const FCollisionShape CapsuleShape = FCollisionShape::MakeCapsule(
		Capsule->GetScaledCapsuleRadius(),
		Capsule->GetScaledCapsuleHalfHeight());
	if (World->OverlapBlockingTestByChannel(
		Location,
		FQuat::Identity,
		Capsule->GetCollisionObjectType(),
		CapsuleShape,
		QueryParams))
	{
		return false;
	}

	// 바닥과 시야 트레이스만 타깃을 무시한다. 목적지 충돌 검사는 타깃도 포함한다.
	if (IsValid(TargetActor))
	{
		QueryParams.AddIgnoredActor(TargetActor);
	}

	FHitResult GroundHit;
	const FVector GroundStart = Location + FVector::UpVector * 100.0f;
	const FVector GroundEnd = GroundStart - FVector::UpVector * GroundTraceDistance;
	if (!World->LineTraceSingleByChannel(GroundHit, GroundStart, GroundEnd, ECC_Visibility, QueryParams))
	{
		return false;
	}

	const float SlopeDegrees = FMath::RadiansToDegrees(FMath::Acos(FVector::DotProduct(GroundHit.ImpactNormal, FVector::UpVector)));
	if (SlopeDegrees > Profile.MaximumFloorSlopeDegrees)
	{
		return false;
	}

	// 위아래 층으로 순간이동하는 것을 막는다.
	if (MaximumHeightDifference > 0.0f
		&& FMath::Abs(Location.Z - OwnerActor->GetActorLocation().Z) > MaximumHeightDifference)
	{
		return false;
	}

	const float Clearance = CalculateClearance(Location, TargetActor);
	if (Clearance < Profile.MinimumClearance)
	{
		return false;
	}

	/*
	 * 최근 위치와 짧은 이동은 여기서 걸러내지 않는다. 둘 다 연출상의 취향이라
	 * CalculateCandidateScore의 감점으로 다룬다. 하드 필터로 두면 갈 곳이 좁을 때
	 * 후보가 전멸해 패턴 자체가 실패한다.
	 */

	// TODO: CombatAreaVolume으로 현재 전투 구역 안인지 검사한다.
	// TODO: KillVolume / NoTeleportVolume으로 금지 구역을 검사한다.
	// TODO: 플레이어 NavMesh 기준으로 후보 위치까지 실제 도달 가능한지 검사한다.

	if (IsValid(TargetActor))
	{
		const FVector Start = Location + FVector::UpVector * (Capsule->GetScaledCapsuleHalfHeight() * 0.5f);
		const FVector End = TargetActor->GetActorLocation() + FVector::UpVector * 50.0f;
		bOutHasLineOfSight = !World->LineTraceTestByChannel(Start, End, ECC_Visibility, QueryParams);
	}

	return !Profile.bRequireLineOfSight || bOutHasLineOfSight;
}

float UMonsterCombatPositioningComponent::CalculateCandidateScore(
	const FMonsterActionPositionProfile& Profile,
	const FVector& Location,
	const AActor* TargetActor,
	const bool bHasLineOfSight) const
{
	const float Distance = FVector::Dist2D(Location, TargetActor->GetActorLocation());
	const float RangeCenter = (Profile.DesiredMinDistance + Profile.DesiredMaxDistance) * 0.5f;
	const float RangeHalfWidth = FMath::Max((Profile.DesiredMaxDistance - Profile.DesiredMinDistance) * 0.5f, 1.0f);
	const float DistanceUtility = 1.0f - FMath::Clamp(FMath::Abs(Distance - RangeCenter) / RangeHalfWidth, 0.0f, 1.0f);
	const float DirectionUtility = CalculateDirectionUtility(Profile.DirectionPreference, Location, TargetActor);
	const float ClearanceUtility = FMath::Clamp(
		(CalculateClearance(Location, TargetActor) - Profile.MinimumClearance)
		/ FMath::Max(Profile.IdealClearance - Profile.MinimumClearance, 1.0f),
		0.0f,
		1.0f);

	float RecentPenalty = 0.0f;
	if (Profile.RecentPositionRadius > 0.0f)
	{
		for (const FVector& RecentLocation : RecentTeleportLocations)
		{
			const float DistanceToRecent = FVector::Dist2D(Location, RecentLocation);
			RecentPenalty = FMath::Max(RecentPenalty, 1.0f - FMath::Clamp(DistanceToRecent / Profile.RecentPositionRadius, 0.0f, 1.0f));
		}
	}

	/*
	 * 목표 이동 거리를 넘긴 후보는 이 항목에서 모두 동점이 된다. "최대한 멀리"가
	 * 아니라 "이만큼은 움직여라"에 가깝고, 넘긴 뒤로는 다른 가중치가 승부를 본다.
	 */
	float ShortRepositionUtility = 0.0f;
	if (Profile.MinimumRepositionDistance > 0.0f)
	{
		if (const AActor* OwnerActor = GetOwner())
		{
			const float TravelDistance = FVector::Dist2D(Location, OwnerActor->GetActorLocation());
			ShortRepositionUtility =
				1.0f - FMath::Clamp(TravelDistance / Profile.MinimumRepositionDistance, 0.0f, 1.0f);
		}
	}

	return DistanceUtility * Profile.DistanceWeight
		+ DirectionUtility * Profile.DirectionWeight
		+ (bHasLineOfSight ? Profile.LineOfSightWeight : 0.0f)
		+ ClearanceUtility * Profile.ClearanceWeight
		- RecentPenalty * Profile.RecentPositionPenalty
		- ShortRepositionUtility * Profile.ShortRepositionPenalty;
}

float UMonsterCombatPositioningComponent::CalculateClearance(const FVector& Location, const AActor* TargetActor) const
{
	const AActor* OwnerActor = GetOwner();
	const UWorld* World = GetWorld();
	const ACharacter* OwnerCharacter = Cast<ACharacter>(OwnerActor);
	const UCapsuleComponent* Capsule = IsValid(OwnerCharacter) ? OwnerCharacter->GetCapsuleComponent() : nullptr;
	if (!IsValid(OwnerActor) || !IsValid(World) || !IsValid(Capsule))
	{
		return 0.0f;
	}

	constexpr float ProbeDistance = 500.0f;
	float NearestClearance = ProbeDistance;
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(MonsterTeleportClearance), false, OwnerActor);
	if (IsValid(TargetActor))
	{
		QueryParams.AddIgnoredActor(TargetActor);
	}

	const FCollisionShape CapsuleShape = FCollisionShape::MakeCapsule(
		Capsule->GetScaledCapsuleRadius(),
		Capsule->GetScaledCapsuleHalfHeight());

	for (int32 Index = 0; Index < 8; ++Index)
	{
		const FVector Direction = FVector::ForwardVector.RotateAngleAxis(45.0f * Index, FVector::UpVector);
		FHitResult Hit;
		if (World->SweepSingleByChannel(
			Hit,
			Location,
			Location + Direction * ProbeDistance,
			FQuat::Identity,
			Capsule->GetCollisionObjectType(),
			CapsuleShape,
			QueryParams))
		{
			NearestClearance = FMath::Min(NearestClearance, Hit.Distance);
		}
	}

	return NearestClearance;
}

float UMonsterCombatPositioningComponent::CalculateDirectionUtility(
	const EMonsterPositioningDirectionPreference Preference,
	const FVector& Location,
	const AActor* TargetActor) const
{
	if (Preference == EMonsterPositioningDirectionPreference::Any || !IsValid(TargetActor))
	{
		return 1.0f;
	}

	const FVector TargetForward = TargetActor->GetActorForwardVector().GetSafeNormal2D();
	const FVector TargetToCandidate = (Location - TargetActor->GetActorLocation()).GetSafeNormal2D();
	const float ForwardDot = FVector::DotProduct(TargetForward, TargetToCandidate);

	switch (Preference)
	{
	case EMonsterPositioningDirectionPreference::Front:
		return (ForwardDot + 1.0f) * 0.5f;
	case EMonsterPositioningDirectionPreference::Side:
		return 1.0f - FMath::Abs(ForwardDot);
	case EMonsterPositioningDirectionPreference::Rear:
		return (1.0f - ForwardDot) * 0.5f;
	default:
		return 1.0f;
	}
}

float UMonsterCombatPositioningComponent::GetOwnerCapsuleHalfHeight() const
{
	const ACharacter* OwnerCharacter = Cast<ACharacter>(GetOwner());
	const UCapsuleComponent* Capsule = IsValid(OwnerCharacter) ? OwnerCharacter->GetCapsuleComponent() : nullptr;
	return IsValid(Capsule) ? Capsule->GetScaledCapsuleHalfHeight() : 0.0f;
}

float UMonsterCombatPositioningComponent::GetOwnerHoverHeight() const
{
	// 순찰 지점에 쓰는 높이와 같은 기준이다. 비행형만 이동 프로필의 비행 높이를 돌려준다.
	const ACommonEnemyCharacter* EnemyCharacter = Cast<ACommonEnemyCharacter>(GetOwner());
	return IsValid(EnemyCharacter) ? FMath::Max(EnemyCharacter->GetPatrolHeightOffset(), 0.0f) : 0.0f;
}

UAbilitySystemComponent* UMonsterCombatPositioningComponent::GetOwnerAbilitySystemComponent() const
{
	const IAbilitySystemInterface* AbilitySystemInterface = Cast<IAbilitySystemInterface>(GetOwner());
	return AbilitySystemInterface != nullptr ? AbilitySystemInterface->GetAbilitySystemComponent() : nullptr;
}

EEnemyPatternPreparationStatus UMonsterCombatPositioningComponent::ResolveRepositionResult() const
{
	return bLastRepositionSuccessful
		? EEnemyPatternPreparationStatus::Ready
		: EEnemyPatternPreparationStatus::Failed;
}

void UMonsterCombatPositioningComponent::FinalizeTeleport()
{
	ACharacter* OwnerCharacter = Cast<ACharacter>(GetOwner());
	UMonsterMovementComponent* MovementComponent =
		IsValid(OwnerCharacter)
			? Cast<UMonsterMovementComponent>(OwnerCharacter->GetCharacterMovement())
			: nullptr;
	if (!IsValid(MovementComponent))
	{
		return;
	}

	/*
	 * AIController의 bMovementLocked는 새 이동 요청만 막는다. 이미 발급된 경로와
	 * 잔여 속도는 남아 있어서, 정리하지 않으면 도착 직후 미끄러지거나 잠금이 풀린 뒤
	 * 이전 목적지로 되돌아간다.
	 */
	MovementComponent->ResetMovementState();
	MovementComponent->StopMovementImmediately();

	// 지상형은 낙하 중에 옮겨졌어도 바로 걷게 하고, 비행형은 비행 모드를 그대로 둔다.
	if (!MovementComponent->IsFlying())
	{
		MovementComponent->SetMovementMode(MOVE_Walking);
	}
}

void UMonsterCombatPositioningComponent::RememberTeleportLocation(const FVector& Location)
{
	RecentTeleportLocations.Add(Location);
	constexpr int32 MaxRememberedLocations = 3;
	if (RecentTeleportLocations.Num() > MaxRememberedLocations)
	{
		RecentTeleportLocations.RemoveAt(0, RecentTeleportLocations.Num() - MaxRememberedLocations, EAllowShrinking::No);
	}
}
