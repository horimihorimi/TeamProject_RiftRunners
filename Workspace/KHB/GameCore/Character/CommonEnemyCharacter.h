#pragma once

#include "CoreMinimal.h"
#include "GameCore/Character/MonsterCharacter.h"
#include "GameCore/Enemy/Data/EnemyDataAssets.h"
#include "GameCore/Enemy/Patrol/EnemyPatrolRoute.h"
#include "GameCore/Enemy/Patrol/PatrolArea.h"
#include "ActiveGameplayEffectHandle.h"
#include "GameCore/Interface/EncounterIntroTarget.h"
#include "CommonEnemyCharacter.generated.h"

class UEnemySplitComponent;
class UEnemySplitHealthState;
class IEnemyPatrolRoute;
class UEnemyDefinitionDataAsset;
class UEnemyMovementProfileDataAsset;
class UEnemyPatternComponent;
class UEnemyBeamAimReplicationComponent;
class UEnemyTraitPoolDataAsset;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class ACommonEnemyCharacter;
DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnEnemySplitCommitted, ACommonEnemyCharacter*, ACommonEnemyCharacter*,
                                       ACommonEnemyCharacter*);

UENUM(BlueprintType)
enum class EEncounterEntryMode : uint8
{
	Immediate UMETA(DisplayName = "즉시 시작"),
	Emerge UMETA(DisplayName = "Emerge 등장"),
	Cinematic UMETA(DisplayName = "시네마틱")
};

/**
 * 새 Enemy Framework를 사용하는 모든 적의 공통 Character입니다.
 *
 * 이동 방식은 파생 클래스가 이동 컴포넌트를 지정해 정한다.
 *   AGroundEnemyCharacter : 지상형 (Elite)
 *   AFlyingEnemyCharacter : 비행형 (Melee, Range)
 *   ABossMonsterCharacter : 보스
 * 기존 AGroundMonsterCharacter, AFlyingMonsterCharacter는 이관이 끝나면 삭제한다.
 */
UCLASS(Abstract, Blueprintable)
class TEAMPROJECT_FIN_API ACommonEnemyCharacter : public AMonsterCharacter, public IEncounterIntroTarget
{
	GENERATED_BODY()

public:
	friend class UEnemySplitComponent;

	friend class UEnemyPoolSubsystem;

	ACommonEnemyCharacter(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	UFUNCTION(BlueprintPure, Category = "Enemy|Definition")

	UEnemyDefinitionDataAsset* GetEnemyDefinition() const
	{
		return EnemyDefinition;
	}

	/** 이름은 종류별 Enemy Definition에서 관리합니다. 미설정이면 빈 텍스트를 반환합니다. */
	UFUNCTION(BlueprintPure, Category = "Enemy|UI")
	FText GetEnemyDisplayName() const;

	UFUNCTION(BlueprintPure, Category = "Enemy|Pattern")

	UEnemyPatternComponent* GetEnemyPatternComponent() const
	{
		return EnemyPatternComponent;
	}

	virtual void HandleDamageHitReact(float Damage, AActor* InstigatorActor,
	                                  const FGameplayEffectContextHandle& EffectContext) override;

	const FText& GetActiveTraitDisplayName() const
	{
		return ActiveTraitDisplayName;
	}

	FSimpleMulticastDelegate OnTraitDisplayChanged;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void Serialize(FArchive& Ar) override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Initial Multipliers", meta = (ClampMin = "0.01"))
	float InitialScaleMultiplier = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Initial Multipliers", meta = (ClampMin = "0.01"))
	float InitialHealthMultiplier = 1.f;

	UFUNCTION(BlueprintCallable, Category = "Enemy|Trait")
	bool ApplyAdditionalTrait(const FEnemyTraitEffectDefinition& Trait);

	UFUNCTION(BlueprintPure, Category = "Enemy|Trait")

	TArray<FText> GetActiveTraitNames() const
	{
		return ActiveTraitNames;
	}

	void StartPlacedBattle();

	virtual void EnablePlacedEncounterFlow()
	{
		bPlacedEncounterFlow = true;
	}

	virtual bool RequestEncounter_Implementation(AActor* InstigatorActor) override;

	virtual void FinishEncounterIntro_Implementation() override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Encounter")
	EEncounterEntryMode EncounterEntryMode = EEncounterEntryMode::Immediate;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Encounter",
	          meta = (EditCondition = "EncounterEntryMode == EEncounterEntryMode::Emerge", EditConditionHides))
	FMonsterEmergeParams EncounterEmergeSettings;

