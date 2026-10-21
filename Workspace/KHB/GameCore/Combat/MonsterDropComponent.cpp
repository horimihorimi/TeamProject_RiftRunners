#include "MonsterDropComponent.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "GameCore/BasePlayerState.h"
#include "GameCore/Character/MonsterCharacter.h"
#include "GameCore/Tags/GameplayTagSystem.h"
#include "GameCore/Weapon/World/GunObject.h"
#include "GameFramework/Controller.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"

namespace
{
	/** 데미지 컨텍스트의 Instigator는 폰일 수도, 컨트롤러일 수도, 투사체일 수도 있다. */
	ABasePlayerState* ResolvePlayerState(AActor* Actor)
	{
		if (!Actor)
		{
			return nullptr;
		}

		if (ABasePlayerState* AsPlayerState = Cast<ABasePlayerState>(Actor))
		{
			return AsPlayerState;
		}

		if (const APawn* AsPawn = Cast<APawn>(Actor))
		{
			return Cast<ABasePlayerState>(AsPawn->GetPlayerState());
		}

		if (const AController* AsController = Cast<AController>(Actor))
		{
			return Cast<ABasePlayerState>(AsController->PlayerState.Get());
		}

		// 투사체처럼 자기 자신은 플레이어가 아닌 경우. 한 단계만 거슬러 올라간다.
		if (const APawn* InstigatorPawn = Actor->GetInstigator())
		{
			return Cast<ABasePlayerState>(InstigatorPawn->GetPlayerState());
		}

		return nullptr;
	}
}

UMonsterDropComponent::UMonsterDropComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

	// 집계도 지급도 서버에서 끝난다. 클라에는 껍데기만 있으면 된다.
	SetIsReplicatedByDefault(false);
}

void UMonsterDropComponent::BeginPlay()
{
	Super::BeginPlay();

	AMonsterCharacter* Monster = Cast<AMonsterCharacter>(GetOwner());
	if (!Monster || !Monster->HasAuthority())
	{
		return;
	}

	// 골드는 NotifyKilled에서 이미 나갔다. 이 훅은 무기 드랍 요청 전용이다.
	Monster->OnMonsterDeath.AddDynamic(this, &UMonsterDropComponent::HandleMonsterDeath);
}

void UMonsterDropComponent::RecordDamage(AActor* InstigatorActor, float AppliedDamage)
{
	// 지급이 끝난 뒤 들어오는 시체 타격은 기여도에 넣지 않는다.
	if (AppliedDamage <= 0.0f || bGoldPaidOut)
	{
		return;
	}

	// 몬스터 자폭이나 몬스터 간 팀킬은 여기서 걸러진다. 보상 대상이 아니다.
	ABasePlayerState* PlayerState = ResolvePlayerState(InstigatorActor);
	if (!PlayerState)
	{
		return;
	}

	DamageContributions.FindOrAdd(PlayerState) += AppliedDamage;
}

void UMonsterDropComponent::NotifyKilled()
{
	if (bGoldPaidOut || bDropsSuppressed)
	{
		return;
	}
	bGoldPaidOut = true;

	if (DropRow.IsNull())
	{
		UE_LOG(LogTemp, Verbose,
			TEXT("[MonsterDrop] DropRow 미지정 | Monster=%s"), *GetNameSafe(GetOwner()));
		return;
	}

	if (const FMonsterDropRow* Row = DropRow.GetRow<FMonsterDropRow>(GetNameSafe(GetOwner())))
	{
		PayOutGold(*Row);
	}
}

void UMonsterDropComponent::PayOutGold(const FMonsterDropRow& Row)
{
	const int32 TotalGold = RollGoldAmount(Row);
	if (TotalGold <= 0)
	{
		return;
	}

	// 살아 있는 기여자만 남긴다. 죽은 사람 몫은 버려지는 게 아니라,
	// 아래에서 TotalWeight를 산 사람들로만 잡기 때문에 자동으로 재분배된다.
	TArray<TPair<ABasePlayerState*, float>> Eligible;
	float TotalWeight = 0.0f;
	float ExcludedWeight = 0.0f;
	for (const TPair<TWeakObjectPtr<ABasePlayerState>, float>& Contribution : DamageContributions)
	{
		ABasePlayerState* PlayerState = Contribution.Key.Get();
		if (!PlayerState || Contribution.Value <= 0.0f)
		{
			continue;
		}

		if (!IsPlayerAlive(PlayerState))
		{
			ExcludedWeight += Contribution.Value;
			UE_LOG(LogTemp, Warning,
				TEXT("[MonsterDrop] 제외(사망) | Monster=%s | PS=%s | Nickname=%s | Damage=%.1f"),
				*GetNameSafe(GetOwner()),
				*GetNameSafe(PlayerState),
				*PlayerState->GetLoadout().PlayerName,
				Contribution.Value);
			continue;
		}

		Eligible.Emplace(PlayerState, Contribution.Value);
		TotalWeight += Contribution.Value;
	}

	// 기여자가 전멸했거나, 애초에 플레이어가 죽인 몬스터가 아니다.
	if (Eligible.IsEmpty() || TotalWeight <= 0.0f)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[MonsterDrop] 지급 대상 없음 | Monster=%s | TotalGold=%d | 사망자기여도=%.1f"),
			*GetNameSafe(GetOwner()),
			TotalGold,
			ExcludedWeight);
		return;
	}

	// 기여도 내림차순. 내림에서 깎인 잔돈을 최대 기여자에게 몰아주기 위한 정렬이다.
	Eligible.Sort([](const TPair<ABasePlayerState*, float>& Lhs, const TPair<ABasePlayerState*, float>& Rhs)
	{
		return Lhs.Value > Rhs.Value;
	});

	UE_LOG(LogTemp, Warning,
		TEXT("[MonsterDrop] 지급 시작 | Monster=%s | Row=%s | TotalGold=%d | 대상=%d명 | 유효기여도=%.1f | 제외기여도=%.1f"),
		*GetNameSafe(GetOwner()),
		*DropRow.RowName.ToString(),
		TotalGold,
		Eligible.Num(),
		TotalWeight,
		ExcludedWeight);

	int32 Distributed = 0;
	for (const TPair<ABasePlayerState*, float>& Entry : Eligible)
	{
		const float Ratio = Entry.Value / TotalWeight;
		const int32 Share = FMath::FloorToInt(TotalGold * Ratio);
		Entry.Key->AddGold(Share);
		Distributed += Share;

		UE_LOG(LogTemp, Warning,
			TEXT("[MonsterDrop]   PS=%s | Nickname=%s | Damage=%.1f (%.1f%%) | 지급=%d | 보유=%d"),
			*GetNameSafe(Entry.Key),
			*Entry.Key->GetLoadout().PlayerName,
			Entry.Value,
			Ratio * 100.0f,
			Share,
			Entry.Key->GetGold());
	}

	// 내림 때문에 남은 잔돈. 총 지급액이 GoldReward와 어긋나지 않게 붙여 준다.
	const int32 Remainder = TotalGold - Distributed;
	if (Remainder > 0)
	{
		Eligible[0].Key->AddGold(Remainder);

		UE_LOG(LogTemp, Warning,
			TEXT("[MonsterDrop]   잔돈 %d -> 최대기여자 PS=%s | 보유=%d"),
			Remainder,
			*GetNameSafe(Eligible[0].Key),
			Eligible[0].Key->GetGold());
	}
}

