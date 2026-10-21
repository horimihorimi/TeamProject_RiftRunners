#pragma once

#include "CoreMinimal.h"
#include "GameplayCueNotify_Actor.h"
#include "GC_Invincible.generated.h"

class UNiagaraComponent;

/**
 * 무적 이펙트를 캐릭터의 pelvis 뼈에 강제 부착하는 C++ GameplayCue 클래스
 */
UCLASS()
class TEAMPROJECT_FIN_API AGC_Invincible : public AGameplayCueNotify_Actor
{
	GENERATED_BODY()

public:
	AGC_Invincible();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Effect")
	TObjectPtr<UNiagaraComponent> NiagaraEffect;

	/** 부착할 뼈(소켓) 이름. 블루프린트에서 자유롭게 변경 가능합니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Effect")
	FName TargetBoneName = FName("pelvis");

protected:
	virtual bool OnActive_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) override;
};