	EEncounterEntryMode GetEncounterEntryMode() const;

	// 기존 맵의 설정을 읽어 새 등장 방식으로 이전하기 위한 숨김 속성입니다.
	UPROPERTY(meta = (DeprecatedProperty))
	bool bUseEncounterIntro = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Encounter", meta = (DisplayName = "Track All Players"))
	bool bTrackAllPlayersDuringEncounter = false;

	FOnEnemySplitCommitted OnSplitCommitted;

	UEnemySplitHealthState* GetSharedHealthPool() const;

	UEnemySplitComponent* GetEnemySplitComponent() const
	{
		return EnemySplitComponent;
	}

	virtual void OnReleased() override;

	void PreparePlacedEncounter();

	void OnEncounteredTagChanged(FGameplayTag Tag, int32 Count);

	bool InheritSplitPresentation(const ACommonEnemyCharacter* Parent, bool bCanSplit);

	/*
	 * 이동 컴포넌트가 읽을 현재 이동 설정입니다.
	 *
	 * 초기화 때 Definition의 프로필이 적용되고, 보스처럼 페이즈마다 규칙이 바뀌는 적은
	 * ApplyMovementProfile로 교체한다. 이동 컴포넌트는 보스인지 일반 적인지 알 필요가 없다.
	 */
	UFUNCTION(BlueprintPure, Category = "Enemy|Movement")
	virtual UEnemyMovementProfileDataAsset* GetActiveMovementProfile() const;

	/*
	 * 활성 이동 프로필을 교체하고 이동 컴포넌트에 알립니다.
	 *
	 * 널이면 Definition의 기본 프로필로 되돌린다.
	 */
	UFUNCTION(BlueprintCallable, Category = "Enemy|Movement")
	void ApplyMovementProfile(UEnemyMovementProfileDataAsset* InMovementProfile);

	/*
	 * 스폰한 쪽이 배치 정보를 넘기는 창구입니다.
	 *
	 * 몬스터는 자기를 누가 스폰했는지 모른다. 스폰 지점이든 전투 섹터든 레벨 배치 BP든
	 * 이 함수만 부르면 된다. 경로를 넘기지 않으면 순찰하지 않고 제자리에서 대기한다.
	 *
	 * PCG로 조합되는 맵에서는 몬스터가 런타임에 생성되어 레벨에서 미리 지정할 수 없으므로,
	 * 순찰 경로와 시작 방식은 에디터 기본값이 아니라 이 호출로 정해지는 것이 정상이다.
	 */
	UFUNCTION(BlueprintCallable, Category = "Enemy|Spawn")
	void InitializeSpawnPlacement(const TScriptInterface<IEnemyPatrolRoute>& InPatrolRoute, EEnemyStartMode InStartMode,
	                              int32 InPatrolStartPointIndex = 0);

	UFUNCTION(BlueprintPure, Category = "Enemy|Patrol")
	bool HasPatrolRoute() const;

	UFUNCTION(BlueprintPure, Category = "Enemy|Patrol")

	TScriptInterface<IEnemyPatrolRoute> GetPatrolRoute() const
	{
		return PatrolRoute;
	}

	UFUNCTION(BlueprintPure, Category = "Enemy|Patrol")

	int32 GetPatrolStartPointIndex() const
	{
		return PatrolStartPointIndex;
	}

	// 영역을 받아 두고 보이는 채로 깨운다. 순찰 태스크가 이 영역을 읽는다
	virtual void StartPatrol(const TScriptInterface<IPatrolArea>& Area) override;

	// Emerge로 나온 몹이 플레이어를 놓친 뒤 돌아갈 자리
	virtual void SetPatrolArea(const TScriptInterface<IPatrolArea>& Area) override
	{
		PatrolArea = Area;
	}

