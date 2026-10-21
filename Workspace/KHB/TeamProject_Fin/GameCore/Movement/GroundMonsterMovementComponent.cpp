#include "GroundMonsterMovementComponent.h"

#include "AIController.h"
#include "Engine/World.h"
#include "Engine/OverlapResult.h"
#include "GameFramework/Character.h"
#include "EnemyJumpNavLink.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "MotionWarpingComponent.h"
#include "GameCore/Character/CommonEnemyCharacter.h"
#include "GameCore/Tags/GameplayTagSystem.h"
#include "AbilitySystemComponent.h"
#include "EngineUtils.h"
#include "AnimNotifyState_NavLinkFlight.h"
#include "AnimNotifyState_MotionWarping.h"
#include "RootMotionModifier.h"
#include "NavigationSystem.h"
#include "Navigation/PathFollowingComponent.h"
#include "GameCore/AI/MonsterAIController.h"

void UGroundMonsterMovementComponent::HandleImpact(const FHitResult& Hit, float TimeSlice, const FVector& MoveDelta)
{
	LastMovementImpact = Hit;
	LastMovementImpactTime = GetWorld() ? GetWorld()->GetTimeSeconds() : -1.0;
	Super::HandleImpact(Hit, TimeSlice, MoveDelta);
}

void UGroundMonsterMovementComponent::UpdateSplitMovementBlockers()
{
	if (!CharacterOwner || !CharacterOwner->HasAuthority()) return;
	UCapsuleComponent* Capsule = CharacterOwner->GetCapsuleComponent();
	for (int32 Index = SplitMovementBlockers.Num() - 1; Index >= 0; --Index)
	{
		ACommonEnemyCharacter* Other = SplitMovementBlockers[Index].Get();
		if (!Other) { SplitMovementBlockers.RemoveAtSwap(Index); continue; }
		const float Spacing = Capsule->GetScaledCapsuleRadius() + Other->GetCapsuleComponent()->GetScaledCapsuleRadius() + 30.f;
		if (!CharacterOwner->IsHidden() && !Other->HasStartedDeath() && !Other->IsHidden() &&
			FVector::DistSquared2D(CharacterOwner->GetActorLocation(), Other->GetActorLocation()) < FMath::Square(Spacing)) continue;
		const auto* OtherMovement = Cast<UGroundMonsterMovementComponent>(Other->GetCharacterMovement());
		if (bNavLinkFlight || (OtherMovement && OtherMovement->bNavLinkFlight)) continue;
		Capsule->IgnoreActorWhenMoving(Other, false);
		SplitMovementBlockers.RemoveAtSwap(Index);
	}
}

void UGroundMonsterMovementComponent::RecoverStoppedSplitMovement()
{
	if (!CharacterOwner || !CharacterOwner->HasAuthority() || !GetWorld()) return;
	const double Now = GetWorld()->GetTimeSeconds();
	if (Now < NextSplitRecoveryTime) return;
	const FVector Position = CharacterOwner->GetActorLocation();
	const float Travel = FVector::Dist(Position, LastSplitRecoveryLocation);
	LastSplitRecoveryLocation = Position;
	NextSplitRecoveryTime = Now + 2.0;
	ACommonEnemyCharacter* Enemy = Cast<ACommonEnemyCharacter>(CharacterOwner);
	const AMonsterAIController* Controller = Cast<AMonsterAIController>(CharacterOwner->GetController());
	AActor* Target = Controller ? Controller->GetPerceivedTarget() : nullptr;
	if (!Enemy || Enemy->HasStartedDeath() || Enemy->IsHidden() || !IsValid(Target) || Travel > 10.f) return;
	const UPathFollowingComponent* PathFollowing = Controller->GetPathFollowingComponent();
	const UCapsuleComponent* Capsule = CharacterOwner->GetCapsuleComponent();
	ACommonEnemyCharacter* Blocker = Cast<ACommonEnemyCharacter>(LastMovementImpact.GetActor());
	if (PathFollowing && PathFollowing->GetStatus() == EPathFollowingStatus::Moving &&
		!bNavLinkJumpActive && !IsPatternMovementActive() && Now - LastMovementImpactTime < 0.25 &&
		Blocker && Blocker != Enemy && Enemy->GetSharedHealthPool() &&
		Enemy->GetSharedHealthPool() == Blocker->GetSharedHealthPool() &&
		LastMovementImpact.GetComponent() == Blocker->GetCapsuleComponent())
	{
		auto* OtherMovement = Cast<UGroundMonsterMovementComponent>(Blocker->GetCharacterMovement());
		if (OtherMovement && !Capsule->GetMoveIgnoreActors().Contains(Blocker) &&
			!Blocker->GetCapsuleComponent()->GetMoveIgnoreActors().Contains(CharacterOwner))
		{
			CharacterOwner->GetCapsuleComponent()->IgnoreActorWhenMoving(Blocker, true);
			Blocker->GetCapsuleComponent()->IgnoreActorWhenMoving(CharacterOwner, true);
			SplitMovementBlockers.AddUnique(Blocker);
			OtherMovement->SplitMovementBlockers.AddUnique(Enemy);
		}
	}
}

