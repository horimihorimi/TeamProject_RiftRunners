#include "GC_Invincible.h"
#include "NiagaraComponent.h"
#include "GameFramework/Character.h"
#include "Components/SkeletalMeshComponent.h"

AGC_Invincible::AGC_Invincible()
{
	NiagaraEffect = CreateDefaultSubobject<UNiagaraComponent>(TEXT("NiagaraEffect"));
	RootComponent = NiagaraEffect;

	// 자동 활성화 방지 (OnActive에서 명시적으로 켤 것)
	NiagaraEffect->SetAutoActivate(false);

	// 엔진의 기본 RootComponent 자동 부착을 방지 (우리가 직접 뼈에 붙이기 위함)
	bAutoAttachToOwner = false;
	
	// 버프가 끝나면 액터 자동 파괴
	bAutoDestroyOnRemove = true;
}

bool AGC_Invincible::OnActive_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters)
{
	if (ACharacter* Character = Cast<ACharacter>(MyTarget))
	{
		if (USkeletalMeshComponent* Mesh = Character->GetMesh())
		{
			// 블루프린트에서 설정한 뼈(TargetBoneName)에 강제 스냅
			NiagaraEffect->AttachToComponent(Mesh, FAttachmentTransformRules::SnapToTargetNotIncludingScale, TargetBoneName);
		}
	}

	NiagaraEffect->Activate();

	return Super::OnActive_Implementation(MyTarget, Parameters);
}