	// 순찰 태스크가 읽는 영역. 없으면 제자리에서 대기한다
	TScriptInterface<IPatrolArea> GetPatrolArea() const
	{
		return PatrolArea;
	}

	// StateTree의 Idle이 순찰과 제자리 대기를 고를 때 읽는다
	UFUNCTION(BlueprintPure, Category = "Enemy|Patrol")
	bool HasPatrolArea() const;

	/*
	 * 순찰 지점의 바닥 높이에 더할 값(cm)입니다.
	 *
	 * 순찰 지점은 레벨 바닥에 찍히므로, 비행 적은 그 위를 날아야 한다.
	 * 지상 적은 0이고, 비행 적이 이동 프로필의 비행 높이를 돌려준다.
	 */
	UFUNCTION(BlueprintPure, Category = "Enemy|Patrol")

	virtual float GetPatrolHeightOffset() const
	{
		return 0.0f;
	}

	UFUNCTION(BlueprintPure, Category = "Enemy|Target Lost")

	EEnemyTargetLostBehavior GetTargetLostBehavior() const
	{
		return TargetLostBehavior;
	}

	UFUNCTION(BlueprintPure, Category = "Enemy|Target Lost")

	float GetSearchDuration() const
	{
		return SearchDuration;
	}

protected:
	bool bApplyingInheritedTraits = false;

	bool bPlacedEncounterFlow = false;

	bool bPlacedIntroPending = false;

	TWeakObjectPtr<AActor> PlacedEncounterInstigator;

	FTimerHandle PlacedIntroRetryTimer;

	// 편집, 복제, 맵 재로드 시 크기 미리보기가 누적되지 않도록 값을 저장합니다.
	UPROPERTY()
	float AppliedPreviewScaleMultiplier = 1.f;

	UPROPERTY(ReplicatedUsing = OnRep_ActiveTraitDisplayName, Transient)
	TArray<FText> ActiveTraitNames;

	UPROPERTY(Transient)
	TArray<FEnemyTraitEffectDefinition> ActiveTraitDefinitions;

	UPROPERTY(Transient)
	FEnemyTraitEffectDefinition SplitTraitPresentation;

	void ApplyInitialMultipliers();

	// 풀에서 초기화 전에 설정하고, 분열 자식 획득이 끝나면 해제합니다.
	bool bSkipRandomTraitForAcquire = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Enemy|Split")
	TObjectPtr<UEnemySplitComponent> EnemySplitComponent;

	UPROPERTY(ReplicatedUsing = OnRep_SplitScale)
	FVector SplitScale = FVector::OneVector;

	UFUNCTION()
	void OnRep_SplitScale();

	virtual void BeginPlay() override;

	/** 핫리로드로 UPROPERTY 연결만 끊긴 PatternComponent를 기존 인스턴스에 다시 연결한다. */
	void EnsureEnemyPatternComponent();

	void InitializeEnemyDefinition();

	/** 기존 BP InitStatsRow 초기화 뒤, Definition DA의 행을 다시 적용한다. */
	void ApplyDefinitionInitialStats();

	/** 서버에서 한 번만 변형 여부와 특성 GE를 확정한다. */
	void ApplyRandomEnemyTrait();

	/** 서버가 선택한 특성 오버레이를 복제 상태에 기록하고 즉시 적용한다. */
	void SetTraitOverlay(UMaterialInterface* InMaterial, const FLinearColor& InColor);

	/** 복제된 특성 오버레이를 본체 메시 Overlay Material 슬롯에 적용한다. */
	void ApplyTraitOverlay();

	UFUNCTION()
	void OnRep_ActiveTraitOverlay();

	void SetTraitDisplayName(const FText& InDisplayName);

	UFUNCTION()
	void OnRep_ActiveTraitDisplayName();

	UPROPERTY(ReplicatedUsing = OnRep_ActiveTraitDisplayName, Transient)
	FText ActiveTraitDisplayName;

	void StartStationaryDamageTest();