UGroundMonsterMovementComponent::UGroundMonsterMovementComponent()
{
	SetIsReplicatedByDefault(true);
}

void UGroundMonsterMovementComponent::BeginPlay()
{
	Super::BeginPlay();
	if (ACommonEnemyCharacter* Enemy = Cast<ACommonEnemyCharacter>(GetOwner()))
	{
		Enemy->OnDeathStarted.AddUObject(this, &ThisClass::HandleNavLinkOwnerDeath);
	}
}

void UGroundMonsterMovementComponent::HandleNavLinkOwnerDeath(ABaseCharacter* Character)
{
	CancelNavLinkJump();
}

bool UGroundMonsterMovementComponent::TryStartNavLinkJump(AEnemyJumpNavLink* Link, const FVector& Destination)
{
	ACommonEnemyCharacter* Enemy = Cast<ACommonEnemyCharacter>(CharacterOwner);
	UAnimInstance* Anim = Enemy && Enemy->GetMesh() ? Enemy->GetMesh()->GetAnimInstance() : nullptr;
	UAnimMontage* Montage = Enemy && Destination.Z >= Enemy->GetActorLocation().Z -
														  Enemy->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()
								? JumpUpMontage.Get()
								: JumpDownMontage.Get();
	UAbilitySystemComponent* ASC = Enemy ? Enemy->GetAbilitySystemComponent() : nullptr;
	if (!Enemy || !Enemy->HasAuthority() || Enemy->HasStartedDeath() || !IsValid(Link) || bNavLinkJumpActive ||
		IsFalling() || IsPatternMovementActive() || !Anim || !Montage || !Montage->HasRootMotion() ||
		!Enemy->FindComponentByClass<UMotionWarpingComponent>() ||
		(ASC && ASC->HasMatchingGameplayTag(GameplayTagSystem::State_AI_ActionCommitted)))
	{
		return false;
	}
	bool bHasFlightWindow = false;
	bool bHasWarpWindow = false;
	for (const FAnimNotifyEvent& Notify : Montage->Notifies)
	{
		bHasFlightWindow |= Notify.NotifyStateClass && Notify.NotifyStateClass->IsA<UAnimNotifyState_NavLinkFlight>();
		if (const UAnimNotifyState_MotionWarping* Window =
				Cast<UAnimNotifyState_MotionWarping>(Notify.NotifyStateClass))
		{
			const URootMotionModifier_Warp* Modifier = Cast<URootMotionModifier_Warp>(Window->RootMotionModifier);
			bHasWarpWindow |= Modifier && Modifier->WarpTargetName == JumpWarpTargetName &&
							  Modifier->bWarpTranslation && !Modifier->bIgnoreZAxis;
		}
	}
	if (!bHasFlightWindow || !bHasWarpWindow)
	{
		return false;
	}
	ActiveJumpLink = Link;
	ActiveJumpMontage = Montage;
	JumpFloorDestination = Destination;
	bNavLinkJumpActive = true;
	bJumpMontageStarted = false;
	bFlightNotifyReceived = false;

	NextLandingSearchTime = 0.0;

	JumpDeadline = GetWorld()->GetTimeSeconds() + LandingWaitTimeout;
	StopMovementImmediately();
	// 다음 공격 패턴이 도약 몽타주를 덮어쓰지 않도록 기존 행동 점유 태그를 사용합니다.
	if (ASC)
	{
		ASC->AddLooseGameplayTag(GameplayTagSystem::State_AI_ActionCommitted);
		bOwnsJumpActionTag = true;
	}
	return true;
}

