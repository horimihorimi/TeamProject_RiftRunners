#pragma once

#include "GameCore/Ability/GA_EnemyPatternBase.h"
#include "CoreMinimal.h"
#include "GA_RangedAttack.generated.h"

class AMonsterProjectile;
class UAnimMontage;

/**
 * 몬스터 원거리 GA의 공통 설정값을 제공하는 베이스 클래스.
 * 실제 발사 이벤트와 투사체 패턴은 Phantom/Sentinel 전용 GA가 처리한다.
 */
UCLASS()
class TEAMPROJECT_FIN_API UGA_RangedAttack : public UGA_EnemyPatternBase
{
	GENERATED_BODY()

public:
	UGA_RangedAttack();

protected:
	virtual UAnimMontage* GetPatternMontage() const override { return AttackMontage; }

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ranged")
	TObjectPtr<UAnimMontage> AttackMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ranged")
	TSubclassOf<AMonsterProjectile> ProjectileClass;

	// 오른손/왼손 발사 몽타주 섹션 이름.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ranged")
	FName RightSectionName = TEXT("Right");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ranged")
	FName LeftSectionName = TEXT("Left");

	// 투사체가 나갈 메시 소켓. 스켈레톤에 이 이름으로 소켓을 만들어 둬야 한다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ranged")
	FName RightMuzzleSocketName = TEXT("Bone_PH_Hand_R");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ranged")
	FName LeftMuzzleSocketName = TEXT("Bone_PH_Hand_L");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ranged|Damage")
	TSubclassOf<UGameplayEffect> DamageEffectClass;

};
