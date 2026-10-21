// Copyright Epic Games, Inc. All Rights Reserved.

#include "Test_EnemyCharacter.h"
#include "Components/SkeletalMeshComponent.h"

ATest_EnemyCharacter::ATest_EnemyCharacter()
{
	// 스켈레탈 메시 피격 판정 세팅 (적군용)
	if (USkeletalMeshComponent* MeshComp = GetMesh())
	{
		// 적군은 플레이어 총알(PlayerAttack)에만 Block 판정을 냄
		MeshComp->SetCollisionResponseToChannel(ECC_TraceAttack, ECR_Block);
		// 몬스터끼리의 공격(EnemyAttack)은 완전히 관통(Ignore)시켜 팀킬 길막 방지
	}
}

void ATest_EnemyCharacter::BeginPlay()
{
	Super::BeginPlay();
}