bool UGroundMonsterMovementComponent::FindNavLinkLanding(FVector& OutLocation)
{
	if (ValidateNavLinkLanding(JumpFloorDestination, OutLocation))
	{
		return true;
	}

	UNavigationSystemV1* Navigation = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	if (Navigation)
	{
		FVector Forward = (JumpFloorDestination - CharacterOwner->GetActorLocation()).GetSafeNormal2D();
		if (Forward.IsNearlyZero())
		{
			Forward = CharacterOwner->GetActorForwardVector().GetSafeNormal2D();
		}
		const float Diameter = CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleRadius() * 2.0f;
		const float Angles[] = {90.0f, -90.0f, 0.0f, 180.0f, 45.0f, -45.0f, 135.0f, -135.0f};
		// 옆쪽부터 빈 공간을 찾고, 먼 후보도 원래 착지점에서 6m 안으로 제한합니다.
		for (int32 Ring = 1; Ring <= 2; ++Ring)
		{
			const float Distance = FMath::Min((Diameter + 25.0f) * Ring, 600.0f);
			for (const float Angle : Angles)
			{
				const FVector Candidate = JumpFloorDestination + Forward.RotateAngleAxis(Angle, FVector::UpVector) * Distance;
				FNavLocation Projected;
				if (!Navigation->ProjectPointToNavigation(Candidate, Projected, FVector(30.0f, 30.0f, 50.0f)) ||
					FMath::Abs(Projected.Location.Z - JumpFloorDestination.Z) > 60.0f)
				{
					continue;
				}
				FVector NavHit;
				// 목적지 층 안에서 직선으로 연결되지 않으면 턱이나 다른 구역의 후보입니다.
				if (UNavigationSystemV1::NavigationRaycast(GetWorld(), JumpFloorDestination, Projected.Location,
														 NavHit, {}, CharacterOwner->GetController()) ||
					!ValidateNavLinkLanding(Projected.Location, OutLocation))
				{
					continue;
				}

				return true;
			}
		}
	}
	return false;
}

