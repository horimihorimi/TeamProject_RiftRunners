#include "GA_BossP3Attack.h"

#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "AbilitySystemComponent.h"
#include "GameplayEffect.h"
#include "AbilitySystemGlobals.h"
#include "GameCore/Character/BaseCharacter.h"
#include "GameCore/Combat/MonsterDamageLibrary.h"
#include "GameCore/Projectile/BossP3AttackPillar.h"
#include "GameCore/Projectile/BossWaveProjectile.h"
#include "GameCore/Projectile/MonsterProjectile.h"
#include "GameCore/Tags/GameplayTagSystem.h"
#include "Kismet/GameplayStatics.h"

UGA_BossP3Attack::UGA_BossP3Attack()
{
	SetAssetTags(FGameplayTagContainer(GameplayTagSystem::Ability_Enemy_Boss_P3Attack));
	PillarSpawnOffsets = {
		FVector(0.0f, -600.0f, 0.0f),
		FVector::ZeroVector,
		FVector(0.0f, 600.0f, 0.0f)
	};
	FBossMissileBurstPreset DefaultBurstPreset;
	DefaultBurstPreset.SpawnOffsets = {
		FVector(0.0f, -600.0f, 500.0f),
		FVector(0.0f, 0.0f, 700.0f),
		FVector(0.0f, 600.0f, 500.0f),
		FVector(250.0f, -300.0f, 1000.0f),
		FVector(250.0f, 300.0f, 1000.0f)
	};
	MissileBurstPresets.Add(MoveTemp(DefaultBurstPreset));
}

void UGA_BossP3Attack::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	bShoutResolved = false;
	MissileBurstIndex = 0;
	RemoveAppliedShoutStaggers();

	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
	if (!IsActive())
	{
		return;
	}

	// P3Attack 전용 Mirror Fire 애님 노티파이만 받는다.
	MirrorLaunchTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
		this, GameplayTagSystem::Event_AI_Boss_P3Attack_MirrorFire, nullptr, false, true);
	if (!IsValid(MirrorLaunchTask))
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}

	MirrorLaunchTask->EventReceived.AddDynamic(this, &ThisClass::OnMirrorLaunchEvent);
	MirrorLaunchTask->ReadyForActivation();

	PillarSpawnTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
		this, GameplayTagSystem::Event_AI_Boss_P3Attack_PillarSpawn, nullptr, false, true);
	if (!IsValid(PillarSpawnTask))
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}

	PillarSpawnTask->EventReceived.AddDynamic(this, &ThisClass::OnPillarSpawnEvent);
	PillarSpawnTask->ReadyForActivation();

	MissileFireTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
		this, GameplayTagSystem::Event_AI_Projectile_Fire, nullptr, false, true);
	if (!IsValid(MissileFireTask))
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}

	MissileFireTask->EventReceived.AddDynamic(this, &ThisClass::OnMissileFireEvent);
	MissileFireTask->ReadyForActivation();

	ShoutTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
		this, GameplayTagSystem::Event_AI_Boss_P3Attack_Shout, nullptr, false, true);
	if (!IsValid(ShoutTask))
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}

	ShoutTask->EventReceived.AddDynamic(this, &ThisClass::OnShoutEvent);
	ShoutTask->ReadyForActivation();
}

