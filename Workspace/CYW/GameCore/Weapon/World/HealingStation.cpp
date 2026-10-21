// Fill out your copyright notice in the Description page of Project Settings.


#include "HealingStation.h"
#include "GameCore/Interact/InteractTriggerComponent.h"
#include "GameCore/BasePlayerState.h"
#include "GameCore/Attribute/CharacterAttributeSet.h"
#include "GameCore/Tags/GameplayTagSystem.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameplayEffect.h"

// Sets default values
AHealingStation::AHealingStation()
{
	PrimaryActorTick.bCanEverTick = false;
	HealingStationRoot = CreateDefaultSubobject<USceneComponent>(TEXT("HealingStationRoot"));
	SetRootComponent(HealingStationRoot);
	HealingStationMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HealingStationMesh"));
	HealingStationMesh->SetupAttachment(HealingStationRoot);
	HealingStationMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	InteractTrigger = CreateDefaultSubobject<UInteractTriggerComponent>(TEXT("InteractTrigger"));
	InteractTrigger->SetupAttachment(HealingStationRoot);
	InteractText = NSLOCTEXT("HealingStation", "Heal", " E : 회복");
}

void AHealingStation::Execute(APlayerController* PC)
{
	if (!PC || !PC->HasAuthority())
	{
		return;
	}

	ABasePlayerState* PlayerState = PC->GetPlayerState<ABasePlayerState>();
	APawn* Player = PC->GetPawn();
	UAbilitySystemComponent* ASC = Player
		? UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Player) : nullptr;
	if (!PlayerState || !ASC || !ASC->HasAttributeSetForAttribute(UCharacterAttributeSet::GetHealthAttribute())
		|| !HealEffectClass || ASC->HasMatchingGameplayTag(GameplayTagSystem::State_Dead))
	{
		return;
	}

	const float Health = ASC->GetNumericAttribute(UCharacterAttributeSet::GetHealthAttribute());
	const float HealthCap = ASC->GetNumericAttribute(UCharacterAttributeSet::GetMaxHealthAttribute())
		* ASC->GetNumericAttribute(UCharacterAttributeSet::GetHealthCapMultiplierAttribute());
	if (Health <= 0.0f || Health >= HealthCap)
	{
		return;
	}

	FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
	Context.AddSourceObject(this);
	FGameplayEffectSpecHandle Heal = ASC->MakeOutgoingSpec(HealEffectClass, 1.0f, Context);
	if (!Heal.IsValid())
	{
		return;
	}
	Heal.Data->SetSetByCallerMagnitude(GameplayTagSystem::Data_Heal, HealthCap - Health);

	const int32 Cost = HealingCost;
	if (!PlayerState->TrySpendGold(Cost))
	{
		return;
	}
	ASC->ApplyGameplayEffectSpecToSelf(*Heal.Data.Get());
	// Instant effects have no persistent handle, so check the actual health change.
	if (ASC->GetNumericAttribute(UCharacterAttributeSet::GetHealthAttribute()) <= Health)
	{
		PlayerState->AddGold(Cost);
	}
}

FText AHealingStation::GetInteractText() const
{
	return FText::Format(NSLOCTEXT("HealingStation", "HealWithGold", "{0} ({1} 골드)"),
		InteractText, FText::AsNumber(HealingCost));
}

