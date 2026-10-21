#include "CombatFeedbackComponent.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "GameCore/Character/PlayerCharacter.h"
#include "GameCore/Tags/GameplayTagSystem.h"
#include "GameCore/Weapon/Data/WeaponTypes.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "TimerManager.h"

UCombatFeedbackComponent::UCombatFeedbackComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UCombatFeedbackComponent::BeginPlay()
{
	Super::BeginPlay();

	if (APlayerController* PlayerController = Cast<APlayerController>(GetOwner()))
	{
		OwningPlayerController = PlayerController;
		PawnChangedDelegateHandle = PlayerController->GetOnNewPawnNotifier().AddUObject(
			this,
			&UCombatFeedbackComponent::RebindToPawn);
		RebindToPawn(PlayerController->GetPawn());
	}
}

void UCombatFeedbackComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (APlayerController* PlayerController = OwningPlayerController.Get())
	{
		PlayerController->GetOnNewPawnNotifier().Remove(PawnChangedDelegateHandle);
	}
	OwningPlayerController.Reset();
	PawnChangedDelegateHandle.Reset();

	UnbindDamageDealtEvent();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(BatchFlushTimerHandle);
	}

	PendingFeedback.Reset();
	DamageFeedbackReceived.Clear();

	Super::EndPlay(EndPlayReason);
}

void UCombatFeedbackComponent::ApplyWeaponRecoil(
	const FWeaponRuntimeStats& RuntimeStats,
	const UAbilitySystemComponent* SourceASC) const
{
	const APlayerController* PlayerController = OwningPlayerController.Get();
	APlayerCharacter* PlayerCharacter = PlayerController ? Cast<APlayerCharacter>(PlayerController->GetPawn()) : nullptr;
	if (!PlayerCharacter)
	{
		return;
	}

	const float RecoilMultiplier = SourceASC && SourceASC->HasMatchingGameplayTag(GameplayTagSystem::State_Weapon_Aiming)
		? RuntimeStats.Recoil.AimingMultiplier
		: 1.0f;
	PlayerCharacter->ApplyWeaponRecoil(RuntimeStats.Recoil.Pitch * RecoilMultiplier, RuntimeStats.Recoil.YawRatio);
}

void UCombatFeedbackComponent::RebindToPawn(APawn* NewPawn)
{
	UnbindDamageDealtEvent();
	PendingFeedback.Reset();
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(BatchFlushTimerHandle);
	}

	if (!GetOwner() || !GetOwner()->HasAuthority() || !NewPawn)
	{
		return;
	}

	UAbilitySystemComponent* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(NewPawn);
	if (!ASC)
	{
		return;
	}

	FGameplayEventMulticastDelegate& Delegate =
		ASC->GenericGameplayEventCallbacks.FindOrAdd(GameplayTagSystem::Event_Combat_DamageDealt);
	Delegate.RemoveAll(this);
	DamageDealtDelegateHandle = Delegate.AddUObject(this, &UCombatFeedbackComponent::OnDamageDealt);
	BoundAbilitySystemComponent = ASC;
}

void UCombatFeedbackComponent::UnbindDamageDealtEvent()
{
	if (UAbilitySystemComponent* ASC = BoundAbilitySystemComponent.Get())
	{
		if (FGameplayEventMulticastDelegate* Delegate =
			ASC->GenericGameplayEventCallbacks.Find(GameplayTagSystem::Event_Combat_DamageDealt))
		{
			Delegate->Remove(DamageDealtDelegateHandle);
		}
	}

	BoundAbilitySystemComponent.Reset();
	DamageDealtDelegateHandle.Reset();
}

void UCombatFeedbackComponent::OnDamageDealt(const FGameplayEventData* Payload)
{
	if (!Payload || Payload->EventMagnitude <= 0.0f)
	{
		return;
	}

	FDamageFeedbackData Feedback;
	Feedback.WorldLocation = ResolveDamageWorldLocation(*Payload);
	Feedback.DamageAmount = Payload->EventMagnitude;
	Feedback.bWeakpoint = Payload->TargetTags.HasTag(GameplayTagSystem::Hit_Weakpoint);
	QueueDamageFeedback(Feedback);
}

FVector UCombatFeedbackComponent::ResolveDamageWorldLocation(const FGameplayEventData& Payload) const
{
	if (const FHitResult* HitResult = Payload.ContextHandle.GetHitResult();
		HitResult && HitResult->bBlockingHit)
	{
		return HitResult->ImpactPoint;
	}

	if (const AActor* TargetActor = Payload.Target.Get())
	{
		FVector Origin = FVector::ZeroVector;
		FVector Extent = FVector::ZeroVector;
		TargetActor->GetActorBounds(true, Origin, Extent);
		return Origin + FVector(0.0f, 0.0f, FMath::Max(Extent.Z, 80.0f));
	}

	return FVector::ZeroVector;
}

void UCombatFeedbackComponent::QueueDamageFeedback(const FDamageFeedbackData& Feedback)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || Feedback.DamageAmount <= 0.0f)
	{
		return;
	}

	PendingFeedback.Add(Feedback);
	if (PendingFeedback.Num() >= MaxBatchSize || BatchIntervalSeconds <= 0.0f)
	{
		FlushPendingFeedback();
		return;
	}

	if (UWorld* World = GetWorld())
	{
		FTimerManager& TimerManager = World->GetTimerManager();
		if (!TimerManager.IsTimerActive(BatchFlushTimerHandle))
		{
			TimerManager.SetTimer(
				BatchFlushTimerHandle,
				this,
				&UCombatFeedbackComponent::FlushPendingFeedback,
				BatchIntervalSeconds,
				false);
		}
	}
}

void UCombatFeedbackComponent::FlushPendingFeedback()
{
	if (PendingFeedback.IsEmpty())
	{
		return;
	}

	ClientReceiveDamageFeedbackBatch(PendingFeedback);
	PendingFeedback.Reset();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(BatchFlushTimerHandle);
	}
}

void UCombatFeedbackComponent::ClientReceiveDamageFeedbackBatch_Implementation(
	const TArray<FDamageFeedbackData>& FeedbackBatch)
{
	if (const APlayerController* PlayerController = OwningPlayerController.Get();
		PlayerController && PlayerController->IsLocalController())
	{
		DamageFeedbackReceived.Broadcast(FeedbackBatch);
	}
}
