#include "GameCore/Combat/MonsterDamageLibrary.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Abilities/GameplayAbility.h"
#include "Engine/World.h"
#include "GameCore/Character/BaseCharacter.h"
#include "GameCore/Combat/EnemyDamageTagSource.h"
#include "GameCore/Combat/MonsterAttackDamageTable.h"
#include "GameCore/Interface/MonsterAttackContext.h"
#include "GameCore/Tags/GameplayTagSystem.h"

namespace MonsterDamageLibrary
{
	FDamagePayload MakeAttackPayload(
		const FGameplayTag& AttackTag,
		const TSubclassOf<UGameplayEffect> DamageEffectClass)
	{
		FDamagePayload Payload;
		Payload.DamageEffectClass = DamageEffectClass;

		/*
		 * 표에 행이 없을 때의 기본 계수. 페이로드 기본값인 0을 그대로 두면
		 * 데미지가 통째로 사라져, 표에 빠뜨린 공격과 원래 아프지 않은 공격을
		 * 구분할 수 없다. 1.0이면 최소한 맞은 것은 느껴진다.
		 */
		Payload.BaseDamage = 1.0f;

		/*
		 * 몬스터 고유 공격력은 ExecCalc_Damage가 세트에서 직접 캡처한다.
		 * 여기서 곱해두면 같은 값이 두 번 반영되므로 계수만 넘긴다.
		 */
		if (const FMonsterAttackDamageRow* Row = UMonsterCombatSettings::FindAttackRow(AttackTag))
		{
			Payload.BaseDamage = Row->SkillCoefficient;
			Payload.ArmorPenetration = Row->ArmorPenetration;
			Payload.MaxHealthPercent = Row->MaxHealthPercent;
			Payload.GroggyType = Row->GroggyType;
		}

		return Payload;
	}

	FGameplayTag FindAttackTag(const UGameplayAbility* Ability)
	{
		if (!IsValid(Ability))
		{
			return FGameplayTag();
		}

		if (const IEnemyDamageTagSource* DamageTagSource = Cast<IEnemyDamageTagSource>(Ability))
		{
			const FGameplayTag DamageAttackTag = DamageTagSource->GetDamageAttackTag();
			if (DamageAttackTag.IsValid())
			{
				return DamageAttackTag;
			}
		}

		/*
		 * 이관 전 BP GA는 Asset Tags에 데미지 키를 넣어 두었다. 필드로 옮기면 이 경로는 쓰이지 않는다.
		 * Action.AI 하위만 본다. Asset Tags는 식별 외의 용도로도 쓰이므로
		 * 아무거나 집으면 엉뚱한 태그로 표를 뒤지게 된다.
		 */
		const FGameplayTagContainer Filtered = Ability->GetAssetTags().Filter(
			FGameplayTagContainer(GameplayTagSystem::Action_AI));

		return Filtered.IsEmpty() ? FGameplayTag() : Filtered.First();
	}

	void PushAttackTag(const UGameplayAbility* Ability, AActor* AvatarActor)
	{
		if (IMonsterAttackContext* Context = Cast<IMonsterAttackContext>(AvatarActor))
		{
			Context->SetCurrentAttackTag(FindAttackTag(Ability));
		}
	}

	void ClearAttackTag(AActor* AvatarActor)
	{
		if (IMonsterAttackContext* Context = Cast<IMonsterAttackContext>(AvatarActor))
		{
			Context->SetCurrentAttackTag(FGameplayTag());
		}
	}

	FGameplayTag ResolveAttackTag(const AActor* Actor)
	{
		const IMonsterAttackContext* Context = Cast<IMonsterAttackContext>(Actor);
		return Context ? Context->GetCurrentAttackTag() : FGameplayTag();
	}

	void GatherTargets(
		const UObject* WorldContextObject,
		const FVector& Start,
		const FVector& End,
		const float Radius,
		const TArray<const AActor*>& IgnoredActors,
		TMap<AActor*, FHitResult>& InOutHits)
	{
		const UWorld* World = GEngine
			? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull)
			: nullptr;

		if (World == nullptr)
		{
			return;
		}

		FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(MonsterAreaDamage), false);
		for (const AActor* Ignored : IgnoredActors)
		{
			if (IsValid(Ignored))
			{
				QueryParams.AddIgnoredActor(Ignored);
			}
		}

		TArray<FHitResult> HitResults;
		World->SweepMultiByChannel(
			HitResults,
			Start,
			End,
			FQuat::Identity,
			ECC_TraceAttack,
			FCollisionShape::MakeSphere(FMath::Max(Radius, 1.0f)),
			QueryParams);

		for (const FHitResult& Hit : HitResults)
		{
			AActor* HitActor = Hit.GetActor();
			if (!IsValid(HitActor) || InOutHits.Contains(HitActor))
			{
				continue;
			}

			const UAbilitySystemComponent* TargetASC =
				UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(HitActor);

			if (!IsValid(TargetASC) ||
				!TargetASC->HasMatchingGameplayTag(GameplayTagSystem::Character_Player))
			{
				continue;
			}

			InOutHits.Add(HitActor, Hit);
		}
	}

	void ApplyDamageToTargets(
		AActor* InstigatorActor,
		const FDamagePayload& BasePayload,
		const TMap<AActor*, FHitResult>& Hits,
		const FName LogAttackType)
	{
		for (const TPair<AActor*, FHitResult>& Entry : Hits)
		{
			FDamagePayload Payload = BasePayload;

			// 대상은 이 HitResult에서 결정된다. 비워두면 데미지가 통째로 무시된다.
			Payload.HitResult = Entry.Value;

			UDamageSystemLibrary::LogDamageApplied(InstigatorActor, Entry.Key, LogAttackType);
			UDamageSystemLibrary::ApplyGameDamage(InstigatorActor, InstigatorActor, Payload);
		}
	}
}
