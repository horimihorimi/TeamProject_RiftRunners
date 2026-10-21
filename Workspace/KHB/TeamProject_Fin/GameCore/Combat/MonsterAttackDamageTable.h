#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Engine/DeveloperSettings.h"
#include "GameplayTagContainer.h"
#include "GameCore/Ability/DamageSystemLibrary.h"
#include "MonsterAttackDamageTable.generated.h"

/**
 * 몬스터 공격 하나의 위력.
 *
 * RowName은 그 공격의 액션 태그 문자열과 맞춰서 쓴다. FMonsterDropRow가
 * CharacterRoleTag를 RowName으로 쓰는 것과 같은 방식이라, 태그만 알면
 * 순회 없이 바로 찾을 수 있다.
 *
 *   Action.AI.Melee              1.0
 *   Action.AI.Elite.Spin         1.4
 *   Action.AI.Elite.Beam         0.6
 *
 * 몬스터가 얼마나 센지는 여기서 정하지 않는다. 그건 AttackPower가 맡고,
 * 이 표는 "그 공격력의 몇 배인가"만 갖는다. 그래야 몬스터 한 종이 여러
 * 공격을 가져도 표가 커지지 않고, 같은 공격을 여러 몬스터가 공유할 수 있다.
 */
USTRUCT(BlueprintType)
struct TEAMPROJECT_FIN_API FMonsterAttackDamageRow : public FTableRowBase
{
	GENERATED_BODY()

	/**
	 * 이 공격의 스킬 계수.
	 *
	 * 최종 데미지 = 이 값 x 몬스터 고유 공격력(AttackPower) x 방어 경감.
	 * 기본 공격을 1.0으로 두고 나머지를 상대값으로 잡는다.
	 *
	 * 지속 타격은 한 대 기준이 아니라 패턴 전체 합을 타수로 나눠서 정한다.
	 * ReHitInterval마다 반복되는 패턴을 한 대 값으로 잡으면 실제 위력이
	 * 타수만큼 곱해진다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Damage",
		meta = (ClampMin = "0.0"))
	float SkillCoefficient = 1.0f;

	/**
	 * 피격자 방어력을 이만큼 깎고 경감률을 구한다.
	 *
	 * 방어 특화 빌드가 모든 공격을 무의미하게 만드는 것을 막는 장치다. 보스
	 * 패턴 하나쯤은 값을 줘야 방어만 올려서 무적이 되는 상황이 안 생긴다.
	 * 현재 프로젝트에서 이 값을 채우는 곳이 없어 전부 0으로 동작한다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Damage",
		meta = (ClampMin = "0.0"))
	float ArmorPenetration = 0.0f;

	/**
	 * 피격자의 최대 체력에 비례해 추가로 넣을 피해(0~1).
	 *
	 * 계수(SkillCoefficient)와 달리 몬스터 공격력도 방어 경감도 타지 않는다.
	 * "빌드와 무관하게 절반을 날린다" 같은 공격을 표현하기 위한 값이라,
	 * 방어를 태우면 그 정체성이 사라진다.
	 *
	 * 계수와 같이 쓸 수 있다. 둘 다 채우면 계수 피해 위에 더해진다.
	 * 0이면 아무 일도 하지 않는다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Damage",
		meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MaxHealthPercent = 0.0f;

	/**
	 * 피격자에게 유발할 경직.
	 *
	 * 지속 타격은 None으로 둔다. HeavyGroggy는 피격자의 이동 입력을 막기 때문에
	 * 짧은 간격으로 반복되면 빠져나갈 방법이 사라진다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Damage")
	EDamageGroggyType GroggyType = EDamageGroggyType::None;
};

/**
 * 몬스터 전투 관련 전역 설정.
 *
 * 공격 위력표를 노티파이마다 물리지 않고 여기 한 곳에 둔다. 공격이 늘 때마다
 * 에셋을 다시 지정하는 방식이면 빠뜨리기 쉽고, 빠뜨려도 조용히 기본값으로
 * 넘어가서 밸런싱 중에 원인을 찾기 어렵다.
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Monster Combat"))
class TEAMPROJECT_FIN_API UMonsterCombatSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UMonsterCombatSettings();

	/** FMonsterAttackDamageRow로 만든 데이터 테이블. */
	UPROPERTY(Config, EditAnywhere, Category = "Damage",
		meta = (AllowedClasses = "/Script/Engine.DataTable"))
	TSoftObjectPtr<UDataTable> AttackDamageTable;

	/**
	 * 태그에 해당하는 행을 찾는다. 없으면 nullptr.
	 *
	 * 찾지 못하면 경고를 남긴다. 같은 태그로는 한 번만 남기므로 타격마다
	 * 로그가 도배되지 않는다. 조용히 기본값으로 넘어가면 밸런싱 중에
	 * 어느 공격이 표에 빠졌는지 알 수 없다.
	 */
	static const FMonsterAttackDamageRow* FindAttackRow(const FGameplayTag& AttackTag);
};
