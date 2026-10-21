#include "GameCore/Animation/AN_SpawnDroneSwarm.h"

#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameCore/AI/MonsterAIController.h"
#include "GameCore/Combat/DroneSwarm.h"
#include "GameCore/Combat/MonsterDamageLibrary.h"
#include "GameFramework/Pawn.h"

void UAN_SpawnDroneSwarm::Notify(
	USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	AActor* Owner = IsValid(MeshComp) ? MeshComp->GetOwner() : nullptr;
	if (!IsValid(Owner) || !Owner->HasAuthority() || SwarmClass == nullptr)
	{
		return;
	}

	const APawn* OwnerPawn = Cast<APawn>(Owner);
	const AMonsterAIController* Controller =
		IsValid(OwnerPawn) ? Cast<AMonsterAIController>(OwnerPawn->GetController()) : nullptr;

	AActor* TargetActor = IsValid(Controller) ? Controller->GetPerceivedTarget() : nullptr;

	// 소켓 이름을 위치로 바꿔 넘긴다. 스웜은 스켈레톤을 몰라도 된다.
	TArray<FVector> LaunchLocations;
	LaunchLocations.Reserve(FMath::Max(LaunchSocketNames.Num(), 1));

	for (const FName& SocketName : LaunchSocketNames)
	{
		if (!SocketName.IsNone())
		{
			LaunchLocations.Add(MeshComp->GetSocketLocation(SocketName));
		}
	}

	if (LaunchLocations.Num() == 0)
	{
		LaunchLocations.Add(Owner->GetActorLocation());
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = Owner;
	SpawnParams.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	ADroneSwarm* Swarm = Owner->GetWorld()->SpawnActor<ADroneSwarm>(
		SwarmClass, LaunchLocations[0], FRotator::ZeroRotator, SpawnParams);

	if (IsValid(Swarm))
	{
		/*
		 * 태그는 지금 읽어서 넘긴다. 스웜은 어빌리티가 끝난 뒤에도 살아 있어서
		 * 자폭할 때 다시 물어보면 이미 비어 있거나 다음 공격 태그로 바뀌어 있다.
		 */
		Swarm->Deploy(
			Owner,
			LaunchLocations,
			TargetActor,
			MonsterDamageLibrary::ResolveAttackTag(Owner));
	}
}