	/** 스탯, GA 구성, 패턴, 반응, 이동 프로필을 조립하는 새 Framework의 루트 DA입니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Definition")
	TObjectPtr<UEnemyDefinitionDataAsset> EnemyDefinition;

	/** 초기화나 페이즈 전환으로 적용된 현재 이동 프로필입니다. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Enemy|Movement")
	TObjectPtr<UEnemyMovementProfileDataAsset> ActiveMovementProfile = nullptr;

	/** 모든 새 Framework 적이 공유하는 런타임 패턴 선택 상태입니다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Enemy|Pattern", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UEnemyPatternComponent> EnemyPatternComponent;

	/** 서버가 계산한 Beam HitWindow 조준 방향을 클라이언트 연출에 전달한다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Enemy|Combat", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UEnemyBeamAimReplicationComponent> EnemyBeamAimReplicationComponent;

	/** 비어 있으면 이 적은 변형 특성 추첨 대상이 아니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Trait")
	TObjectPtr<UEnemyTraitPoolDataAsset> TraitPool;

	/** 0 이상이면 확률·추첨 없이 TraitPool->Effects의 해당 인덱스 특성을 강제 적용한다. 촬영·테스트용이며 -1이면 평소처럼 추첨한다. */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Enemy|Trait|Test", meta = (ClampMin = "-1"))
	int32 ForcedTraitIndex = -1;

	/** 서버가 추첨한 특성의 Overlay Material. 모든 클라이언트에서 동일하게 표시한다. */
	UPROPERTY(ReplicatedUsing = OnRep_ActiveTraitOverlay, Transient)
	TObjectPtr<UMaterialInterface> ActiveTraitOverlayMaterial;

	UPROPERTY(ReplicatedUsing = OnRep_ActiveTraitOverlay, Transient)
	FLinearColor ActiveTraitOverlayColor = FLinearColor::White;

	/** 각 액터의 색만 따로 넣기 위한 런타임 DMI입니다. */
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> ActiveTraitOverlayMID;

	/*
	 * 이 적이 레벨에 나타나는 방식입니다.
	 *
	 * 레벨 배치 몬스터는 에디터에서 정하고, 스폰되는 몬스터는 InitializeSpawnPlacement가 덮어쓴다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Spawn")
	EEnemyStartMode StartMode = EEnemyStartMode::Dormant;

	/** 타깃을 잃었을 때의 탐색 방식입니다. 돌아갈 곳은 StateTree의 Idle이 정합니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Target Lost")
	EEnemyTargetLostBehavior TargetLostBehavior = EEnemyTargetLostBehavior::StopAndSearch;

	/** 탐색을 포기하고 Idle로 돌아가기까지의 시간(초)입니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Target Lost", meta = (ClampMin = "0.0"))
	float SearchDuration = 5.0f;

	/*
	 * 따라갈 순찰 경로입니다. 비어 있으면 순찰하지 않습니다.
	 *
	 * 스폰 시 정해지므로 저장하지 않는다. 레벨 배치 테스트에서는 BP에서 직접 넣어도 된다.
	 */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Enemy|Patrol")
	TScriptInterface<IEnemyPatrolRoute> PatrolRoute;

	/** 여러 마리가 같은 경로를 쓸 때 서로 다른 지점에서 시작하도록 스폰 시 지정합니다. */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Enemy|Patrol")
	int32 PatrolStartPointIndex = 0;

	/** StartMode에 맞춰 휴면 태그와 표시 상태를 정리한다. */
	void ApplyStartMode();

	// 풀에서 다시 꺼낼 때 스탯·특성·이동 프로필을 새로 스폰된 것처럼 되돌리고 특성을 다시 뽑는다
	virtual void ResetForReuse() override;

	// 스폰 포인트가 넘겨 준 순찰 영역. 풀로 돌아가면 비운다
	UPROPERTY(Transient)
	TScriptInterface<IPatrolArea> PatrolArea;

	// 지금 붙어 있는 특성 GE. 다시 뽑기 전에 이걸로 떼어 낸다
	TArray<FActiveGameplayEffectHandle> ActiveTraitEffectHandles;
};
