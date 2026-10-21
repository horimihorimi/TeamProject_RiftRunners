#include "GameCore/Animation/AN_BossP3AttackShout.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameCore/Tags/GameplayTagSystem.h"

FString UAN_BossP3AttackShout::GetNotifyName_Implementation() const
{
	return TEXT("Boss P3 Attack Shout");
}

void UAN_BossP3AttackShout::Notify(
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
		GameplayTagSystem::Event_AI_Boss_P3Attack_Shout,
		FGameplayEventData());
}
