#include "WeaponDataAsset.h"

const FWeaponFeedbackSet* UWeaponDataAsset::FindFeedbackSet(EWeaponCategory InWeaponCategory) const
{
	switch (InWeaponCategory)
	{
	case EWeaponCategory::Rifle:
		return &RifleFeedback;
	case EWeaponCategory::SniperRifle:
		return &SniperFeedback;
	case EWeaponCategory::GrenadeLauncher:
		return &GrenadeLauncherFeedback;
	default:
		return nullptr;
	}
}
