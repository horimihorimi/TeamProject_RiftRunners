#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "WeaponTypes.h"
#include "SmartTrackingTypes.h"
#include "WeaponAssemblyTypes.h"
#include "WeaponFeedbackTypes.h"
#include "WeaponDataAsset.generated.h"

class UGameplayEffect;
class AWeaponProjectile;
class USoundBase;

UCLASS()
class TEAMPROJECT_FIN_API UWeaponDataAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
	EWeaponCategory WeaponCategory = EWeaponCategory::Rifle;

	// 무기 외형은 항상 이 조립 데이터에서 결정한다.
	// PCG로 생성된 무기는 확정된 프레임/총열 메시를 여기에 제공한다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Assembly")
	FWeaponAssemblyData AssemblyData;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
	FWeaponRuntimeStats RuntimeStats;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Damage")
	TSubclassOf<UGameplayEffect> DamageEffectClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Projectile")
	TSubclassOf<AWeaponProjectile> ProjectileClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Smart Tracking")
	TSubclassOf<AWeaponProjectile> SmartTrackingProjectileClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Smart Tracking")
	FSmartTrackingPathSettings SmartTrackingPath;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Smart Tracking")
	FSmartTrackingTargetingSettings SmartTrackingTargeting;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Smart Tracking")
	FSmartTrackingFireSettings SmartTrackingFire;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Feedback|Sound")
	FWeaponSoundSettings EquipSound;

	// 이 데이터 에셋의 모든 무기 카테고리가 공유하는 재장전 사운드다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Feedback|Sound|Reload")
	TObjectPtr<USoundBase> ReloadStartSound;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Feedback|Sound|Reload", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float ReloadStartVolume = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Feedback|Sound|Reload", meta = (ClampMin = "0.1", UIMin = "0.1"))
	float ReloadStartPitch = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Feedback|Sound|Reload")
	TObjectPtr<USoundBase> ReloadInsertSound;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Feedback|Sound|Reload", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float ReloadInsertVolume = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Feedback|Sound|Reload", meta = (ClampMin = "0.1", UIMin = "0.1"))
	float ReloadInsertPitch = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Feedback|Sound|Reload")
	TObjectPtr<USoundBase> ReloadCompleteSound;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Feedback|Sound|Reload", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float ReloadCompleteVolume = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Feedback|Sound|Reload", meta = (ClampMin = "0.1", UIMin = "0.1"))
	float ReloadCompletePitch = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Feedback")
	FWeaponFeedbackSet RifleFeedback;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Feedback")
	FWeaponFeedbackSet SniperFeedback;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Feedback")
	FWeaponFeedbackSet GrenadeLauncherFeedback;

	const FWeaponFeedbackSet* FindFeedbackSet(EWeaponCategory InWeaponCategory) const;
};
