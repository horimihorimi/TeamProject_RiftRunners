#include "AN_BossP3AttackPillarSpawn.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameCore/Tags/GameplayTagSystem.h"

FString UAN_BossP3AttackPillarSpawn::GetNotifyName_Implementation() const
{
	return TEXT("Boss P3 Attack Pillar Spawn");
}

void UAN_BossP3AttackPillarSpawn::Notify(
	USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	AActor* Owner = IsValid(MeshComp) ? MeshComp->GetOwner() : nullptr;
	if (!IsValid(Owner) || !Owner->HasAuthority())
	{
		return;
	}

	UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(
		Owner,
		GameplayTagSystem::Event_AI_Boss_P3Attack_PillarSpawn,
		FGameplayEventData());
}
