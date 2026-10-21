#pragma once

#include "CoreMinimal.h"
#include "GameCore/Character/CommonEnemyCharacter.h"
#include "GameCore/Interface/EncounterIntroTarget.h"
#include "BossMonsterCharacter.generated.h"

class UBossEnemyDefinitionDataAsset;
class UBossPhaseComponent;
class UEnemyMovementProfileDataAsset;
class UCurveFloat;
struct FOnAttributeChangeData;
struct FEnemyBossPhaseDefinition;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBossEncounterIntroRequested, AActor*, EncounterInstigator);

/**
 * 모든 보스가 공유하는 Character 기반 클래스입니다.
 *
 * 고유 이동·공격 로직은 이 클래스에 넣지 않습니다. 보스별 BP가 고유 이동
 * 컴포넌트와 Boss Definition을 조립하고, 이 클래스는 페이즈/사망 공통 진입점만 제공합니다.
 */
UCLASS(Abstract, Blueprintable)
class TEAMPROJECT_FIN_API ABossMonsterCharacter : public ACommonEnemyCharacter
{
	GENERATED_BODY()


public:
	// 기존 보스의 등장 연출과 페이즈 초기화 경로를 유지합니다.
	virtual void EnablePlacedEncounterFlow() override
	{
	}

	ABossMonsterCharacter(const FObjectInitializer& ObjectInitializer);

	UFUNCTION(BlueprintPure, Category = "Boss")

	UBossPhaseComponent* GetBossPhaseComponent() const
	{
		return BossPhaseComponent;
	}

	UFUNCTION(BlueprintPure, Category = "Boss")
	UBossEnemyDefinitionDataAsset* GetBossDefinition() const;

	UFUNCTION(BlueprintPure, Category = "Boss|HUD",
	          meta = (DeprecatedFunction,
	                  DeprecationMessage = "Use GetEnemyDisplayName. Set the name in Enemy Definition."))

	FText GetBossDisplayName() const
	{
		return GetEnemyDisplayName();
	}

	/** 현재 페이즈가 패턴 이동 중 아레나 중심을 바라보도록 설정됐는지 반환합니다. */
	bool ShouldFaceArenaCenter() const;

	/*
	 * 조우 요청을 받습니다.
	 *
	 * 일반 몬스터는 여기서 곧바로 전투를 시작하지만 보스는 등장 연출을 거친다.
	 * 휴면을 유지한 채 연출만 요청하고, 실제 전투 시작은 FinishEncounterIntro가 한다.
	 */
	virtual bool RequestEncounter_Implementation(AActor* EncounterInstigator) override;

	/*
	 * 등장 연출 종료. 휴면을 풀고 최초 타깃을 정해 전투를 시작합니다.
	 *
	 * 컨트롤러나 플레이어가 아직 없으면 잠시 뒤 다시 시도한다.
	 */
	virtual void FinishEncounterIntro_Implementation() override;

	UFUNCTION(BlueprintPure, Category = "Boss|Encounter")

	bool IsEncounterIntroPending() const
	{
		return bEncounterIntroPending;
	}

	/** 조우 요청을 받아 등장 연출이 필요해졌을 때 방송합니다. 연출을 재생하는 쪽이 구독합니다. */
	UPROPERTY(BlueprintAssignable, Category = "Boss|Encounter")
	FOnBossEncounterIntroRequested OnEncounterIntroRequested;

	/** 현재 페이즈 DA의 영구 Form 값을 Mesh와 몸 피격 Capsule에 적용합니다. */
	UFUNCTION(BlueprintCallable, Category = "Boss|Form")
	void ApplyCurrentPhaseForm();

	/** Form Scale 노티파이 구간 시작 시 호출합니다. */
	UFUNCTION(BlueprintCallable, Category = "Boss|Form")
	void BeginBossFormScaleTransition(UCurveFloat* ScaleCurve, float NotifyDuration);

	/** Form Scale 노티파이 구간의 매 프레임 진행값을 반영합니다. */
	UFUNCTION(BlueprintCallable, Category = "Boss|Form")
	void UpdateBossFormScaleTransition(float DeltaSeconds);

