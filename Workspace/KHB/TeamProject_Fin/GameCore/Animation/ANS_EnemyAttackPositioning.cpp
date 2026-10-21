#include "ANS_EnemyAttackPositioning.h"

#include "Components/SkeletalMeshComponent.h"
#include "GameCore/AI/MonsterAIController.h"
#include "GameCore/Character/BossMonsterCharacter.h"
#include "GameCore/Movement/FlyingMonsterMovementComponent.h"
#include "GameFramework/Pawn.h"

namespace EnemyAttackPositioningNotify
{
	static UFlyingMonsterMovementComponent* FindMovement(const USkeletalMeshComponent* MeshComp)
	{
		AActor* Owner = IsValid(MeshComp) ? MeshComp->GetOwner() : nullptr;
		return IsValid(Owner) ? Owner->FindComponentByClass<UFlyingMonsterMovementComponent>() : nullptr;
	}

	static AActor* FindCombatTarget(const USkeletalMeshComponent* MeshComp)
	{
		AActor* Owner = IsValid(MeshComp) ? MeshComp->GetOwner() : nullptr;
		const APawn* Pawn = Cast<APawn>(Owner);
		const AMonsterAIController* Controller = IsValid(Pawn)
			? Cast<AMonsterAIController>(Pawn->GetController())
			: nullptr;
		return IsValid(Controller) ? Controller->GetPerceivedTarget() : nullptr;
	}

	static AActor* FindCenterAnchor(const USkeletalMeshComponent* MeshComp)
	{
		const AActor* Owner = IsValid(MeshComp) ? MeshComp->GetOwner() : nullptr;
		const ABossMonsterCharacter* Boss = Cast<ABossMonsterCharacter>(Owner);
		return IsValid(Boss) ? Boss->GetArenaCenter() : nullptr;
	}
}

FString UANS_EnemyAttackPositioning::GetNotifyName_Implementation() const
{
	return TEXT("Enemy Attack Positioning");
}

void UANS_EnemyAttackPositioning::NotifyBegin(
	USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	const float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);

	AActor* Owner = IsValid(MeshComp) ? MeshComp->GetOwner() : nullptr;
	if (!IsValid(Owner) || !Owner->HasAuthority())
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[EnemyAttackPositioning] Begin ignored | Owner=%s Authority=%s"),
			*GetNameSafe(Owner), IsValid(Owner) && Owner->HasAuthority() ? TEXT("True") : TEXT("False"));
		return;
	}

	UFlyingMonsterMovementComponent* Movement = EnemyAttackPositioningNotify::FindMovement(MeshComp);
	const bool bUseCenterAnchor = PositioningTarget == EEnemyAttackPositioningTarget::CenterAnchor;
	AActor* Target = bUseCenterAnchor
		? EnemyAttackPositioningNotify::FindCenterAnchor(MeshComp)
		: (bAdjustTargetDistance ? EnemyAttackPositioningNotify::FindCombatTarget(MeshComp) : nullptr);
	const float DesiredDistance = bUseCenterAnchor ? AnchorDistance : AttackDistance;
	UE_LOG(LogTemp, Warning,
		TEXT("[EnemyAttackPositioning] Begin | Owner=%s Movement=%s Target=%s Mode=%s Distance=%.1f Height=%.1f Acceptance=%.1f"),
		*GetNameSafe(Owner), *GetNameSafe(Movement), *GetNameSafe(Target),
		bUseCenterAnchor ? TEXT("CenterAnchor") : TEXT("CombatTarget"), DesiredDistance, HeightFromFloor, AcceptanceRadius);

	if (IsValid(Movement) && (!bUseCenterAnchor || IsValid(Target)))
	{
		Movement->BeginAttackPositioning(Target, DesiredDistance, HeightFromFloor, AcceptanceRadius, SpeedOverride, AccelerationOverride);
	}
}

void UANS_EnemyAttackPositioning::NotifyTick(
	USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	const float FrameDeltaTime, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyTick(MeshComp, Animation, FrameDeltaTime, EventReference);

	AActor* Owner = IsValid(MeshComp) ? MeshComp->GetOwner() : nullptr;
	if (!IsValid(Owner) || !Owner->HasAuthority())
	{
		return;
	}

	if (UFlyingMonsterMovementComponent* Movement = EnemyAttackPositioningNotify::FindMovement(MeshComp))
	{
		const bool bUseCenterAnchor = PositioningTarget == EEnemyAttackPositioningTarget::CenterAnchor;
		AActor* Target = bUseCenterAnchor
			? EnemyAttackPositioningNotify::FindCenterAnchor(MeshComp)
			: (bAdjustTargetDistance ? EnemyAttackPositioningNotify::FindCombatTarget(MeshComp) : nullptr);
		UE_LOG(LogTemp, Warning,
			TEXT("[EnemyAttackPositioning] Tick | Owner=%s Movement=%s Target=%s Mode=%s Delta=%.3f"),
			*GetNameSafe(Owner), *GetNameSafe(Movement), *GetNameSafe(Target),
			bUseCenterAnchor ? TEXT("CenterAnchor") : TEXT("CombatTarget"), FrameDeltaTime);
		if (!bUseCenterAnchor || IsValid(Target))
		{
			Movement->TickAttackPositioning(Target);
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[EnemyAttackPositioning] Tick ignored | Owner=%s Reason=NoFlyingMovement"),
			*GetNameSafe(Owner));
	}
}

void UANS_EnemyAttackPositioning::NotifyEnd(
	USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);

	AActor* Owner = IsValid(MeshComp) ? MeshComp->GetOwner() : nullptr;
	if (!IsValid(Owner) || !Owner->HasAuthority())
	{
		return;
	}

	if (UFlyingMonsterMovementComponent* Movement = EnemyAttackPositioningNotify::FindMovement(MeshComp))
	{
		Movement->EndAttackPositioning();
		UE_LOG(LogTemp, Warning,
			TEXT("[EnemyAttackPositioning] End | Owner=%s Movement=%s"),
			*GetNameSafe(Owner), *GetNameSafe(Movement));
	}
}
