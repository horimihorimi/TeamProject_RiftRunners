#include "GA_SentinelRangedAttack.h"

#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "GameCore/AI/MonsterAIController.h"
#include "GameCore/Combat/MonsterDamageLibrary.h"
#include "GameCore/Projectile/MonsterProjectile.h"
#include "GameCore/Tags/GameplayTagSystem.h"
#include "GameFramework/Character.h"

UGA_SentinelRangedAttack::UGA_SentinelRangedAttack()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	/*
	 * 식별은 Asset Tags의 Ability.Enemy.* 로, 데미지 표는 Damage Attack Tag의 Action.AI.* 로 찾는다.
	 * Asset Tags의 Action.AI.* 는 ST_Enemy_Range가 아직 이 태그로 GA를 발동해서 남겨 둔다. 새 StateTree로 옮기면 지운다.
	 */
	FGameplayTagContainer DefaultAssetTags;
	DefaultAssetTags.AddTag(GameplayTagSystem::Ability_Enemy_Range_RangedAttack);
	DefaultAssetTags.AddTag(GameplayTagSystem::Action_AI_RangedAttack_Sentinel_Normal);
	SetAssetTags(DefaultAssetTags);
	DefaultDamageAttackTag = GameplayTagSystem::Action_AI_RangedAttack_Sentinel_Normal;
	// 쌍포 발사 중 피격 반응으로 몽타주와 발사 Notify가 끊기지 않게 한다.
	ActivationOwnedTags.AddTag(GameplayTagSystem::State_AI_SuperArmor);
	MuzzleSocketNames = {TEXT("Muzzle_L"), TEXT("Muzzle_R")};
}

void UGA_SentinelRangedAttack::OnBeforePatternMontagePlay(const FGameplayAbilityActorInfo* ActorInfo)
{
	WaitFireEventTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
		this, GameplayTagSystem::Event_AI_RangedAttack_Sentinel_Normal, nullptr, false, true);
	WaitFireEventTask->EventReceived.AddDynamic(this, &UGA_SentinelRangedAttack::OnFireEventReceived);
	WaitFireEventTask->ReadyForActivation();

}

void UGA_SentinelRangedAttack::OnFireEventReceived(FGameplayEventData Payload)
{
	FireTwinCannons();
}

void UGA_SentinelRangedAttack::FireTwinCannons()
{
	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	ACharacter* Avatar = ActorInfo ? Cast<ACharacter>(ActorInfo->AvatarActor.Get()) : nullptr;
	const AMonsterAIController* Controller = IsValid(Avatar) ? Cast<AMonsterAIController>(Avatar->GetController()) : nullptr;
	AActor* Target = IsValid(Controller) ? Controller->GetPerceivedTarget() : nullptr;
	if (!IsValid(Avatar) || !IsValid(Target) || !IsValid(ProjectileClass))
	{
		return;
	}

	const USkeletalMeshComponent* Mesh = Avatar->GetMesh();
	const FGameplayTag AttackTag = MonsterDamageLibrary::FindAttackTag(this);

	// 소켓 위치를 먼저 모아둔다. 조준 방향을 포신들의 중심에서 한 번만 구하기 위해서다.
	TArray<FVector> MuzzleLocations;
	MuzzleLocations.Reserve(MuzzleSocketNames.Num());
	for (const FName SocketName : MuzzleSocketNames)
	{
		MuzzleLocations.Add(IsValid(Mesh) && Mesh->DoesSocketExist(SocketName)
			? Mesh->GetSocketLocation(SocketName)
			: Avatar->GetActorLocation());
	}

	if (MuzzleLocations.IsEmpty())
	{
		return;
	}

	FVector MuzzleCenter = FVector::ZeroVector;
	for (const FVector& Location : MuzzleLocations)
	{
		MuzzleCenter += Location;
	}
	MuzzleCenter /= static_cast<float>(MuzzleLocations.Num());

	/*
	 * 포신마다 타깃을 직접 조준하면 두 진행선이 타깃 지점에서 교차해, 날아가는
	 * 내내 서로 붙는 그림이 된다. 분열 자탄과 같은 방식으로 맞춘다.
	 * 조준은 포신 중심에서 한 번만 잡고, 모든 포신이 그 방향으로 평행하게 쏜다.
	 */
	const FVector AimDirection =
		(Target->GetActorLocation() - MuzzleCenter).GetSafeNormal();
	const FRotator SpawnRotation = AimDirection.Rotation();

	for (const FVector& MuzzleLocation : MuzzleLocations)
	{
		FActorSpawnParameters Params;
		Params.Owner = Avatar;
		Params.Instigator = Avatar;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		if (AMonsterProjectile* Projectile = GetWorld()->SpawnActor<AMonsterProjectile>(ProjectileClass, MuzzleLocation, SpawnRotation, Params))
		{
			Projectile->SetUseSplitPattern(false);
			Projectile->SetDamageEffectClass(DamageEffectClass);
			Projectile->SetAttackTag(AttackTag);
			/*
			 * 유도하지 않는다. 두 탄이 끝까지 나란히 날아가 탄막을 이루고,
			 * 플레이어는 그 사이나 바깥으로 빠져 피한다.
			 */
			Projectile->LaunchStraight(AimDirection, Avatar);
		}
	}
}

void UGA_SentinelRangedAttack::EndAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility, bool bWasCancelled)
{
	WaitFireEventTask = nullptr;
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
