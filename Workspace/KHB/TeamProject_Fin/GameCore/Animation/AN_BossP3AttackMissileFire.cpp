#include "AN_BossP3AttackMissileFire.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameCore/Tags/GameplayTagSystem.h"

FString UAN_BossP3AttackMissileFire::GetNotifyName_Implementation() const
{
	return TEXT("Boss P3 Attack Missile Fire");
}

void UAN_BossP3AttackMissileFire::Notify(
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
		GameplayTagSystem::Event_AI_Projectile_Fire,
		FGameplayEventData());
}
