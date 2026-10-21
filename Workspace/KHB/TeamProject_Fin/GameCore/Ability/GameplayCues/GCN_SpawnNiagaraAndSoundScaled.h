#pragma once

#include "CoreMinimal.h"
#include "GCN_SpawnNiagaraAndSound.h"
#include "GameCore/Projectile/MonsterProjectile.h"
#include "GCN_SpawnNiagaraAndSoundScaled.generated.h"

class UNiagaraComponent;

/**
 * 재생할 이펙트와 사운드를 요청 쪽에서 받아오는 큐.
 *
 * 부모(GCN_SpawnNiagaraAndSound)는 에셋을 자기 프로퍼티로만 들고 있어서
 * 큐 태그 하나당 연출이 하나로 고정된다. 여기서는 큐 파라미터의
 * SourceObject로 넘어온 UNiagaraSoundCueData를 우선 사용하므로, 여러
 * 몬스터가 같은 태그를 공유하면서 각자 다른 연출을 낼 수 있다.
 *
 * SourceObject가 없으면 이 클래스의 프로퍼티가 폴백으로 쓰인다.
 * 나이아가라 사용자 파라미터도 채워 넣기 때문에, 크기를 컴포넌트 스케일이
 * 아니라 파라미터로 받는 이펙트에도 대응한다.
 *
 * 부모는 여러 큐가 공유하는 클래스라 건드리지 않았다.
 */
UCLASS(Blueprintable)
class TEAMPROJECT_FIN_API UGCN_SpawnNiagaraAndSoundScaled : public UGCN_SpawnNiagaraAndSound
{
	GENERATED_BODY()

public:
	virtual bool OnExecute_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) const override;

protected:
	/*
	 * SourceObject가 없을 때 쓸 기본 크기 파라미터.
	 * 이름을 비워두면 건드리지 않고 에셋 기본 크기로 나간다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cue|VFX")
	FName ScaleParameterName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cue|VFX", meta = (ClampMin = "0.0"))
	float ScaleParameterValue = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cue|VFX")
	TArray<FNiagaraUserFloatParameter> FloatParameters;

private:
	void ApplyUserParameters(
		UNiagaraComponent* SpawnedEffect,
		const UNiagaraSoundCueData* CueData) const;
};