int32 UMonsterDropComponent::RollGoldAmount(const FMonsterDropRow& Row) const
{
	if (Row.GoldReward <= 0)
	{
		return 0;
	}

	// 기본은 확정 지급이다. 기여도 분배만으로도 사람마다 액수가 갈리므로,
	// 여기에 난수를 더 얹으면 플레이어가 편차의 원인을 읽을 수 없게 된다.
	if (Row.GoldVariance <= 0.0f)
	{
		return Row.GoldReward;
	}

	const float Spread = Row.GoldReward * Row.GoldVariance;
	return FMath::Max(1, FMath::RoundToInt(Row.GoldReward + FMath::FRandRange(-Spread, Spread)));
}

bool UMonsterDropComponent::IsPlayerAlive(const ABasePlayerState* PlayerState)
{
	if (!PlayerState)
	{
		return false;
	}

	if (APawn* Pawn = PlayerState->GetPawn())
	{
		if (const UAbilitySystemComponent* ASC =
			UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Pawn))
		{
			return !ASC->HasMatchingGameplayTag(GameplayTagSystem::State_Dead);
		}
	}

	// 폰이 없거나 ASC를 못 얻는 경우. PlayerState의 체력 사본으로 판정한다.
	return PlayerState->GetHealth() > 0.0f;
}

void UMonsterDropComponent::HandleMonsterDeath(AMonsterCharacter* DeadMonster)
{
	if (bDropsSuppressed || !DeadMonster || !DeadMonster->HasAuthority() || DropRow.IsNull())
	{
		return;
	}

	const FMonsterDropRow* Row = DropRow.GetRow<FMonsterDropRow>(GetNameSafe(DeadMonster));
	if (!Row || Row->WeaponDropChance <= 0.0f)
	{
		return;
	}

	if (FMath::FRand() > Row->WeaponDropChance)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[MonsterDrop] 무기 드랍 실패 | Monster=%s | Chance=%.2f"),
			*GetNameSafe(DeadMonster), Row->WeaponDropChance);
		return;
	}

	if (!EnemyDropGunObjectClass)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[MonsterDrop] 무기 드랍 생성 불가 | Monster=%s | GunObjectClass 미지정"),
			*GetNameSafe(DeadMonster));
		return;
	}

	FTransform DropTransform = DeadMonster->GetActorTransform();
	DropTransform.AddToTranslation(FVector(0.0f, 0.0f, 35.0f));
	AGameStateBase* GameState = GetWorld() ? GetWorld()->GetGameState() : nullptr;
	if (!GameState)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[MonsterDrop] 무기 드랍 생성 실패 | Monster=%s | GameState 없음"),
			*GetNameSafe(DeadMonster));
		return;
	}

	int32 SpawnedCount = 0;
	for (APlayerState* PlayerState : GameState->PlayerArray)
	{
		ABasePlayerState* BasePlayerState = Cast<ABasePlayerState>(PlayerState);
		APlayerController* PlayerController = BasePlayerState
			? Cast<APlayerController>(BasePlayerState->GetOwner())
			: nullptr;
		if (!PlayerController || !IsPlayerAlive(BasePlayerState))
		{
			continue;
		}

		if (AGunObject::SpawnRandomEnemyDrop(this, PlayerController, DropTransform, EnemyDropGunObjectClass))
		{
			++SpawnedCount;
		}
	}

	UE_LOG(LogTemp, Warning,
		TEXT("[MonsterDrop] 개인 무기 드랍 생성 | Monster=%s | Count=%d | Chance=%.2f | Listeners=%d"),
		*GetNameSafe(DeadMonster),
		SpawnedCount,
		Row->WeaponDropChance,
		OnWeaponDropRequested.IsBound() ? 1 : 0);

	// 스폰은 무기 쪽 몫이다. 여기서는 "어디에" 까지만 넘긴다.
	// 몬스터는 0.1초 뒤 파괴되므로 스폰한 액터를 몬스터에 어태치하면 안 된다.
	OnWeaponDropRequested.Broadcast(DeadMonster, DropTransform);
}
