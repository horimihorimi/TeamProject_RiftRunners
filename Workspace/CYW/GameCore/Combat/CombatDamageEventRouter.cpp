#include "CombatDamageEventRouter.h"

#include "../Ability/GA_HitReact.h"
#include "../Character/MonsterCharacter.h"
#include "../Character/BossMonsterCharacter.h"
#include "GameCore/BasePlayerState.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "MonsterDropComponent.h"
#include "../Tags/GameplayTagSystem.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"

namespace
{
	ABasePlayerState* ResolveStatisticsPlayer(AActor* Actor)
	{
		if (!IsValid(Actor))
		{
			return nullptr;
		}
		if (ABasePlayerState* State = Cast<ABasePlayerState>(Actor))
		{
			return State;
		}
		if (const APawn* SourcePawn = Cast<APawn>(Actor))
		{
			return SourcePawn->GetPlayerState<ABasePlayerState>();
		}
		if (const AController* SourceController = Cast<AController>(Actor))
		{
			return SourceController->GetPlayerState<ABasePlayerState>();
		}
		const APawn* InstigatorPawn = Actor->GetInstigator();
		return InstigatorPawn ? InstigatorPawn->GetPlayerState<ABasePlayerState>() : nullptr;
	}

	void RecordRunDamage(AActor* SourceActor, AActor* TargetActor, float AppliedDamage, bool bJustDied)
	{
		if (!IsValid(TargetActor) || !TargetActor->HasAuthority() || AppliedDamage <= 0.0f)
		{
			return;
		}
		if (Cast<AMonsterCharacter>(TargetActor))
		{
			if (ABasePlayerState* SourcePS = ResolveStatisticsPlayer(SourceActor))
			{
				SourcePS->RecordDamageDealt(AppliedDamage, TargetActor->IsA<ABossMonsterCharacter>(), bJustDied);
			}
		}
		else if (ABasePlayerState* TargetPS = ResolveStatisticsPlayer(TargetActor))
		{
			TargetPS->RecordDamageTaken(AppliedDamage);
		}
	}

	void BroadcastDamageDealt(
		AActor* InstigatorActor,
		AActor* TargetActor,
		const FGameplayEffectModCallbackData& Data,
		float AppliedDamage,
		const FGameplayTagContainer& DynamicTags)
	{
		if (AppliedDamage <= 0.0f || !TargetActor || !TargetActor->HasAuthority())
		{
			return;
		}

		UAbilitySystemComponent* SourceASC =
			UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(InstigatorActor);
		if (!SourceASC)
		{
			return;
		}

		FGameplayEventData DamageDealtPayload;
		DamageDealtPayload.EventTag = GameplayTagSystem::Event_Combat_DamageDealt;
		DamageDealtPayload.Instigator = InstigatorActor;
		DamageDealtPayload.Target = TargetActor;
		DamageDealtPayload.EventMagnitude = AppliedDamage;
		DamageDealtPayload.ContextHandle = Data.EffectSpec.GetEffectContext();
		if (DynamicTags.HasTag(GameplayTagSystem::Hit_Weakpoint))
		{
			DamageDealtPayload.TargetTags.AddTag(GameplayTagSystem::Hit_Weakpoint);
		}

		SourceASC->HandleGameplayEvent(DamageDealtPayload.EventTag, &DamageDealtPayload);
	}

	void BroadcastDeath(
		UAbilitySystemComponent* TargetASC,
		AActor* TargetActor,
		AActor* InstigatorActor,
		const FGameplayEffectModCallbackData& Data,
		float LocalDamageDone)
	{
		if (!TargetASC || !TargetActor)
		{
			return;
		}

		// TargetASC->SetLooseGameplayTagCount(GameplayTagSystem::State_Dead, 1);

		FGameplayEventData DeathPayload;
		DeathPayload.EventTag = GameplayTagSystem::Event_Death;
		DeathPayload.Instigator = InstigatorActor;
		DeathPayload.Target = TargetActor;
		DeathPayload.ContextHandle = Data.EffectSpec.GetEffectContext();
		DeathPayload.EventMagnitude = LocalDamageDone;
		TargetASC->HandleGameplayEvent(GameplayTagSystem::Event_Death, &DeathPayload);
	}

