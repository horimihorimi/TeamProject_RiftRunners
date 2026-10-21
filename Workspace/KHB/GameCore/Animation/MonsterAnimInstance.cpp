#include "GameCore/Animation/MonsterAnimInstance.h"

#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"

void UMonsterAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();
	OwningPawn = TryGetPawnOwner();
}

void UMonsterAnimInstance::NativeUpdateAnimation(const float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	if (!IsValid(OwningPawn))
	{
		OwningPawn = TryGetPawnOwner();
	}

	if (!IsValid(OwningPawn))
	{
		WorldVelocity = FVector::ZeroVector;
		MeshTransform = FTransform::Identity;
		return;
	}

	WorldVelocity = OwningPawn->GetVelocity();

	const ACharacter* Character = Cast<ACharacter>(OwningPawn);
	const USkeletalMeshComponent* MeshComponent =
		Character ? Character->GetMesh() : nullptr;
	MeshTransform = MeshComponent
		? MeshComponent->GetComponentTransform()
		: OwningPawn->GetActorTransform();
}

void UMonsterAnimInstance::NativeThreadSafeUpdateAnimation(const float DeltaSeconds)
{
	Super::NativeThreadSafeUpdateAnimation(DeltaSeconds);

	const FVector HorizontalVelocity(WorldVelocity.X, WorldVelocity.Y, 0.0f);

	/*
	 * Blend Space 입력은 Pawn/캡슐의 축이 아닌 실제 스켈레탈 메시 축을
	 * 기준으로 계산한다. 메시가 BP에서 축 보정되어 있거나 원본 메시의
	 * 전방 축이 Pawn과 다른 몬스터도 F/B/L/R 샘플을 올바르게 선택한다.
	 */
	const FVector LocalHorizontalVelocity =
		MeshTransform.InverseTransformVectorNoScale(HorizontalVelocity);
	const FVector LocalVelocity =
		MeshTransform.InverseTransformVectorNoScale(WorldVelocity);
	const FVector LocalHorizontalDirection =
		LocalHorizontalVelocity.GetSafeNormal();
	const FVector LocalDirection = LocalVelocity.GetSafeNormal();

	Speed = WorldVelocity.Size();
	GroundSpeed = HorizontalVelocity.Size();
	bIsMoving = GroundSpeed > MovingSpeedThreshold;
	LocalVelocityX = LocalHorizontalDirection.X;
	LocalVelocityY = LocalHorizontalDirection.Y;
	LocalVelocityZ = LocalDirection.Z;
}