void UGA_BossP3Attack::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const bool bReplicateEndAbility,
	const bool bWasCancelled)
{
	// 정상 종료에서는 플레이어 경직 몽타주가 끝날 때 GE를 회수한다.
	// 보스가 죽거나 패턴이 강제 취소된 경우에만 즉시 해제한다.
	if (bWasCancelled)
	{
		RemoveAppliedShoutStaggers();
	}
	ShoutTask = nullptr;
	PillarSpawnTask = nullptr;
	MissileFireTask = nullptr;
	MirrorLaunchTask = nullptr;

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UGA_BossP3Attack::OnMirrorLaunchEvent(FGameplayEventData Payload)
{
	AActor* Boss = CurrentActorInfo ? CurrentActorInfo->AvatarActor.Get() : nullptr;
	UWorld* World = IsValid(Boss) ? Boss->GetWorld() : nullptr;
	if (!IsValid(Boss) || !IsValid(World) || !MirrorProjectileClass || !Boss->HasAuthority())
	{
		return;
	}

	const FRotator MirrorRotation = Boss->GetActorRotation() + FRotator(MirrorPitchDegrees, 0.0f, 0.0f);
	const FVector MirrorDirection = MirrorRotation.Vector().GetSafeNormal();
	const FVector SpawnLocation = Boss->GetActorLocation()
		+ Boss->GetActorTransform().TransformVectorNoScale(MirrorSpawnOffset);

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = Boss;
	SpawnParams.Instigator = Cast<APawn>(Boss);
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	ABossWaveProjectile* Mirror = World->SpawnActor<ABossWaveProjectile>(
		MirrorProjectileClass, SpawnLocation, MirrorRotation, SpawnParams);
	if (!IsValid(Mirror))
	{
		return;
	}

	Mirror->InitializeWave(
		MirrorBoxExtent,
		MirrorDirection,
		MirrorSpeed,
		MirrorAttackTag.IsValid() ? MirrorAttackTag : GameplayTagSystem::Action_AI_Boss_P3_MirrorFire,
		MirrorDamageEffectClass);

	if (MirrorLifeSpan > 0.0f)
	{
		Mirror->SetLifeSpan(MirrorLifeSpan);
	}

	ActiveMirror = Mirror;
	SharedPillarDirection = MirrorDirection;
}

void UGA_BossP3Attack::OnPillarSpawnEvent(FGameplayEventData Payload)
{
	AActor* Boss = CurrentActorInfo ? CurrentActorInfo->AvatarActor.Get() : nullptr;
	ABossWaveProjectile* Mirror = ActiveMirror.Get();
	UWorld* World = IsValid(Boss) ? Boss->GetWorld() : nullptr;
	if (!IsValid(Boss) || !IsValid(Mirror) || !IsValid(World) || !PillarClass || !Boss->HasAuthority())
	{
		return;
	}

	FCollisionQueryParams TraceParams(SCENE_QUERY_STAT(BossP3AttackPillar), false, Boss);
	TraceParams.AddIgnoredActor(Mirror);

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = Boss;
	SpawnParams.Instigator = Cast<APawn>(Boss);
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	// 기둥 배치 오프셋은 미러의 Pitch가 적용된 로컬 축이 아니라 보스의 수평 평면을 쓴다.
	// 따라서 X/Y를 바꾸면 기둥 열이 바닥 위에서 앞뒤/좌우로 움직이며, 모든 기둥은
	// 동일한 SharedPillarDirection을 사용해 평행한 상태를 유지한다.
	FVector BossForwardOnGround = Boss->GetActorForwardVector();
	BossForwardOnGround.Z = 0.0f;
	BossForwardOnGround = BossForwardOnGround.GetSafeNormal();
	if (BossForwardOnGround.IsNearlyZero())
	{
		BossForwardOnGround = FVector::ForwardVector;
	}

	FVector BossRightOnGround = Boss->GetActorRightVector();
	BossRightOnGround.Z = 0.0f;
	BossRightOnGround = BossRightOnGround.GetSafeNormal();
	if (BossRightOnGround.IsNearlyZero())
	{
		BossRightOnGround = FVector::RightVector;
	}

	const FVector MirrorStart = Mirror->GetActorLocation();
	for (const FVector& LocalOffset : PillarSpawnOffsets)
	{
		const FVector Start = MirrorStart
			+ BossForwardOnGround * LocalOffset.X
			+ BossRightOnGround * LocalOffset.Y
			+ FVector::UpVector * LocalOffset.Z;
		const FVector End = Start + SharedPillarDirection * PillarTraceLength;

		FHitResult GroundHit;
		if (!World->LineTraceSingleByChannel(GroundHit, Start, End, PillarGroundTraceChannel, TraceParams))
		{
			continue;
		}

		ABossP3AttackPillar* Pillar = World->SpawnActor<ABossP3AttackPillar>(
			PillarClass, FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);
		if (!IsValid(Pillar))
		{
			continue;
		}

		// 잰 길이에 배율을 곱한다. 바닥에 닿는 끝은 두고 미러 쪽 끝을 진행 방향 반대로 당긴다
		const float TracedLength = FVector::Dist(Start, GroundHit.Location);
		const FVector PillarTop = GroundHit.Location - SharedPillarDirection * (TracedLength * PillarLengthScale);
		Pillar->InitializePillar(PillarTop, GroundHit.Location, PillarRadius);
		if (PillarLifeSpan > 0.0f)
		{
			Pillar->SetLifeSpan(PillarLifeSpan);
		}
	}
}

void UGA_BossP3Attack::OnMissileFireEvent(FGameplayEventData Payload)
{
	AActor* Boss = CurrentActorInfo ? CurrentActorInfo->AvatarActor.Get() : nullptr;
	UWorld* World = IsValid(Boss) ? Boss->GetWorld() : nullptr;
	if (!IsValid(Boss) || !IsValid(World) || !MissileProjectileClass || !Boss->HasAuthority())
	{
		return;
	}

	if (MissileBurstPresets.IsEmpty())
	{
		return;
	}

	const int32 PresetIndex = MissileBurstIndex % MissileBurstPresets.Num();
	const TArray<FVector>& SpawnOffsets = MissileBurstPresets[PresetIndex].SpawnOffsets;
	++MissileBurstIndex;

	TArray<AActor*> CharacterCandidates;
	UGameplayStatics::GetAllActorsOfClass(World, ABaseCharacter::StaticClass(), CharacterCandidates);

	TArray<AActor*> Targets;
	const float MaxTargetDistanceSquared = FMath::Square(MissileTargetSearchRadius);
	for (AActor* Candidate : CharacterCandidates)
	{
		UAbilitySystemComponent* CandidateASC =
			UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Candidate);
		if (!IsValid(Candidate)
			|| !IsValid(CandidateASC)
			|| !CandidateASC->HasMatchingGameplayTag(GameplayTagSystem::Character_Player)
			|| CandidateASC->HasMatchingGameplayTag(GameplayTagSystem::State_Dead)
			|| (MissileTargetSearchRadius > 0.0f
				&& FVector::DistSquared(Boss->GetActorLocation(), Candidate->GetActorLocation()) > MaxTargetDistanceSquared))
		{
			continue;
		}

		Targets.Add(Candidate);
	}

	if (Targets.IsEmpty())
	{
		return;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = Boss;
	SpawnParams.Instigator = Cast<APawn>(Boss);
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	for (int32 MissileIndex = 0; MissileIndex < SpawnOffsets.Num(); ++MissileIndex)
	{
		AActor* Target = Targets[MissileIndex % Targets.Num()];
		const FVector SpawnLocation = Boss->GetActorLocation()
			+ Boss->GetActorTransform().TransformVectorNoScale(SpawnOffsets[MissileIndex]);
		const FRotator SpawnRotation = (Target->GetActorLocation() - SpawnLocation).Rotation();

		AMonsterProjectile* Missile = World->SpawnActor<AMonsterProjectile>(
			MissileProjectileClass, SpawnLocation, SpawnRotation, SpawnParams);
		if (!IsValid(Missile))
		{
			continue;
		}

		Missile->SetUseSplitPattern(false);
		Missile->SetDamageEffectClass(MissileDamageEffectClass);
		Missile->SetAttackTag(MissileAttackTag.IsValid() ? MissileAttackTag : GameplayTagSystem::Action_AI_Boss_P3_Missile);
		Missile->LaunchAtTarget(Target, Boss);
	}
}

void UGA_BossP3Attack::OnShoutEvent(FGameplayEventData Payload)
{
	if (bShoutResolved)
	{
		return;
	}
	bShoutResolved = true;

	AActor* Boss = CurrentActorInfo ? CurrentActorInfo->AvatarActor.Get() : nullptr;
	UAbilitySystemComponent* BossASC = CurrentActorInfo ? CurrentActorInfo->AbilitySystemComponent.Get() : nullptr;
	UWorld* World = IsValid(Boss) ? Boss->GetWorld() : nullptr;
	if (!IsValid(Boss) || !IsValid(BossASC) || !IsValid(World) || !ShoutStaggerEffectClass || !Boss->HasAuthority())
	{
		UE_LOG(LogTemp, Warning, TEXT("[ShoutStagger] Skipped | Boss=%s | ASC=%s | Effect=%s | Authority=%s"),
			*GetNameSafe(Boss), *GetNameSafe(BossASC), *GetNameSafe(ShoutStaggerEffectClass.Get()),
			IsValid(Boss) && Boss->HasAuthority() ? TEXT("True") : TEXT("False"));
		return;
	}

	// 한 번의 확률 판정 결과를 모든 대상 플레이어가 공유한다.
	const float ShoutRoll = FMath::FRand();
	if (ShoutRoll > ShoutChance)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ShoutStagger] ChanceFailed | Roll=%.2f | Chance=%.2f"), ShoutRoll, ShoutChance);
		return;
	}

	const FGameplayEffectContextHandle EffectContext = BossASC->MakeEffectContext();
	const FGameplayEffectSpecHandle EffectSpec = BossASC->MakeOutgoingSpec(ShoutStaggerEffectClass, 1.0f, EffectContext);
	if (!EffectSpec.IsValid())
	{
		return;
	}

	TArray<AActor*> Characters;
	UGameplayStatics::GetAllActorsOfClass(World, ABaseCharacter::StaticClass(), Characters);
	const float MaxDistanceSquared = FMath::Square(ShoutTargetSearchRadius);
	for (AActor* Candidate : Characters)
	{
		UAbilitySystemComponent* TargetASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Candidate);
		if (!IsValid(Candidate)
			|| !IsValid(TargetASC)
			|| !TargetASC->HasMatchingGameplayTag(GameplayTagSystem::Character_Player))
		{
			continue;
		}

		const float Distance = FVector::Dist(Boss->GetActorLocation(), Candidate->GetActorLocation());
		if (TargetASC->HasMatchingGameplayTag(GameplayTagSystem::State_Dead)
			|| (ShoutTargetSearchRadius > 0.0f
				&& FVector::DistSquared(Boss->GetActorLocation(), Candidate->GetActorLocation()) > MaxDistanceSquared))
		{
			UE_LOG(LogTemp, Warning, TEXT("[ShoutStagger] TargetRejected | Player=%s | Dead=%s | Distance=%.0f | Radius=%.0f"),
				*GetNameSafe(Candidate),
				TargetASC->HasMatchingGameplayTag(GameplayTagSystem::State_Dead) ? TEXT("True") : TEXT("False"),
				Distance, ShoutTargetSearchRadius);
			continue;
		}

		const FActiveGameplayEffectHandle AppliedHandle = TargetASC->ApplyGameplayEffectSpecToSelf(*EffectSpec.Data.Get());
		UE_LOG(LogTemp, Warning, TEXT("[ShoutStagger] EffectApplied | Player=%s | Success=%s | HasStaggeredTag=%s | Distance=%.0f"),
			*GetNameSafe(Candidate),
			AppliedHandle.IsValid() ? TEXT("True") : TEXT("False"),
			TargetASC->HasMatchingGameplayTag(GameplayTagSystem::State_Debuff_Staggered) ? TEXT("True") : TEXT("False"),
			Distance);
		if (AppliedHandle.IsValid())
		{
			AppliedShoutStaggers.Add({ TargetASC, AppliedHandle });
		}
	}
}

void UGA_BossP3Attack::RemoveAppliedShoutStaggers()
{
	for (const FAppliedShoutStagger& Applied : AppliedShoutStaggers)
	{
		if (UAbilitySystemComponent* TargetASC = Applied.TargetASC.Get())
		{
			TargetASC->RemoveActiveGameplayEffect(Applied.Handle);
		}
	}
	AppliedShoutStaggers.Reset();
}
