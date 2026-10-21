#include "GA_BossShatteredMirror.h"

#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Components/SkeletalMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "GameplayEffect.h"
#include "GameFramework/Character.h"
#include "GameCore/Projectile/BossWaveProjectile.h"
#include "GameCore/Tags/GameplayTagSystem.h"

UGA_BossShatteredMirror::UGA_BossShatteredMirror()
{
	SetAssetTags(FGameplayTagContainer(GameplayTagSystem::Ability_Enemy_Boss_ShatteredMirror));
}

void UGA_BossShatteredMirror::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	// 베이스가 몽타주 준비에 실패하면 이미 종료된 상태다. 그 위에 태스크를 얹지 않는다.
	if (!IsActive())
	{
		return;
	}

	WaveBlastTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
		this,
		GameplayTagSystem::Event_AI_Boss_ShatteredMirror_Fire,
		nullptr,
		false,
		true);

	if (!IsValid(WaveBlastTask))
	{
		return;
	}

	WaveBlastTask->EventReceived.AddDynamic(this, &UGA_BossShatteredMirror::OnWaveBlastEvent);
	WaveBlastTask->ReadyForActivation();
}

void UGA_BossShatteredMirror::OnWaveBlastEvent(FGameplayEventData Payload)
{
	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	ACharacter* Character = ActorInfo ? Cast<ACharacter>(ActorInfo->AvatarActor.Get()) : nullptr;
	USkeletalMeshComponent* Mesh = IsValid(Character) ? Character->GetMesh() : nullptr;
	UWorld* World = Character ? Character->GetWorld() : nullptr;

	if (!IsValid(Mesh) || !IsValid(World) || !WaveProjectileClass)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[ShatteredMirror] 벽을 스폰할 수 없습니다. WaveProjectileClass 를 확인하세요. | Boss=%s"),
			*GetNameSafe(Character));
		return;
	}

	// 노티파이가 서버에서만 이벤트를 보내지만, 스폰 직전에 한 번 더 확인한다.
	if (!Character->HasAuthority())
	{
		return;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = Character;
	SpawnParams.Instigator = Character;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	for (const FBossWaveEmitter& Emitter : WaveEmitters)
	{
		if (Emitter.PlateBoneName.IsNone() || Mesh->GetBoneIndex(Emitter.PlateBoneName) == INDEX_NONE)
		{
			UE_LOG(LogTemp, Warning,
				TEXT("[ShatteredMirror] 본 %s 를 찾지 못했습니다. | Boss=%s"),
				*Emitter.PlateBoneName.ToString(),
				*GetNameSafe(Character));
			continue;
		}

		/*
		 * 판의 위치와 회전을 본에서 그대로 가져온다.
		 *
		 * 본이 판마다 이미 정렬되어 있어 소켓을 따로 만들 필요가 없다. 판이 기울어져
		 * 있어도 그 기울기가 회전에 담겨 있으므로 방향 계산이 필요 없다.
		 */
		const FTransform BoneTransform = Mesh->GetSocketTransform(Emitter.PlateBoneName, RTS_World);

		FVector LaunchDirection;
		switch (LaunchAxis)
		{
		case EBossWaveLaunchAxis::BoneY:
			LaunchDirection = BoneTransform.GetUnitAxis(EAxis::Y);
			break;
		case EBossWaveLaunchAxis::BoneZ:
			LaunchDirection = BoneTransform.GetUnitAxis(EAxis::Z);
			break;
		default:
			LaunchDirection = BoneTransform.GetUnitAxis(EAxis::X);
			break;
		}

		if (bInvertLaunchAxis)
		{
			LaunchDirection = -LaunchDirection;
		}

		ABossWaveProjectile* Wave = World->SpawnActor<ABossWaveProjectile>(
			WaveProjectileClass,
			BoneTransform.GetLocation(),
			BoneTransform.Rotator(),
			SpawnParams);

		if (!IsValid(Wave))
		{
			continue;
		}

		Wave->InitializeWave(
			Emitter.BoxExtent,
			LaunchDirection,
			WaveSpeed,
			WaveAttackTag.IsValid() ? WaveAttackTag : GameplayTagSystem::Action_AI_Boss_ShatteredMirror_Wall,
			WaveDamageEffectClass);

		if (WaveLifeSpan > 0.0f)
		{
			Wave->SetLifeSpan(WaveLifeSpan);
		}

#if !UE_BUILD_SHIPPING
		if (bDrawDebugWave)
		{
			// 축이 맞는지는 눈으로 봐야 안다. 판 면과 화살표가 수직이면 맞다.
			DrawDebugBox(World, BoneTransform.GetLocation(), Emitter.BoxExtent,
				BoneTransform.GetRotation(), FColor::Cyan, false, 3.0f, 0, 3.0f);
			DrawDebugDirectionalArrow(World, BoneTransform.GetLocation(),
				BoneTransform.GetLocation() + LaunchDirection * 300.0f,
				60.0f, FColor::Red, false, 3.0f, 0, 5.0f);
		}
#endif
	}
}