bool UGroundMonsterMovementComponent::ValidateNavLinkLanding(const FVector& FloorLocation, FVector& OutLocation)
{
	const UCapsuleComponent* Capsule = CharacterOwner->GetCapsuleComponent();
	const float Radius = Capsule->GetScaledCapsuleRadius();
	const float HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
	FCollisionQueryParams Params(SCENE_QUERY_STAT(NavLinkLanding), false, CharacterOwner);
	FCollisionQueryParams FloorParams = Params;
	for (TActorIterator<ACharacter> It(GetWorld()); It; ++It)
	{
		// 캐릭터의 몸을 바닥으로 읽지 않고, 착지 공간은 이동용 캡슐로 검사합니다.
		FloorParams.AddIgnoredActor(*It);
		if (It->GetMesh())
		{
			Params.AddIgnoredComponent(It->GetMesh());
		}
	}
	FCollisionObjectQueryParams Objects;
	Objects.AddObjectTypesToQuery(ECC_WorldStatic);
	Objects.AddObjectTypesToQuery(ECC_WorldDynamic);
	FHitResult Floor;
	if (!GetWorld()->LineTraceSingleByObjectType(Floor, FloorLocation + FVector(0, 0, 100),
												 FloorLocation - FVector(0, 0, 150), Objects, FloorParams))
	{
		return false;
	}
	if (FMath::Abs(Floor.ImpactPoint.Z - JumpFloorDestination.Z) > 75.0f)
	{
		return false;
	}
	// 캡슐 가장자리가 다른 층이나 허공에 걸리는 후보를 제외합니다.
	const FVector SupportOffsets[] = {
		FVector(Radius + 10.0f, 0, 0), FVector(-Radius - 10.0f, 0, 0),
		FVector(0, Radius + 10.0f, 0), FVector(0, -Radius - 10.0f, 0)};
	for (const FVector& Offset : SupportOffsets)
	{
		const FVector SupportPoint = Floor.ImpactPoint + Offset;
		FHitResult Support;
		if (!GetWorld()->LineTraceSingleByObjectType(Support, SupportPoint + FVector(0, 0, 50),
			SupportPoint - FVector(0, 0, 60), Objects, FloorParams) || !IsWalkable(Support) ||
			FMath::Abs(Support.ImpactPoint.Z - Floor.ImpactPoint.Z) > 50.0f)
		{
			return false;
		}
	}
	if (!IsWalkable(Floor))
	{
		return false;
	}
	OutLocation = Floor.ImpactPoint + FVector(0, 0, HalfHeight + 3.0f);
	TArray<FOverlapResult> Overlaps;
	// BP의 Custom 프로필은 등록된 프로필 이름이 아니므로 실제 캡슐 응답을 직접 사용합니다.
	GetWorld()->OverlapMultiByChannel(Overlaps, OutLocation, FQuat::Identity, Capsule->GetCollisionObjectType(),
		FCollisionShape::MakeCapsule(Radius, HalfHeight), Params,
		FCollisionResponseParams(Capsule->GetCollisionResponseToChannels()));
	for (const FOverlapResult& Overlap : Overlaps)
	{
		if (Overlap.bBlockingHit)
		{
			return false;
		}
	}
	// 서로 다른 링크를 사용해도 동일한 착지 공간을 동시에 예약하지 않습니다.
	for (TActorIterator<ACommonEnemyCharacter> It(GetWorld()); It; ++It)
	{
		if (*It == CharacterOwner || It->IsHidden() || It->HasStartedDeath())
		{
			continue;
		}
		const UGroundMonsterMovementComponent* Other =
			Cast<UGroundMonsterMovementComponent>(It->GetCharacterMovement());
		const float Spacing = Radius + It->GetCapsuleComponent()->GetScaledCapsuleRadius() + 10.0f;
		const bool bOverlapsActor =
			FVector::DistSquared2D(OutLocation, It->GetActorLocation()) < FMath::Square(Spacing) &&
			FMath::Abs(OutLocation.Z - It->GetActorLocation().Z) <
				HalfHeight + It->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
		const bool bReserved = Other && Other->bJumpMontageStarted && Other->bNavLinkJumpActive &&
							   FVector::DistSquared2D(OutLocation, Other->JumpLandingLocation) < FMath::Square(Spacing) &&
							   FMath::Abs(OutLocation.Z - Other->JumpLandingLocation.Z) <
								   HalfHeight + It->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
		if (bOverlapsActor || bReserved)
		{
			return false;
		}
	}
	return true;
}

void UGroundMonsterMovementComponent::TickComponent(float DeltaTime, ELevelTick TickType,
													FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	UpdateSplitMovementBlockers();
	RecoverStoppedSplitMovement();
	if (!bNavLinkJumpActive || !CharacterOwner || !CharacterOwner->HasAuthority())
	{
		return;
	}
	ACommonEnemyCharacter* Enemy = Cast<ACommonEnemyCharacter>(CharacterOwner);
	if (!ActiveJumpLink.IsValid() || !Enemy || Enemy->HasStartedDeath() || Enemy->IsHidden() ||
		MovementMode == MOVE_None)
	{
		FinishNavLinkJump(false);
		return;
	}
	if (GetWorld()->GetTimeSeconds() >= JumpDeadline)
	{
		FinishNavLinkJump(false);
		return;
	}
	if (!bJumpMontageStarted)
	{
		if (GetWorld()->GetTimeSeconds() < NextLandingSearchTime)
		{
			return;
		}
		NextLandingSearchTime = GetWorld()->GetTimeSeconds() + 0.1;
		if (FindNavLinkLanding(JumpLandingLocation))
		{
			// 워핑 전 근사 궤적으로 취소하지 않고 실제 루트 이동의 충돌 스윕으로 벽을 처리합니다.
			bJumpMontageStarted = true;

			JumpDeadline = GetWorld()->GetTimeSeconds() + ActiveJumpMontage->GetPlayLength() + 2.0f;
			FRotator Facing = (JumpLandingLocation - Enemy->GetActorLocation()).Rotation();
			Facing.Pitch = Facing.Roll = 0.0f;
			MulticastStartNavLinkMontage(ActiveJumpMontage.Get(), JumpLandingLocation, Facing);
		}
		return;
	}
	if (bNavLinkFlight)
	{
		IgnoreOtherMonsters();
	}
	UAnimInstance* Anim = Enemy->GetMesh()->GetAnimInstance();
	if (!Anim || !Anim->Montage_IsPlaying(ActiveJumpMontage.Get()))
	{
		if (!bFlightNotifyReceived)
		{
			FinishNavLinkJump(false);
		}
		else if (IsMovingOnGround())
		{
			FinishNavLinkJump(FVector::Dist(Enemy->GetActorLocation(), JumpLandingLocation) <= LandingTolerance);
		}
	}
}

