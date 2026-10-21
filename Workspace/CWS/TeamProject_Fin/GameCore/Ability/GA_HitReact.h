// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GA_HitReact.generated.h"

/**
 * 피격 및 그로기(Groggy) 반응을 처리하는 부모 어빌리티 클래스
 * 블루프린트에서 이 클래스를 상속받아 몽타주를 재생합니다.
 */
UCLASS()
class TEAMPROJECT_FIN_API UGA_HitReact : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_HitReact();

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

protected:
	/** 
	 * 타격자(Instigator)의 위치를 기반으로 피격 방향을 계산하여 
	 * 몽타주 섹션 이름(Front, Back, Left, Right)을 반환합니다.
	 */
	FName ComputeHitDirectionSection(const FGameplayEventData* TriggerEventData);

	// 약한 그로기 몽타주
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HitReact")
	class UAnimMontage* HitReactMontage;

	// 강한 그로기 몽타주
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HitReact|Heavy")
	class UAnimMontage* HeavyHitReactMontage;

	// 강한 그로기일 때 뒤로 밀려나는 힘
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "HitReact|Heavy")
	float KnockbackForce = 1500.0f;

	// 강한 그로기일 때 밀려나는 시간 (초)
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "HitReact|Heavy")
	float KnockbackDuration = 0.25f;

	// 각 방향별 몽타주 섹션 이름 (블루프린트에서 커스텀 가능하도록 노출)
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "HitReact|Sections")
	FName SectionFront = FName("Front");

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "HitReact|Sections")
	FName SectionBack = FName("Back");

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "HitReact|Sections")
	FName SectionLeft = FName("Left");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HitReact|Sections")
	FName SectionRight = FName("Right");

private:
	UFUNCTION()
	void OnMontageCompleted();

	UFUNCTION()
	void OnMontageInterrupted();
};
