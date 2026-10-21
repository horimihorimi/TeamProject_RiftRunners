#include "GA_SnowHazardManager.h"
#include "AbilitySystemComponent.h"
#include "../Tags/GameplayTagSystem.h"

UGA_SnowHazardManager::UGA_SnowHazardManager()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly; // 패시브 GE 부여는 서버에서만 수행
}

void UGA_SnowHazardManager::OnAvatarSet(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec)
{
	Super::OnAvatarSet(ActorInfo, Spec);

	// 어빌리티 부여 시, 서버라면 즉시 활성화하여 인스턴스를 생성하고 패시브로 동작하게 합니다.
	if (ActorInfo->AbilitySystemComponent.IsValid() && ActorInfo->AbilitySystemComponent->GetOwnerRole() == ROLE_Authority)
	{
		ActorInfo->AbilitySystemComponent->TryActivateAbility(Spec.Handle, false);
	}
}

void UGA_SnowHazardManager::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();
	if (ASC && HasAuthority(&ActivationInfo))
	{
		// 인스턴스화된 후, 태그 변경 이벤트 리스너 등록
		ASC->RegisterGameplayTagEvent(GameplayTagSystem::State_Environment_Snow_Level1, EGameplayTagEventType::NewOrRemoved).AddUObject(this, &UGA_SnowHazardManager::OnSnowTagChanged);
		ASC->RegisterGameplayTagEvent(GameplayTagSystem::State_Environment_Snow_Level2, EGameplayTagEventType::NewOrRemoved).AddUObject(this, &UGA_SnowHazardManager::OnSnowTagChanged);
		ASC->RegisterGameplayTagEvent(GameplayTagSystem::State_Environment_Snow_Level3, EGameplayTagEventType::NewOrRemoved).AddUObject(this, &UGA_SnowHazardManager::OnSnowTagChanged);

		// 어빌리티 활성화 시점에 이미 붙어 있는 태그가 있다면 즉시 적용
		if (ASC->HasMatchingGameplayTag(GameplayTagSystem::State_Environment_Snow_Level1))
		{
			OnSnowTagChanged(GameplayTagSystem::State_Environment_Snow_Level1, 1);
		}
		if (ASC->HasMatchingGameplayTag(GameplayTagSystem::State_Environment_Snow_Level2))
		{
			OnSnowTagChanged(GameplayTagSystem::State_Environment_Snow_Level2, 1);
		}
		if (ASC->HasMatchingGameplayTag(GameplayTagSystem::State_Environment_Snow_Level3))
		{
			OnSnowTagChanged(GameplayTagSystem::State_Environment_Snow_Level3, 1);
		}
	}
}

void UGA_SnowHazardManager::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (ASC && HasAuthority(&ActivationInfo))
	{
		// 부여했던 모든 눈 디버프 해제
		for (auto& Pair : ActiveSnowDebuffs)
		{
			ASC->RemoveActiveGameplayEffect(Pair.Value);
		}
		ActiveSnowDebuffs.Empty();
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UGA_SnowHazardManager::OnSnowTagChanged(const FGameplayTag CallbackTag, int32 NewCount)
{
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (!ASC)
	{
		return;
	}

	if (NewCount > 0)
	{
		// 태그가 추가됨: GE 적용
		if (SnowDebuffEffects.Contains(CallbackTag) && !ActiveSnowDebuffs.Contains(CallbackTag))
		{
			TSubclassOf<UGameplayEffect> GEClass = SnowDebuffEffects[CallbackTag];
			if (GEClass)
			{
				FGameplayEffectContextHandle ContextHandle = ASC->MakeEffectContext();
				ContextHandle.AddInstigator(GetAvatarActorFromActorInfo(), GetAvatarActorFromActorInfo());
				FGameplayEffectSpecHandle SpecHandle = ASC->MakeOutgoingSpec(GEClass, 1.0f, ContextHandle);
				
				if (SpecHandle.IsValid())
				{
					FActiveGameplayEffectHandle ActiveHandle = ASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
					ActiveSnowDebuffs.Add(CallbackTag, ActiveHandle);
				}
			}
		}
	}
	else
	{
		// 태그가 제거됨: GE 해제
		if (ActiveSnowDebuffs.Contains(CallbackTag))
		{
			ASC->RemoveActiveGameplayEffect(ActiveSnowDebuffs[CallbackTag]);
			ActiveSnowDebuffs.Remove(CallbackTag);
		}
	}
}
