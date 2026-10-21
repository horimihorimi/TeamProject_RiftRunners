#include "GA_PhantomRangedAttack.h"

#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "GameCore/AI/MonsterAIController.h"
#include "GameCore/Combat/MonsterDamageLibrary.h"
#include "GameCore/Projectile/MonsterProjectile.h"
#include "GameCore/Tags/GameplayTagSystem.h"
#include "GameFramework/Character.h"

UGA_PhantomRangedAttack::UGA_PhantomRangedAttack()
{
	// 발사 몽타주가 피격으로 중단되면 투사체 이벤트와 연출이 어긋난다.
	ActivationOwnedTags.AddTag(GameplayTagSystem::State_AI_SuperArmor);
}

void UGA_PhantomRangedAttack::OnBeforePatternMontagePlay(const FGameplayAbilityActorInfo* ActorInfo)
{
	ACharacter* Avatar = ActorInfo ? Cast<ACharacter>(ActorInfo->AvatarActor.Get()) : nullptr;
	if (IsValid(Avatar))
	{
		bUsingRightHand = FVector::DotProduct(Avatar->GetVelocity(), Avatar->GetActorRightVector()) >= 0.0f;
	}

	// Phantom.* 하위 이벤트만 받고 Payload.EventTag로 일반탄/분열탄을 나눈다.
	PhantomFireEventTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
		this, GameplayTagSystem::Event_AI_RangedAttack_Phantom, nullptr, false, false);
	PhantomFireEventTask->EventReceived.AddDynamic(this, &UGA_PhantomRangedAttack::OnPhantomFireEventReceived);
	PhantomFireEventTask->ReadyForActivation();

}

FName UGA_PhantomRangedAttack::GetPatternMontageStartSection() const
{
	return bUsingRightHand ? RightSectionName : LeftSectionName;
}

void UGA_PhantomRangedAttack::OnPhantomFireEventReceived(FGameplayEventData Payload)
{
	UE_LOG(LogTemp, Warning, TEXT("[PhantomSplit] EventReceived | Avatar=%s Tag=%s"),
		*GetNameSafe(GetCurrentActorInfo() ? GetCurrentActorInfo()->AvatarActor.Get() : nullptr),
		*Payload.EventTag.ToString());

	if (Payload.EventTag == GameplayTagSystem::Event_AI_RangedAttack_Phantom_Normal)
	{
		FireProjectile(false);
	}
	else if (Payload.EventTag == GameplayTagSystem::Event_AI_RangedAttack_Phantom_Split)
	{
		FireProjectile(true);
	}
}

void UGA_PhantomRangedAttack::FireProjectile(const bool bUseSplitPattern)
{
	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	ACharacter* Avatar = ActorInfo ? Cast<ACharacter>(ActorInfo->AvatarActor.Get()) : nullptr;
	const AMonsterAIController* Controller = IsValid(Avatar) ? Cast<AMonsterAIController>(Avatar->GetController()) : nullptr;
	AActor* Target = IsValid(Controller) ? Controller->GetPerceivedTarget() : nullptr;
	if (!IsValid(Avatar) || !IsValid(Target) || !IsValid(ProjectileClass))
	{
		UE_LOG(LogTemp, Warning, TEXT("[PhantomSplit] ParentSpawnSkipped | Split=%s Avatar=%s Target=%s ProjectileClass=%s"),
			bUseSplitPattern ? TEXT("true") : TEXT("false"), *GetNameSafe(Avatar), *GetNameSafe(Target), *GetNameSafe(ProjectileClass.Get()));
		return;
	}

	const USkeletalMeshComponent* Mesh = Avatar->GetMesh();
	const FName SocketName = bUsingRightHand ? RightMuzzleSocketName : LeftMuzzleSocketName;
	const FVector MuzzleLocation = IsValid(Mesh) && Mesh->DoesSocketExist(SocketName)
		? Mesh->GetSocketLocation(SocketName) : Avatar->GetActorLocation();
	const FRotator SpawnRotation = (Target->GetActorLocation() - MuzzleLocation).Rotation();
	FActorSpawnParameters Params;
	Params.Owner = Avatar;
	Params.Instigator = Avatar;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	if (AMonsterProjectile* Projectile = GetWorld()->SpawnActor<AMonsterProjectile>(ProjectileClass, MuzzleLocation, SpawnRotation, Params))
	{
		if (bUseSplitPattern)
		{
			UE_LOG(LogTemp, Warning, TEXT("[PhantomSplit] ParentSpawned | Projectile=%s Target=%s Location=%s Hover=%.2f Count=%d"),
				*GetNameSafe(Projectile), *GetNameSafe(Target), *MuzzleLocation.ToString(), SplitHoverDuration, SplitProjectileCount);
		}
		Projectile->SetUseSplitPattern(bUseSplitPattern);
		if (bUseSplitPattern)
		{
			Projectile->SetSplitPatternSettings(SplitHoverDuration, SplitProjectileCount, SplitLateralSpacing,
				SplitChildScale, SplitChildScaleParameterName);
		}
		Projectile->SetDamageEffectClass(DamageEffectClass);
		Projectile->SetAttackTag(MonsterDamageLibrary::FindAttackTag(this));
		Projectile->LaunchAtTarget(Target, Avatar);
	}
	else if (bUseSplitPattern)
	{
		UE_LOG(LogTemp, Warning, TEXT("[PhantomSplit] ParentSpawnFailed | Class=%s Location=%s"),
			*GetNameSafe(ProjectileClass.Get()), *MuzzleLocation.ToString());
	}
}

void UGA_PhantomRangedAttack::EndAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility, bool bWasCancelled)
{
	PhantomFireEventTask = nullptr;
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
