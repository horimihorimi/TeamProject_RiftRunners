#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "GameCore/Ability/DamageSystemLibrary.h"

class UGameplayAbility;
class UGameplayEffect;

/**
 * 몬스터 공격이 데미지를 넣기 전에 거치는 층.
 *
 * WeaponDamageLibrary가 무기 스탯과 거리 감쇠를 페이로드로 번역하듯, 이쪽은
 * 공격 태그를 위력표에서 찾아 페이로드로 번역한다. 데미지를 실제로 적용하는
 * 일은 여기서 하지 않고 UDamageSystemLibrary로 넘긴다.
 *
 * 몬스터 전용 규칙을 UDamageSystemLibrary에 넣지 않는 이유가 이것이다. 그쪽은
 * 플레이어 공격도 함께 지나가는 공용 경로라, 몬스터 위력표를 알게 되면 공용
 * 파이프라인이 한쪽 진영에 의존하게 된다.
 */
namespace MonsterDamageLibrary
{
	/**
	 * 공격 태그로 위력표를 찾아 페이로드를 채운다.
	 *
	 * HitResult는 채우지 않는다. 어디를 맞췄는지는 트레이스한 호출부만 알고,
	 * 표는 그 공격이 얼마나 아픈지만 안다.
	 *
	 * 표에 행이 없으면 계수 1.0으로 둔다. 0으로 두면 표에 빠뜨린 공격과 원래
	 * 아프지 않은 공격을 구분할 수 없다.
	 */
	TEAMPROJECT_FIN_API FDamagePayload MakeAttackPayload(
		const FGameplayTag& AttackTag,
		TSubclassOf<UGameplayEffect> DamageEffectClass);

	/**
	 * 어빌리티의 데미지 키(Action.AI.*)를 꺼낸다.
	 *
	 * IEnemyDamageTagSource의 Damage Attack Tag를 먼저 본다. 비어 있으면 Asset Tags의
	 * Action.AI 하위를 본다. 뒤쪽은 아직 필드로 옮기지 않은 BP GA를 위한 이관용 경로다.
	 */
	TEAMPROJECT_FIN_API FGameplayTag FindAttackTag(const UGameplayAbility* Ability);

	/** 어빌리티의 공격 태그를 아바타에 적는다. 아바타가 창구를 열지 않았으면 무시된다. */
	TEAMPROJECT_FIN_API void PushAttackTag(const UGameplayAbility* Ability, AActor* AvatarActor);

	/** 아바타에 적힌 공격 태그를 지운다. 어빌리티가 끝날 때 부른다. */
	TEAMPROJECT_FIN_API void ClearAttackTag(AActor* AvatarActor);

	/**
	 * 이 액터가 지금 시전 중인 공격 태그. 창구가 없으면 빈 태그.
	 *
	 * 타격 윈도우가 열릴 때, 또는 무언가를 스폰하기 직전에 한 번 부른다.
	 * 매 타격마다 부르면 몽타주 도중 어빌리티가 바뀔 때 위력이 따라 바뀐다.
	 */
	TEAMPROJECT_FIN_API FGameplayTag ResolveAttackTag(const AActor* Actor);

	/**
	 * 구간을 훑어 데미지를 줄 플레이어를 모은다.
	 *
	 * Start와 End를 같게 주면 그 자리의 구 판정이 된다. 폭발은 구, 전깃줄은
	 * 선이지만 판정 모양만 다를 뿐 하는 일이 같아 한 함수로 둔다.
	 *
	 * 이미 담긴 액터는 다시 담지 않는다. 폭발과 선이 겹치는 자리에 선 대상이
	 * 두 번 맞지 않게 하려면 호출 사이에 같은 맵을 넘겨야 한다.
	 */
	TEAMPROJECT_FIN_API void GatherTargets(
		const UObject* WorldContextObject,
		const FVector& Start,
		const FVector& End,
		float Radius,
		const TArray<const AActor*>& IgnoredActors,
		TMap<AActor*, FHitResult>& InOutHits);

	/**
	 * 모은 대상 전부에게 같은 페이로드로 데미지를 넣는다.
	 *
	 * 페이로드의 HitResult는 여기서 대상별로 채운다. 넘겨받은 값은 무시된다.
	 */
	TEAMPROJECT_FIN_API void ApplyDamageToTargets(
		AActor* InstigatorActor,
		const FDamagePayload& BasePayload,
		const TMap<AActor*, FHitResult>& Hits,
		FName LogAttackType);
}
