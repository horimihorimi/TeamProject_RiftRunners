#include "ANS_FaceTarget.h"

#include "Components/SkeletalMeshComponent.h"
#include "GameCore/AI/MonsterAIController.h"
#include "GameFramework/Pawn.h"

void UANS_FaceTarget::NotifyTick(
	USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation,
	float FrameDeltaTime,
	const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyTick(MeshComp, Animation, FrameDeltaTime, EventReference);

	if (!IsValid(MeshComp))
	{
		return;
	}

	APawn* OwnerPawn = Cast<APawn>(MeshComp->GetOwner());

	if (!IsValid(OwnerPawn))
	{
		return;
	}

	AMonsterAIController* Controller = Cast<AMonsterAIController>(OwnerPawn->GetController());

	if (!IsValid(Controller))
	{
		return;
	}

	AActor* TargetActor = Controller->GetPerceivedTarget();

	if (!IsValid(TargetActor))
	{
		return;
	}

	FVector ToTarget = TargetActor->GetActorLocation() - OwnerPawn->GetActorLocation();

	if (!bIncludePitch)
	{
		ToTarget.Z = 0.0f;
	}

	if (ToTarget.IsNearlyZero())
	{
		return;
	}

	const FRotator NewRotation = FMath::RInterpTo(
		OwnerPawn->GetActorRotation(),
		ToTarget.Rotation(),
		FrameDeltaTime,
		RotationSpeed);

	OwnerPawn->SetActorRotation(NewRotation);
}
