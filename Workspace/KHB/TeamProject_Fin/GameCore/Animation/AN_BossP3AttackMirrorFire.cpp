#include "AN_BossP3AttackMirrorFire.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameCore/Tags/GameplayTagSystem.h"

FString UAN_BossP3AttackMirrorFire::GetNotifyName_Implementation() const
{
	return TEXT("Boss P3 Attack Mirror Fire");
}

void UAN_BossP3AttackMirrorFire::Notify(
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
		GameplayTagSystem::Event_AI_Boss_P3Attack_MirrorFire,
		FGameplayEventData());
}