	void TriggerPlayerHitReact(
		UAbilitySystemComponent* TargetASC,
		AActor* TargetActor,
		AActor* InstigatorActor,
		const FGameplayEffectModCallbackData& Data,
		float LocalDamageDone,
		bool bIsHeavyGroggy)
	{
		FGameplayEventData Payload;
		Payload.EventTag = bIsHeavyGroggy ? GameplayTagSystem::Event_HitReact_Heavy : GameplayTagSystem::Event_HitReact;
		Payload.Instigator = InstigatorActor;
		Payload.Target = TargetActor;
		Payload.ContextHandle = Data.EffectSpec.GetEffectContext();
		Payload.EventMagnitude = LocalDamageDone;

		int32 TriggeredAbilities = TargetASC->HandleGameplayEvent(Payload.EventTag, &Payload);
		if (TriggeredAbilities > 0)
		{
			return;
		}

		for (const FGameplayAbilitySpec& Spec : TargetASC->GetActivatableAbilities())
		{
			if (Spec.Ability && Spec.Ability->IsA(UGA_HitReact::StaticClass()))
			{
				TargetASC->TriggerAbilityFromGameplayEvent(
					Spec.Handle,
					TargetASC->AbilityActorInfo.Get(),
					Payload.EventTag,
					&Payload,
					*TargetASC
				);
			}
		}
	}

}

namespace CombatDamageEventRouter
{
	void RouteDamageExecuted(
		UAbilitySystemComponent* TargetASC,
		AActor* TargetActor,
		AActor* InstigatorActor,
		const FGameplayEffectModCallbackData& Data,
		float LocalDamageDone,
		float AppliedDamage,
		float PreviousHealth,
		float NewHealth,
		float MaxHealth)
	{
		const FGameplayTagContainer& DynamicTags = Data.EffectSpec.GetDynamicAssetTags();
		const bool bIsHeavyGroggy = DynamicTags.HasTag(GameplayTagSystem::Damage_Type_HeavyGroggy);
		const bool bIsLightGroggy = DynamicTags.HasTag(GameplayTagSystem::Damage_Type_Groggy);
		const bool bWasAlive = PreviousHealth > 0.0f;
		const bool bJustDied = bWasAlive && NewHealth <= 0.0f;

		// 사망 어빌리티/OnDeathStarted가 결과를 확정하기 전에 마지막 타격과 처치를 기록한다.
		RecordRunDamage(InstigatorActor, TargetActor, AppliedDamage, bJustDied);
		BroadcastDamageDealt(InstigatorActor, TargetActor, Data, AppliedDamage, DynamicTags);

		// 보상 기여도는 사망타까지 포함해 모든 피격에서 누적한다.
		// 이 시점을 놓치면 뒤(FinishDeath)에서는 누가 때렸는지 알 방법이 없다.
		if (TargetActor && TargetActor->HasAuthority())
		{
			if (AMonsterCharacter* DamagedMonster = Cast<AMonsterCharacter>(TargetActor))
			{
				if (UMonsterDropComponent* DropComponent = DamagedMonster->GetDropComponent())
				{
					DropComponent->RecordDamage(InstigatorActor, AppliedDamage);
					if (bJustDied)
					{
						// 골드는 사망 몽타주를 기다리지 않고 여기서 즉시 지급된다.
						DropComponent->NotifyKilled();
					}
				}
			}
		}

		if (bJustDied)
		{
			BroadcastDeath(TargetASC, TargetActor, InstigatorActor, Data, LocalDamageDone);
		}
		else if (NewHealth > 0.0f && TargetASC && TargetActor)
		{
			if (AMonsterCharacter* Monster = Cast<AMonsterCharacter>(TargetActor))
			{
				// 뒤에서 때리는 플레이어를 다음 평가 주기까지 무시하지 않도록 즉시 알린다.
				Monster->NotifyDamagedBy(InstigatorActor, AppliedDamage);

				Monster->HandleDamageHitReact(
					LocalDamageDone,
					InstigatorActor,
					Data.EffectSpec.GetEffectContext());
			}
			else if (TargetASC->HasMatchingGameplayTag(GameplayTagSystem::Character_Player)
				&& (bIsLightGroggy || bIsHeavyGroggy))
			{
				TriggerPlayerHitReact(TargetASC, TargetActor, InstigatorActor, Data, LocalDamageDone, bIsHeavyGroggy);
			}
		}
	}
}
