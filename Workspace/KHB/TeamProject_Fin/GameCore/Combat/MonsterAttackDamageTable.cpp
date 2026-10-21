#include "GameCore/Combat/MonsterAttackDamageTable.h"

namespace
{
	/*
	 * 이미 경고한 태그. 타격마다 로그가 도배되는 것을 막는다.
	 * 밸런싱 중에 표에 빠진 공격을 한 번은 알려야 하므로 침묵하지는 않는다.
	 */
	TSet<FGameplayTag> WarnedMissingTags;
}

UMonsterCombatSettings::UMonsterCombatSettings()
{
	/*
	 * 지정하지 않으면 엔진이 정한 자리에 들어가 프로젝트 세팅에서 찾기 어렵다.
	 * 밸런싱 중에 자주 여는 항목이라 게임 카테고리에 고정한다.
	 */
	CategoryName = TEXT("Game");
}

const FMonsterAttackDamageRow* UMonsterCombatSettings::FindAttackRow(const FGameplayTag& AttackTag)
{
	if (!AttackTag.IsValid())
	{
		/*
		 * 조용히 넘기면 계수 1.0으로 떨어진 채 아무 흔적이 남지 않는다.
		 * 표에 행이 없는 경우와 구분되어야 원인을 찾을 수 있다.
		 */
		static bool bWarnedOnce = false;
		if (!bWarnedOnce)
		{
			bWarnedOnce = true;
			UE_LOG(LogTemp, Warning,
				TEXT("[MonsterCombat] 공격 태그가 비어 있어 계수 1.0으로 처리한다. ")
				TEXT("어빌리티의 Asset Tags 또는 노티파이의 AttackTagOverride를 확인하라."));
		}
		return nullptr;
	}

	const UMonsterCombatSettings* Settings = GetDefault<UMonsterCombatSettings>();
	if (Settings == nullptr)
	{
		return nullptr;
	}

	/*
	 * 소프트 참조라 처음 쓸 때 한 번 로드된다. 테이블은 작고 전투 중 계속
	 * 쓰이므로 그 뒤로는 메모리에 남아 있다.
	 */
	const UDataTable* Table = Settings->AttackDamageTable.LoadSynchronous();
	if (Table == nullptr)
	{
		if (!WarnedMissingTags.Contains(AttackTag))
		{
			WarnedMissingTags.Add(AttackTag);
			UE_LOG(LogTemp, Warning,
				TEXT("[MonsterCombat] AttackDamageTable이 지정되지 않았습니다. ")
				TEXT("프로젝트 세팅 > Monster Combat 에서 지정하세요. Tag=%s"),
				*AttackTag.ToString());
		}
		return nullptr;
	}

	// RowName이 곧 태그 문자열이라 순회 없이 바로 찾는다.
	const FName RowName(*AttackTag.ToString());

	const FMonsterAttackDamageRow* Row =
		Table->FindRow<FMonsterAttackDamageRow>(RowName, TEXT("MonsterAttackDamage"), false);

	if (Row == nullptr && !WarnedMissingTags.Contains(AttackTag))
	{
		WarnedMissingTags.Add(AttackTag);
		UE_LOG(LogTemp, Warning,
			TEXT("[MonsterCombat] 공격 위력 행을 찾지 못했습니다. Table=%s | RowName=%s"),
			*GetNameSafe(Table),
			*RowName.ToString());
	}

	return Row;
}