void UGroundMonsterMovementComponent::MulticastStartNavLinkMontage_Implementation(UAnimMontage* Montage,
																				  FVector LandingLocation,
																				  FRotator LandingRotation)
{
	if (!CharacterOwner || !CharacterOwner->GetMesh() || CharacterOwner->IsHidden())
	{
		return;
	}
	UAnimInstance* Anim = CharacterOwner->GetMesh()->GetAnimInstance();
	UMotionWarpingComponent* Warping = CharacterOwner->FindComponentByClass<UMotionWarpingComponent>();
	if (!Anim || !Warping)
	{
		return;
	}
	ActiveJumpMontage = Montage;
	bJumpPresentationStarted = true;
	bNavLinkJumpActive = true;
	bSavedOrientToMovement = bOrientRotationToMovement;
	bOrientRotationToMovement = false;
	// 상하 이동에서도 도약 방향을 먼저 맞춰 베이크된 루트 이동이 옆으로 나가지 않게 합니다.
	CharacterOwner->SetActorRotation(LandingRotation);
	SavedRootMotionMode = static_cast<uint8>(Anim->RootMotionMode.GetValue());
	Anim->SetRootMotionMode(ERootMotionMode::RootMotionFromMontagesOnly);
	FVector WarpLocation = LandingLocation;
	for (const FAnimNotifyEvent& Notify : Montage->Notifies)
	{
		const UAnimNotifyState_MotionWarping* Window = Cast<UAnimNotifyState_MotionWarping>(Notify.NotifyStateClass);
		const URootMotionModifier_Warp* Modifier =
			Window ? Cast<URootMotionModifier_Warp>(Window->RootMotionModifier) : nullptr;
		if (Modifier && Modifier->WarpTargetName == JumpWarpTargetName)
		{
			// 발 위치 워핑과 액터 중심 워핑을 모두 지원합니다.
			if (Modifier->bWarpToFeetLocation)
			{
				WarpLocation.Z -= CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
			}
			break;
		}
	}
	Warping->AddOrUpdateWarpTargetFromLocationAndRotation(JumpWarpTargetName, WarpLocation, LandingRotation);
	Anim->Montage_Play(Montage);
}

void UGroundMonsterMovementComponent::BeginNavLinkFlight()
{
	if (!bNavLinkJumpActive)
	{
		return;
	}
	bFlightNotifyReceived = true;
	bNavLinkFlight = true;
	// 걷기와 낙하 모드의 루트 이동 높이 제한을 피하기 위해 비행 구간에만 전환합니다.
	SetMovementMode(MOVE_Flying);
	IgnoreOtherMonsters();
}

void UGroundMonsterMovementComponent::EndNavLinkFlight()
{
	if (bNavLinkJumpActive && bNavLinkFlight)
	{
		bNavLinkFlight = false;
		if (MovementMode == MOVE_Flying)
		{
			SetMovementMode(MOVE_Falling);
		}
	}
}

void UGroundMonsterMovementComponent::IgnoreOtherMonsters()
{
	UCapsuleComponent* Capsule = CharacterOwner->GetCapsuleComponent();
	for (TActorIterator<ACommonEnemyCharacter> It(GetWorld()); It; ++It)
	{
		if (*It == CharacterOwner)
		{
			continue;
		}
		UCapsuleComponent* OtherCapsule = It->GetCapsuleComponent();
		if (!Capsule->GetMoveIgnoreActors().Contains(*It))
		{
			Capsule->IgnoreActorWhenMoving(*It, true);
			IgnoredMonsters.Add(*It);
		}
		if (!OtherCapsule->GetMoveIgnoreActors().Contains(CharacterOwner))
		{
			OtherCapsule->IgnoreActorWhenMoving(CharacterOwner, true);
			MonstersIgnoringOwner.Add(*It);
		}
	}
}

