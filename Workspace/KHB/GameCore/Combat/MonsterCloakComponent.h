#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MonsterCloakComponent.generated.h"

class UMaterialInterface;
class UMaterialInstanceDynamic;
class USkeletalMeshComponent;

/**
 * 은신 구간에만 몬스터 머티리얼을 은신용으로 교체하고 진행도를 보간한다.
 *
 * 평상시에는 원본 머티리얼을 그대로 쓴다. 은신용 머티리얼은 반투명이라
 * 오파시티가 1이어도 원본과 완전히 같게 렌더링되지 않기 때문이다.
 * 은신용 DMI는 BeginPlay에서 미리 만들어 두고 필요할 때만 갈아끼운다.
 */
UCLASS(ClassGroup = Combat, meta = (BlueprintSpawnableComponent))
class TEAMPROJECT_FIN_API UMonsterCloakComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UMonsterCloakComponent();

	virtual void BeginPlay() override;

	virtual void TickComponent(
		float DeltaTime,
		ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

	/** 은신용 머티리얼로 교체하고 0에서 1로 보간한다. */
	UFUNCTION(BlueprintCallable, Category = "Cloak")
	void StartCloak(float Duration);

	/** 1에서 0으로 보간하고, 끝나면 원본 머티리얼로 되돌린다. */
	UFUNCTION(BlueprintCallable, Category = "Cloak")
	void StartReveal(float Duration);

	/** 보간 없이 즉시 적용한다. 0이면 원본 머티리얼로 되돌린다. */
	UFUNCTION(BlueprintCallable, Category = "Cloak")
	void SetCloakProgress(float Progress);

	UFUNCTION(BlueprintPure, Category = "Cloak")
	float GetCloakProgress() const { return CurrentProgress; }

	/** 은신용 머티리얼이 준비됐는지. 실패하면 GA가 은신을 건너뛴다. */
	UFUNCTION(BlueprintPure, Category = "Cloak")
	bool HasCloakMaterials() const { return CloakMaterials.Num() > 0; }

protected:
	/** 은신에 쓸 마스터 머티리얼. M_Cloaking. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cloak")
	TObjectPtr<UMaterialInterface> CloakMaterial;

	/** 원본 머티리얼에서 베이스 컬러 텍스처를 읽어올 파라미터 이름. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cloak")
	FName SourceBaseColorParameter = TEXT("DiffuseColorMap");

	/** 원본 머티리얼에서 이미시브 텍스처를 읽어올 파라미터 이름. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cloak")
	FName SourceEmissiveParameter = TEXT("EmissiveColorMap");

private:
	void StartCloakLocal(float Duration);
	void StartRevealLocal(float Duration);
	void SetCloakProgressLocal(float Progress);

	/** Enemy GA는 ServerOnly이므로, 머티리얼 전환도 모든 클라이언트에 전달한다. */
	UFUNCTION(NetMulticast, Reliable)
	void MulticastStartCloak(float Duration);

	UFUNCTION(NetMulticast, Reliable)
	void MulticastStartReveal(float Duration);

	UFUNCTION(NetMulticast, Reliable)
	void MulticastSetCloakProgress(float Progress);

	void BuildCloakMaterials();
	void ApplyCloakMaterials();
	void RestoreOriginalMaterials();
	void ApplyProgressToMaterials();
	void StartInterpolation(float InTargetProgress, float Duration);

	UPROPERTY()
	TObjectPtr<USkeletalMeshComponent> CachedMesh;

	/** 평상시로 되돌릴 때 쓰는 원본 머티리얼. 슬롯 순서 그대로다. */
	UPROPERTY()
	TArray<TObjectPtr<UMaterialInterface>> OriginalMaterials;

	UPROPERTY()
	TArray<TObjectPtr<UMaterialInstanceDynamic>> CloakMaterials;

	bool bCloakMaterialsApplied = false;

	float CurrentProgress = 0.0f;
	float StartProgress = 0.0f;
	float TargetProgress = 0.0f;
	float ElapsedTime = 0.0f;
	float InterpolationDuration = 0.0f;
};
