#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/DataTable.h"
#include "MonsterDropComponent.generated.h"

class AMonsterCharacter;
class ABasePlayerState;
class AGunObject;

/** 몬스터 종류별 처치 보상. RowName은 몬스터 BP의 CharacterRoleTag와 맞춰서 쓴다. */
USTRUCT(BlueprintType)
struct TEAMPROJECT_FIN_API FMonsterDropRow : public FTableRowBase
{
	GENERATED_BODY()

	/** 처치 시 파티에 분배할 총 골드. 개인 지급액은 기여도 비율로 쪼갠 값이다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gold", meta = (ClampMin = "0"))
	int32 GoldReward = 0;

	/** 0이면 확정 지급. 0.2면 총량을 +-20% 범위에서 굴린다. 기본은 꺼 둔다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gold", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float GoldVariance = 0.0f;

	/** 0~1. 무기 드랍 확률이다. 골드는 처치하면 무조건 나가므로 확률이 없다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float WeaponDropChance = 0.0f;
};

/**
 * 무기 스폰 쪽이 바인딩하는 드랍 요청. 확률 판정까지는 이 컴포넌트가 끝내고,
 * 외부 연출/알림이 바인딩할 수 있는 드랍 완료 알림이다.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FOnWeaponDropRequested,
	AMonsterCharacter*, DeadMonster,
	FTransform, DropTransform);

/**
 * 몬스터가 받은 피해를 플레이어별로 누적해 두었다가, 사망이 확정되는 순간
 * 기여도 비율대로 골드를 즉시 지급한다. 무기는 사망 연출이 끝난 뒤 요청만 흘린다.
 * 판정과 지급이 전부 서버에서 끝나므로 복제하지 않는다.
 */
UCLASS(ClassGroup = (Combat), meta = (BlueprintSpawnableComponent))
class TEAMPROJECT_FIN_API UMonsterDropComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UMonsterDropComponent();

	/** 데미지 라우터가 피격마다 호출한다. 사망타도 포함해서 누적한다. */
	void RecordDamage(AActor* InstigatorActor, float AppliedDamage);

	/** 데미지 라우터가 사망 확정 순간 호출한다. 골드는 사망 몽타주를 기다리지 않고 여기서 지급된다. */
	void NotifyKilled();

	/** 보상 없이 죽일 때(보스 사망으로 애드를 정리할 때 등) 무기 드랍까지 막는다. 풀에서 다시 꺼낼 때 끈다. */
	void SetDropsSuppressed(bool bSuppress) { bDropsSuppressed = bSuppress; }

	/** Shared reward eligibility: pawn death tag, or PlayerState health when no ASC is available. */
	static bool IsPlayerAlive(const ABasePlayerState* PlayerState);

	/** 확률에 당첨돼 무기를 생성한 뒤, 사망 위치를 외부 연출/알림에 알린다. */
	UPROPERTY(BlueprintAssignable, Category = "Drop")
	FOnWeaponDropRequested OnWeaponDropRequested;

protected:
	virtual void BeginPlay() override;

	/** 몬스터 BP마다 지정한다. 비워 두면 이 몬스터는 아무것도 떨어뜨리지 않는다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Drop",
		meta = (RowType = "/Script/TeamProject_Fin.MonsterDropRow"))
	FDataTableRowHandle DropRow;

	/** 적 드랍용으로 데이터가 설정된 GunObject Blueprint. 비워 두면 무기 드랍은 생성하지 않는다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Drop|Weapon")
	TSubclassOf<AGunObject> EnemyDropGunObjectClass;

private:
	UFUNCTION()
	void HandleMonsterDeath(AMonsterCharacter* DeadMonster);

	void PayOutGold(const FMonsterDropRow& Row);
	int32 RollGoldAmount(const FMonsterDropRow& Row) const;

	/** 기여도 집계. 서버 전용이며 복제하지 않는다. 약참조라 PS가 사라져도 안전하다. */
	TMap<TWeakObjectPtr<ABasePlayerState>, float> DamageContributions;

	/** 사망 알림이 두 번 들어와도 두 번 지급하지 않는다. */
	bool bGoldPaidOut = false;

	bool bDropsSuppressed = false;
};