void UGroundMonsterMovementComponent::RestoreNavLinkPresentation(bool bAbort)
{
	if (!CharacterOwner)
	{
		return;
	}
	if (bJumpPresentationStarted)
	{
		if (UMotionWarpingComponent* Warping = CharacterOwner->FindComponentByClass<UMotionWarpingComponent>())
		{
			Warping->RemoveWarpTarget(JumpWarpTargetName);
		}
		if (UAnimInstance* Anim = CharacterOwner->GetMesh()->GetAnimInstance())
		{
			if (bAbort && ActiveJumpMontage.IsValid())
			{
				Anim->Montage_Stop(0.1f, ActiveJumpMontage.Get());
			}
			Anim->SetRootMotionMode(static_cast<ERootMotionMode::Type>(SavedRootMotionMode));
		}
		bOrientRotationToMovement = bSavedOrientToMovement;
	}
	for (const auto& Other : IgnoredMonsters)
	{
		if (Other.IsValid())
		{
			ACharacter* OtherCharacter = Cast<ACharacter>(Other.Get());
			UGroundMonsterMovementComponent* OtherMovement =
				OtherCharacter ? Cast<UGroundMonsterMovementComponent>(OtherCharacter->GetCharacterMovement())
							   : nullptr;
			if (OtherMovement && OtherMovement->bNavLinkFlight)
			{
				OtherMovement->MonstersIgnoringOwner.AddUnique(CharacterOwner);
			}
			else
			{
				CharacterOwner->GetCapsuleComponent()->IgnoreActorWhenMoving(Other.Get(), false);
			}
		}
	}
	for (const auto& Other : MonstersIgnoringOwner)
	{
		if (ACharacter* Character = Cast<ACharacter>(Other.Get()))
		{
			UGroundMonsterMovementComponent* OtherMovement =
				Cast<UGroundMonsterMovementComponent>(Character->GetCharacterMovement());
			if (OtherMovement && OtherMovement->bNavLinkFlight)
			{
				OtherMovement->IgnoredMonsters.AddUnique(CharacterOwner);
			}
			else
			{
				Character->GetCapsuleComponent()->IgnoreActorWhenMoving(CharacterOwner, false);
			}
		}
	}
	IgnoredMonsters.Empty();
	MonstersIgnoringOwner.Empty();
	if (MovementMode == MOVE_Flying)
	{
		SetMovementMode(MOVE_Falling);
	}
	bNavLinkJumpActive = bNavLinkFlight = bJumpMontageStarted = bFlightNotifyReceived = false;
	bJumpPresentationStarted = false;
	ActiveJumpMontage.Reset();
}

void UGroundMonsterMovementComponent::MulticastFinishNavLinkMontage_Implementation(bool bAbort)
{
	RestoreNavLinkPresentation(bAbort);
}

void UGroundMonsterMovementComponent::FinishNavLinkJump(bool bSuccess)
{
	AEnemyJumpNavLink* Link = ActiveJumpLink.Get();
	ActiveJumpLink.Reset();
	if (bOwnsJumpActionTag)
	{
		if (ACommonEnemyCharacter* Enemy = Cast<ACommonEnemyCharacter>(CharacterOwner))
		{
			if (UAbilitySystemComponent* ASC = Enemy->GetAbilitySystemComponent())
			{
				ASC->RemoveLooseGameplayTag(GameplayTagSystem::State_AI_ActionCommitted);
			}
		}
		bOwnsJumpActionTag = false;
	}
	MulticastFinishNavLinkMontage(!bSuccess);
	if (Link && CharacterOwner)
	{
		Link->ResumePathFollowing(CharacterOwner);
	}
	if (!bSuccess && CharacterOwner)
	{
		if (AAIController* Controller = Cast<AAIController>(CharacterOwner->GetController()))
		{
			Controller->StopMovement();
		}
	}
	RequestedTarget.Reset();
	NextRequestTime = 0.0;
}

void UGroundMonsterMovementComponent::CancelNavLinkJump()
{
	if (bNavLinkJumpActive)
	{
		if (CharacterOwner && CharacterOwner->HasAuthority())
		{
			FinishNavLinkJump(false);
		}
		else
		{
			RestoreNavLinkPresentation(true);
		}
	}
}

void UGroundMonsterMovementComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	CancelNavLinkJump();
	for (const auto& Blocker : SplitMovementBlockers)
	{
		if (Blocker.IsValid() && CharacterOwner)
		{
			CharacterOwner->GetCapsuleComponent()->IgnoreActorWhenMoving(Blocker.Get(), false);
			Blocker->GetCapsuleComponent()->IgnoreActorWhenMoving(CharacterOwner, false);
		}
	}
	SplitMovementBlockers.Empty();
	if (ACommonEnemyCharacter* Enemy = Cast<ACommonEnemyCharacter>(GetOwner()))
	{
		Enemy->OnDeathStarted.RemoveAll(this);
	}
	Super::EndPlay(Reason);
}

void UGroundMonsterMovementComponent::UpdateMovementTowardTarget(AActor* InTargetActor, const float DeltaSeconds)
{
	// 패턴 이동 중에는 이동을 패턴이 맡는다. 추격의 MoveToActor가 패턴 경로를 덮어쓰지 않게 한다.
	if (IsPatternMovementActive() || bNavLinkJumpActive)
	{
		return;
	}

	ACharacter* Character = GetCharacterOwner();
	AAIController* Controller = Character ? Cast<AAIController>(Character->GetController()) : nullptr;
	if (!Controller || !IsValid(InTargetActor))
	{
		ResetMovementState();
		return;
	}

	if (bUseChaseSlots)
	{
		FVector SlotGoal;
		if (!ResolveChaseSlotGoal(InTargetActor, false, SlotGoal) || bChaseSlotHolding)
		{
			Controller->StopMovement();
			StopMovementImmediately();
			RequestedTarget.Reset();
			NextRequestTime = 0.0;
			FVector Facing = InTargetActor->GetActorLocation() - Character->GetActorLocation();
			Facing.Z = 0.f;
			if (!Facing.IsNearlyZero())
			{
				Character->SetActorRotation(FMath::RInterpTo(Character->GetActorRotation(), Facing.Rotation(), DeltaSeconds, 5.f));
			}
			return;
		}
		if (RequestedTarget.Get() != InTargetActor || CanRefreshRequest(SlotGoal))
		{
			// 부정확한 partial path를 도착으로 인정하지 않는다.
			Controller->MoveToLocation(SlotGoal, ChaseSlotAcceptanceRadius, false, true, true, true, nullptr, false);
			RequestedTarget = InTargetActor;
			RecordRequest(SlotGoal);
		}
		return;
	}
	ReleaseChaseSlot();
	const FVector Destination = InTargetActor->GetActorLocation();
	if (RequestedTarget.Get() != InTargetActor || CanRefreshRequest(Destination))
	{
		const ACharacter* TargetCharacter = Cast<ACharacter>(InTargetActor);
		const FVector OwnerFeet = Character->GetActorLocation() - FVector(0, 0, Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
		const FVector TargetFeet = TargetCharacter
			? Destination - FVector(0, 0, TargetCharacter->GetCapsuleComponent()->GetScaledCapsuleHalfHeight())
			: Destination;
		if (FMath::Abs(TargetFeet.Z - OwnerFeet.Z) > MaxStepHeight + 20.f)
		{
			// MoveToActor의 목표 캡슐 높이/반경이 턱 반대쪽도 도착으로 판정하지 않게 합니다.
			Controller->MoveToLocation(TargetFeet,
				25.f, false, true, true, true, nullptr, true);
		}
		else
		{
			Controller->MoveToActor(InTargetActor,
				AcceptanceRadius, true, true, true, nullptr, true);
		}
		RequestedTarget = InTargetActor;
		RecordRequest(Destination);
	}
}

void UGroundMonsterMovementComponent::UpdateMovementTowardLocation(const FVector& Destination, const float DeltaSeconds)
{
	ReleaseChaseSlot();
	if (bNavLinkJumpActive)
	{
		return;
	}
	ACharacter* Character = GetCharacterOwner();
	AAIController* Controller = Character ? Cast<AAIController>(Character->GetController()) : nullptr;
	if (!Controller)
	{
		return;
	}

	if (!RequestedTarget.IsValid() && !CanRefreshRequest(Destination))
	{
		return;
	}

	RequestedTarget.Reset();
	Controller->MoveToLocation(Destination, AcceptanceRadius, true, true, true, true, nullptr, true);
	RecordRequest(Destination);
}

void UGroundMonsterMovementComponent::ResetMovementState()
{
	ReleaseChaseSlot();
	if (bNavLinkJumpActive)
	{
		return;
	}
	Super::ResetMovementState();
	if (ACharacter* Character = GetCharacterOwner())
	{
		if (AAIController* Controller = Cast<AAIController>(Character->GetController()))
		{
			Controller->StopMovement();
		}
	}
	RequestedTarget.Reset();
	LastRequestedDestination = FVector::ZeroVector;
	NextRequestTime = 0.0;
}

bool UGroundMonsterMovementComponent::CanRefreshRequest(const FVector& Destination) const
{
	const UWorld* World = GetWorld();
	return !World || World->GetTimeSeconds() >= NextRequestTime ||
		   FVector::DistSquared2D(Destination, LastRequestedDestination) >= FMath::Square(DestinationRefreshDistance);
}

void UGroundMonsterMovementComponent::RecordRequest(const FVector& Destination)
{
	LastRequestedDestination = Destination;
	NextRequestTime = GetWorld() ? GetWorld()->GetTimeSeconds() + RequestRefreshInterval : 0.0;
}

void UGroundMonsterMovementComponent::ApplyPatternMovement(const FVector& Direction, const float DeltaTime)
{
	if (ActivePatternOption.bMoveDuringAbility)
	{
		Super::ApplyPatternMovement(Direction, DeltaTime);
		return;
	}

	ACharacter* Character = GetCharacterOwner();
	AAIController* Controller = Character ? Cast<AAIController>(Character->GetController()) : nullptr;
	if (!Controller)
	{
		// 경로를 요청할 컨트롤러가 없으면 입력 이동으로라도 움직인다.
		Super::ApplyPatternMovement(Direction, DeltaTime);
		return;
	}

	const FVector NavDestination = GetPatternNavDestination();
	if (!bPatternNavRequested || CanRefreshRequest(NavDestination))
	{
		Controller->MoveToLocation(NavDestination, AcceptanceRadius, true, true, true, true, nullptr, true);
		RequestedTarget.Reset();
		RecordRequest(NavDestination);
		bPatternNavRequested = true;
	}
}

void UGroundMonsterMovementComponent::FacePatternMovementTarget(const float DeltaTime)
{
	if (ActivePatternOption.bMoveDuringAbility)
	{
		Super::FacePatternMovementTarget(DeltaTime);
	}
}

void UGroundMonsterMovementComponent::OnPatternMovementStopped()
{
	Super::OnPatternMovementStopped();
	if (!bPatternNavRequested)
	{
		return;
	}

	bPatternNavRequested = false;
	if (ACharacter* Character = GetCharacterOwner())
	{
		if (AAIController* Controller = Cast<AAIController>(Character->GetController()))
		{
			Controller->StopMovement();
		}
	}

	// 추격이 다음 틱에 바로 새 경로를 요청하도록 요청 기록을 비운다.
	RequestedTarget.Reset();
	LastRequestedDestination = FVector::ZeroVector;
	NextRequestTime = 0.0;
}

FVector UGroundMonsterMovementComponent::GetPatternNavDestination() const
{
	const ACharacter* Character = GetCharacterOwner();
	if (!IsValid(Character))
	{
		return PatternDestination;
	}

	if (ActivePatternOption.Intent != EEnemyMovementIntent::ApproachTarget || !IsValid(PatternMovementTarget))
	{
		return PatternDestination;
	}

	/*
	 * 틀어진 방향으로 "정지 거리까지 남은 만큼"만 간 지점을 목적지로 잡는다. 목적지는 주기마다 다시 잡으므로
	 * 타깃 쪽으로 휘어 들어가는 궤적이 된다. 수락 반경만큼 더 보내야 가까운 거리에서 요청이 즉시 성공해 멈추지 않는다.
	 */
	const FVector Location = Character->GetActorLocation();
	const float Remaining =
		FMath::Max(FVector::Dist2D(Location, PatternMovementTarget->GetActorLocation()) - ApproachStopDistance, 0.0f);
	return Location + GetPatternMovementDirection() * (Remaining + AcceptanceRadius);
}
