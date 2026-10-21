#include "GA_RadialExplosion.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "DrawDebugHelpers.h"
#include "GameCore/Combat/MonsterDamageLibrary.h"
#include "GameCore/Projectile/MonsterProjectile.h"
#include "GameCore/Tags/GameplayTagSystem.h"

UGA_RadialExplosion::UGA_RadialExplosion()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;

	/*
	 * Asset Tag가 이 어빌리티를 태그로 찾는 유일한 통로다.
	 *
	 * PatternSet 이나 노티파이가 태그로 발동을 요청하므로, 비어 있으면
	 * TryActivateAbilitiesByTag가 이 어빌리티를 찾지 못한다.
	 */
	SetAssetTags(FGameplayTagContainer(GameplayTagSystem::Ability_Enemy_Boss_RadialExplosion));

	// 몽타주의 AnimNotify_SendGameplayEvent가 폭발 프레임에 보내는 이벤트로 활성화한다.
	FAbilityTriggerData TriggerData;
	TriggerData.TriggerTag = GameplayTagSystem::Event_AI_Boss_RadialExplosion;
	TriggerData.TriggerSource = EGameplayAbilityTriggerSource::GameplayEvent;
	AbilityTriggers.Add(TriggerData);
}

void UGA_RadialExplosion::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	AActor* AvatarActor = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
	if (!IsValid(AvatarActor) || !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (bDebugDrawExplosion)
	{
		DrawDebugSphere(
			AvatarActor->GetWorld(), AvatarActor->GetActorLocation(), ExplosionRadius,
			24, FColor::Red, false, DebugExplosionDrawDuration, 0, 2.0f);
	}

	ApplyExplosionDamage(AvatarActor);

	if (ExplosionCueTag.IsValid())
	{
		if (UAbilitySystemComponent* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(AvatarActor))
		{
			FGameplayCueParameters CueParameters;
			// Cue가 Parameter Location을 쓰는 경우에도 월드 원점이 아니라 실제 폭발 지점에 생성한다.
			CueParameters.Location = AvatarActor->GetActorLocation();
			CueParameters.SourceObject = ExplosionCueData.Get();
			ASC->ExecuteGameplayCue(ExplosionCueTag, CueParameters);
		}
	}

	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}

void UGA_RadialExplosion::ApplyExplosionDamage(AActor* AvatarActor)
{
	if (!IsValid(AvatarActor) || !AvatarActor->HasAuthority() || DamageEffectClass == nullptr || !AttackTag.IsValid())
	{
		return;
	}

	TMap<AActor*, FHitResult> DamagedHits;
	const FVector Origin = AvatarActor->GetActorLocation();
	MonsterDamageLibrary::GatherTargets(
		AvatarActor, Origin, Origin, ExplosionRadius, { AvatarActor }, DamagedHits);

	MonsterDamageLibrary::ApplyDamageToTargets(
		AvatarActor,
		MonsterDamageLibrary::MakeAttackPayload(AttackTag, DamageEffectClass),
		DamagedHits,
		TEXT("RadialExplosion"));
}