	/** Form Scale 노티파이 구간 종료 시 커브의 마지막 값을 확정합니다. */
	UFUNCTION(BlueprintCallable, Category = "Boss|Form")
	void EndBossFormScaleTransition();

	/** 임시 연출에서 사용할 균일 Form 배율을 즉시 적용합니다. */
	UFUNCTION(BlueprintCallable, Category = "Boss|Form")
	void ApplyBossFormUniformScale(float ScaleMultiplier);

	/*
	 * 공전과 앵커 이동의 기준이 되는 아레나 중심입니다.
	 *
	 * DA는 콘텐츠 에셋이라 레벨 액터를 참조할 수 없다. 레벨에 존재하는 폰이 대신
	 * 태그로 찾아 두고 이동 컴포넌트에 넘겨준다.
	 */
	UFUNCTION(BlueprintPure, Category = "Boss|Movement")

	AActor* GetArenaCenter() const
	{
		return ArenaCenter;
	}

protected:
	virtual void BeginPlay() override;

	/*
	 * 켜면 등장 연출 없이 조우 요청 즉시 연출이 끝난 것으로 봅니다.
	 *
	 * 시네마틱이 붙기 전까지의 기본값이다. 시네마틱을 재생하는 쪽이 생기면 끄고,
	 * 그쪽에서 연출 종료 시 FinishEncounterIntro를 호출한다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Encounter")
	bool bSkipEncounterIntro = true;

	/** 연출 종료 시 컨트롤러나 플레이어가 아직 없을 때 다시 시도하는 간격(초). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Encounter", meta = (ClampMin = "0.05"))
	float EncounterStartRetryInterval = 0.25f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Boss", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UBossPhaseComponent> BossPhaseComponent;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Movement")
	FName ArenaCenterTag = TEXT("BossArenaCenter");

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Boss|Movement")
	TObjectPtr<AActor> ArenaCenter = nullptr;

private:
	void CacheBaseFormState();

	void ApplyBossPhaseForm(const FEnemyBossPhaseDefinition& PhaseDefinition);

	/** 현재 페이즈의 이동 프로필 오버라이드를 적용한다. 없으면 Definition 기본값으로 되돌린다. */
	void ApplyCurrentPhaseMovementProfile();

	void ApplyBossFormScaleCurveValue();

	void ResolveArenaCenter();

	void HandleBossHealthChanged(const FOnAttributeChangeData& ChangeData);

	void PreemptPhaseTransitionForHealth();

	void RequestNextPhaseFromCurrentHealth();

	void RetryFinishEncounterIntro();

	/*
	 * Visible로 시작한 보스의 전투를 연다. 플레이어나 그 ASC가 아직 준비되지 않았으면
	 * 조우 경로와 같은 간격으로 다시 시도하고, 최초 타깃을 잡은 뒤에만 StateTree를 시작한다.
	 */
	void TryStartVisibleBattle();

	UFUNCTION()
	void HandleBossPhaseChanged(int32 NewPhaseIndex, FGameplayTag NewPhaseId);

	FVector BaseMeshRelativeScale = FVector::OneVector;

	float BaseCapsuleRadius = 0.0f;

	float BaseCapsuleHalfHeight = 0.0f;

	bool bBaseFormStateCached = false;

	UPROPERTY(Transient)
	TObjectPtr<UCurveFloat> ActiveFormScaleCurve = nullptr;

	float ActiveFormScaleDuration = 0.0f;

	float ActiveFormScaleElapsed = 0.0f;

	bool bPhaseTransitionPreemptionInProgress = false;

	/** 조우 요청을 받고 등장 연출이 끝나기를 기다리는 중인지. */
	bool bEncounterIntroPending = false;

	/** 조우를 일으킨 대상. 연출 종료 시 최초 타깃의 1순위 후보다. */
	UPROPERTY(Transient)
	TObjectPtr<AActor> PendingEncounterInstigator = nullptr;

	FTimerHandle EncounterStartRetryTimerHandle;
};
