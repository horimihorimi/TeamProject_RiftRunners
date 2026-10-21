#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "Engine/DataAsset.h"
#include "Engine/DataTable.h"
#include "GameplayTagContainer.h"
#include "EnemyDataAssets.generated.h"

class UEnemyAbilitySetDataAsset;
class UEnemyPatternSetDataAsset;
class UEnemyMovementProfileDataAsset;
class UBossPhaseSetDataAsset;
class UGameplayEffect;
class UMaterialInterface;
class UAnimMontage;

/**
 * 패턴이 이동 컴포넌트에 요청하는 전술적 이동 의도입니다.
 *
 * 새 값은 항상 끝에 추가한다. 중간에 끼우면 이미 저장된 DA의 열거값이 밀린다.
 */
UENUM(BlueprintType)
enum class EEnemyMovementIntent : uint8
{
	None = 0 UMETA(DisplayName = "None"),
	// 기존 DA에 저장된 값 1을 보존하기 위한 숨김 레거시 값이다.
	// 제거하면 뒤의 열거값이 밀려 기존 패턴의 Intent가 손상된다.
	HoldPosition = 1 UMETA(Hidden),
	ApproachTarget = 2 UMETA(DisplayName = "Approach Target"),
	MoveToAnchor = 3 UMETA(DisplayName = "Move To Anchor"),

	/*
	 * 앵커를 중심으로 공전한다.
	 *
	 * 공전은 도착 개념이 없어 시전 이동이 아니라 패턴 사이의 기본 움직임으로 옮긴다.
	 * 기존 DA에 저장된 값 5를 보존하기 위해 남겨 두고 선택지에서만 숨긴다.
	 * 남아 있는 데이터는 이동 프로필의 Orbit 공전 시간으로 끝난다.
	 */
	OrbitAnchor = 5 UMETA(Hidden),

	// 수평 이동 없이 목표 높이까지만 맞춘다.
	MatchHeight = 6 UMETA(DisplayName = "Match Height"),

	// 아레나 바닥 높이까지 강하한다.
	DescendToFloor = 7 UMETA(DisplayName = "Descend To Floor"),

	/*
	 * 이 패턴의 위치 조건(MonsterCombatPositioningComponent의 ActionProfiles)을 확인하고,
	 * 맞지 않으면 프로필에 지정된 재배치 GA(순간이동 등)를 실행한다.
	 *
	 * 프로필은 패턴의 AbilityTag로 찾는다. 위치 보정 컴포넌트가 없는 적에게는 쓰지 않는다.
	 */
	RepositionForAction = 8 UMETA(DisplayName = "Reposition For Action")
};

/** 패턴의 거리 조건을 잴 때 고도 차이를 포함할지 정합니다. */
UENUM(BlueprintType)
enum class EEnemyDistanceMeasure : uint8
{
	/** 수평 거리만 잰다. 고도는 패턴의 높이 조건으로 따로 본다. */
	Horizontal = 0 UMETA(DisplayName = "Horizontal"),
	/** 고도 차이를 포함한 3D 거리를 잰다. */
	Full3D = 1 UMETA(DisplayName = "3D")
};

/** 현재 행동을 외부 이벤트가 끊을 수 있는 범위입니다. */
UENUM(BlueprintType)
enum class EEnemyActionInterruptPolicy : uint8
{
	/*
	 * 끊어도 되는 행동이다.
	 *
	 * 시전 중 다른 상태로 넘어가면 GA를 취소하고, 시전 중에도 회피 요청을 받는다.
	 * 짧은 공격에 쓴다.
	 */
	CancelImmediately UMETA(DisplayName = "Cancel Immediately"),

	/*
	 * 끝까지 재생해야 하는 행동이다.
	 *
	 * 상태가 바뀌어도 GA를 끊지 않고, 시전 중에는 회피도 하지 않는다.
	 * 빙의처럼 대상과 물려 있거나, 중간에 끊기면 연출이 깨지는 행동에 쓴다.
	 */
	FinishCommittedAction UMETA(DisplayName = "Finish Committed Action")
};

/*
 * 타깃을 잃은 적이 탐색하는 방식입니다.
 *
 * 값은 배치마다 달라서 Definition DA가 아니라 ACommonEnemyCharacter가 들고 있다.
 * 탐색이 끝난 뒤 돌아갈 곳은 이 값이 정하지 않는다. StateTree의 Idle이 순찰 경로 유무로 고른다.
 */
UENUM(BlueprintType)
enum class EEnemyTargetLostBehavior : uint8
{
	// 제자리에서 탐색한다.
	StopAndSearch UMETA(DisplayName = "Stop And Search"),
	// 탐색하지 않고 곧바로 휴면으로 돌아간다.
	ReturnToDormant UMETA(DisplayName = "Return To Dormant")
};

/** 적이 레벨에 나타나는 방식입니다. 배치마다 달라서 스폰 지점이 정해 줍니다. */
UENUM(BlueprintType)
enum class EEnemyStartMode : uint8
{
	// 숨은 채로 시작해 조우 시 Emerge로 등장한다. 기존 방식이다.
	Dormant UMETA(DisplayName = "Dormant"),
	// 처음부터 보이고 바로 플레이어를 인지한다. 순찰은 순찰 경로가 있을 때만 돈다.
	Visible UMETA(DisplayName = "Visible")
};

/** Definition이 ASC에 부여할 GA 한 개입니다. */
USTRUCT(BlueprintType)
struct TEAMPROJECT_FIN_API FEnemyAbilityGrantDefinition
{
	GENERATED_BODY()


	/** PatternSet과 StateTree가 이 GA를 찾을 때 사용하는 태그입니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Ability")
	FGameplayTag AbilityTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Ability")
	TSubclassOf<UGameplayAbility> AbilityClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Ability", meta = (ClampMin = "1"))
	int32 GrantLevel = 1;
};

/** 변형 몹으로 선정됐을 때 하나를 추첨할 특성 GE 후보입니다. */
USTRUCT(BlueprintType)
struct TEAMPROJECT_FIN_API FEnemyTraitEffectDefinition
{
	GENERATED_BODY()


	/** 머리 위에 표시할 특성 이름. 비어 있으면 특성 UI를 숨긴다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Trait|UI")
	FText DisplayName;

	/** 이 특성에 적용할 GE 목록. 배열 순서대로 한 번씩 적용한다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Trait")
	TArray<TSubclassOf<UGameplayEffect>> EffectClasses;

	/** 후보 간 선택 비중. 0 이하면 추첨 대상에서 제외한다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Trait", meta = (ClampMin = "0.0"))
	float Weight = 1.0f;

	/** 특성에 선택됐을 때 본체 메시의 Overlay Material 슬롯에 적용할 머티리얼입니다. 비어 있으면 외형은 바꾸지 않습니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Trait|Overlay")
	TObjectPtr<UMaterialInterface> OverlayMaterial = nullptr;

	/** OverlayMaterial의 Color 벡터 파라미터에 넣을 특성 색입니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Trait|Overlay")
	FLinearColor OverlayColor = FLinearColor::White;
};

/**
 * 패턴이 실행 전에 고를 수 있는 이동 후보 한 개입니다.
 *
 * 한 패턴이 여러 후보를 가지면 진입할 때 Weight로 하나를 뽑는다. 같은 Intent 라도
 * 패턴마다, 심지어 같은 패턴 안에서도 몽타주 병행 여부가 달라질 수 있어
 * 타이밍을 패턴이 아니라 후보마다 둔다.
 */
USTRUCT(BlueprintType)
struct TEAMPROJECT_FIN_API FEnemyPatternMovementOption
{
	GENERATED_BODY()


	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement")
	EEnemyMovementIntent Intent = EEnemyMovementIntent::None;

	/*
	 * 참이면 GA 몽타주를 재생하면서 이동한다.
	 *
	 * 이 경우 Prepare는 대기하지 않고 즉시 통과하고, 이동은 몽타주의 노티파이가 끊는다.
	 * 거짓이면 이동이 끝난 뒤에 몽타주를 재생한다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement")
	bool bMoveDuringAbility = false;

	/*
	 * Move During Ability 중 목적지에 도달했을 때 점프할 몽타주 섹션입니다.
	 * None이면 기존처럼 이동만 종료하며, 기존 패턴 데이터의 동작은 변하지 않습니다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement", meta = (EditCondition = "bMoveDuringAbility"))
	FName ArrivalJumpSectionName = NAME_None;

	/** Arrival Jump을 예약할 때 반복 중인 몽타주 섹션 이름입니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement", meta = (EditCondition = "bMoveDuringAbility"))
	FName ArrivalLoopSectionName = NAME_None;

	/** 이동과 준비 연출에 사용할 ApproachLoop의 반복 횟수입니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement",
	          meta = (EditCondition = "bMoveDuringAbility", ClampMin = "1"))
	int32 ArrivalLoopCount = 1;

	/** 후보가 여럿일 때의 선택 비중입니다. 0 이하이면 뽑히지 않습니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement", meta = (ClampMin = "0.0"))
	float Weight = 1.0f;

	/**
	 * 접근을 끝낼 타깃과의 수평 거리 하한(cm)입니다.
	 * 접근을 시작할 때 Min~Max 중 하나를 뽑아 목표 거리로 사용합니다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement",
	          meta = (EditCondition = "Intent == EEnemyMovementIntent::ApproachTarget", ClampMin = "0.0"))
	float MinTargetDistance = 400.0f;

	/** 접근을 끝낼 타깃과의 수평 거리 상한(cm)입니다. Min보다 작으면 Min을 사용합니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement",
	          meta = (EditCondition = "Intent == EEnemyMovementIntent::ApproachTarget", ClampMin = "0.0"))
	float MaxTargetDistance = 400.0f;

	/*
	 * 타깃 방향에서 틀어질 각도 후보(도).
	 *
	 * 정면으로 곧장 가면 궤적이 지나치게 정직하다. 진입할 때 하나를 뽑고 좌우도 랜덤으로 정한다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement",
	          meta = (EditCondition = "Intent == EEnemyMovementIntent::ApproachTarget"))
	TArray<float> OffsetAngleChoices = {15.0f, 30.0f};

	/*
	 * 이동의 목표 높이. 전투 방 바닥(FloorZ)에서의 거리(cm)다.
	 *
	 * -1이면 이동 프로필 기본값을 쓴다. 공전은 Orbit.CruiseHeightFromFloorMin~Max, 나머지는 Flying.DefaultHeightFromFloor다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement", meta = (ClampMin = "-1.0"))
	float HeightFromFloor = -1.0f;

	/*
	 * 도달하지 못했을 때 포기하고 넘어갈 시간(초). 0이면 무제한.
	 *
	 * 종료 조건이 아니라 끼임 방지용이다. 만료하면 실패가 아니라 성공으로 처리해
	 * 벽에 막힌 채로 패턴이 영영 시작되지 않는 상황을 막는다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement",
	          meta = (EditCondition = "Intent != EEnemyMovementIntent::OrbitAnchor", ClampMin = "0.0"))
	float MaxDuration = 5.0f;
};

/** 일반 Combat 루프에서 선택되는 행동 한 개입니다. */
USTRUCT(BlueprintType)
struct TEAMPROJECT_FIN_API FEnemyPatternDefinition
{
	GENERATED_BODY()


	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Pattern")
	FGameplayTag PatternId;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Pattern")
	FGameplayTag AbilityTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Pattern", meta = (ClampMin = "0.0"))
	float Weight = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Pattern", meta = (ClampMin = "0.0"))
	float Cooldown = 0.0f;

	/** 타깃과의 거리 하한(cm). 수평/3D 기준은 이동 프로필의 PatternDistanceMeasure를 따릅니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Pattern|Range", meta = (ClampMin = "0.0"))
	float MinDistance = 0.0f;

	/** 타깃과의 거리 상한(cm). 0이면 상한 없음. 수평/3D 기준은 이동 프로필을 따릅니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Pattern|Range", meta = (ClampMin = "0.0"))
	float MaxDistance = 0.0f;

	/*
	 * 이 패턴을 쓸 수 있는 바닥 기준 고도 하한(cm). 0이면 하한 없음.
	 *
	 * 고도는 이동 컴포넌트가 잰다. 이동 DA의 HeightFromFloor와 같은 기준이라 숫자를 그대로 맞추면 된다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Pattern|Height", meta = (ClampMin = "0.0"))
	float MinHeightFromFloor = 0.0f;

	/** 이 패턴을 쓸 수 있는 바닥 기준 고도 상한(cm). 0이면 상한 없음. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Pattern|Height", meta = (ClampMin = "0.0"))
	float MaxHeightFromFloor = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Pattern|Condition",
	          meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MinHealthRatio = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Pattern|Condition",
	          meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MaxHealthRatio = 1.0f;

	/*
	 * 이 패턴을 시전하기 전에 수행할 이동 후보들입니다.
	 *
	 * 비어 있으면 이동 없이 바로 시전한다. 하나면 항상 그것을, 여럿이면 Weight로 뽑는다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Pattern|Movement")
	TArray<FEnemyPatternMovementOption> MovementOptions;

	/*
	 * 시전 후 다음 행동까지의 대기 시간 하한(초).
	 *
	 * 기존 값 보존을 위해 이름은 그대로 두고 하한으로 쓴다. 상한이 이보다 작으면 이 값으로 고정된다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Pattern",
	          meta = (ClampMin = "0.0", DisplayName = "Recovery Duration Min"))
	float RecoveryDuration = 0.0f;

	/** 대기 시간 상한(초). 하한~상한에서 매번 뽑는다. 0이면 하한으로 고정. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Pattern", meta = (ClampMin = "0.0"))
	float RecoveryDurationMax = 0.0f;

	/*
	 * 시전 전에 전투 디렉터의 공격 토큰을 받아야 하는지.
	 *
	 * 여러 마리가 한 번에 달려드는 것을 막는다. 토큰을 못 받으면 시전하지 않고 실패로 빠진다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Pattern")
	bool bRequiresAttackToken = false;

	/** 타깃을 향한 행동은 시야와 차폐를 확인한다. 자기 강화 등 비대상 행동만 끈다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Pattern")
	bool bRequiresVisibleTarget = true;

	/*
	 * 시전 중 StateTree가 다른 상태로 넘어갈 때(피격, 페이즈 전환 등)이 GA를 어떻게 할지.
	 *
	 * Cancel Immediately는 GA를 취소하고, 그 외에는 GA가 스스로 끝날 때까지 둔다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Pattern")
	EEnemyActionInterruptPolicy InterruptPolicy = EEnemyActionInterruptPolicy::CancelImmediately;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Pattern", meta = (ClampMin = "0"))
	int32 RepeatLimit = 0;
};

/** 이벤트에 의해 강제로 실행되는 행동 한 개입니다. */
UCLASS(BlueprintType)
class TEAMPROJECT_FIN_API UEnemyAbilitySetDataAsset : public UDataAsset
{
	GENERATED_BODY()


public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Ability")
	TArray<FEnemyAbilityGrantDefinition> Abilities;
};

/** CommonEnemy가 스폰 직후 한 번만 읽는 변형 특성 풀입니다. */
UCLASS(BlueprintType)
class TEAMPROJECT_FIN_API UEnemyTraitPoolDataAsset : public UDataAsset
{
	GENERATED_BODY()


public:
	/** 이 풀을 쓰는 몹이 변형 몹이 될 확률이다. 0이면 사용하지 않는다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Trait",
	          meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MutationChance = 0.30f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Trait")
	TArray<FEnemyTraitEffectDefinition> Effects;
};

/** 일반 Combat에서 선택할 패턴 묶음입니다. */
UCLASS(BlueprintType)
class TEAMPROJECT_FIN_API UEnemyPatternSetDataAsset : public UDataAsset
{
	GENERATED_BODY()


public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Pattern")
	TArray<FEnemyPatternDefinition> Patterns;
};

/**
 * 이동 프로필이 어떤 이동 방식용인지 나타냅니다.
 *
 * 실제 이동 능력(보행/비행)은 캐릭터와 이동 컴포넌트 클래스가 정한다. 이 값은 DA 편집 화면에
 * 해당 방식의 설정만 보이게 하고, 이동 컴포넌트가 맞지 않는 프로필을 받았는지 검사하는 데 쓴다.
 *
 * 새 값은 항상 끝에 추가한다. 중간에 끼우면 이미 저장된 DA의 열거값이 밀린다.
 */
UENUM(BlueprintType)
enum class EEnemyMovementType : uint8
{
	Ground = 0 UMETA(DisplayName = "Ground"),
	Flying = 1 UMETA(DisplayName = "Flying"),
	// 비행 설정 위에 아레나 앵커 공전 설정을 더한다.
	BossOrbit = 2 UMETA(DisplayName = "Boss Orbit")
};

/** 비행 이동 공통 설정입니다. Flying, Boss Orbit에서 사용합니다. */
USTRUCT(BlueprintType)
struct TEAMPROJECT_FIN_API FEnemyFlyingMovementSettings
{
	GENERATED_BODY()


	/*
	 * 이동 옵션에 높이가 없을 때 쓰는 기본 높이(cm). 기준은 전투 방 바닥이다.
	 *
	 * 접근, 지점 이동, 높이 맞춤이 모두 같은 값을 봐야 이동을 오갈 때 목표 Z가 어긋나지 않는다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Flying", meta = (ClampMin = "0.0"))
	float DefaultHeightFromFloor = 300.0f;

	/** 목표 높이에 도착했다고 볼 수직 오차(cm). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Flying", meta = (ClampMin = "0.0"))
	float ArrivalHeightTolerance = 75.0f;
};

/** 아레나 앵커 공전 설정입니다. Boss Orbit에서만 사용합니다. */
USTRUCT(BlueprintType)
struct TEAMPROJECT_FIN_API FEnemyOrbitMovementSettings
{
	GENERATED_BODY()


	/*
	 * 공전 반지름의 하한(cm). 공전을 시작할 때마다 하한~상한에서 하나를 뽑는다.
	 *
	 * 매번 반지름이 달라져 같은 궤도를 반복하지 않는다. 뽑은 값이 0 이하이면 공전을 시작하지 않는다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Orbit", meta = (ClampMin = "0.0"))
	float RadiusMin = 1500.0f;

	/** 공전 반지름의 상한(cm). 하한보다 작으면 하한을 씁니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Orbit", meta = (ClampMin = "0.0"))
	float RadiusMax = 1500.0f;

	/*
	 * 공전 중 유지할 높이의 하한(cm). 기준은 전투 방 바닥이다.
	 *
	 * 공전을 시작할 때마다 하한~상한에서 하나를 뽑아 매번 다른 높이로 돈다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Orbit", meta = (ClampMin = "0.0"))
	float CruiseHeightFromFloorMin = 300.0f;

	/** 공전 중 유지할 높이의 상한(cm). 하한보다 작으면 하한을 씁니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Orbit", meta = (ClampMin = "0.0"))
	float CruiseHeightFromFloorMax = 300.0f;

	/*
	 * 한 번 공전할 시간의 하한(초). 시작할 때 하한~상한에서 하나를 뽑는다.
	 *
	 * 공전은 도착 개념이 없어 시간으로만 끝난다. 0이면 곧바로 끝나므로 멈춰 서는 일은 없다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Orbit", meta = (ClampMin = "0.0"))
	float CruiseDurationMin = 2.0f;

	/** 한 번 공전할 시간의 상한(초). 하한보다 작으면 하한을 씁니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Orbit", meta = (ClampMin = "0.0"))
	float CruiseDurationMax = 3.0f;

	/*
	 * 공전을 새로 시작할 때 직전 방향의 반대로 돌 확률(0~1).
	 *
	 * 직전 방향을 기억해 두고 이 확률로만 뒤집는다. 0.5면 이전 방향:반대 방향이 5:5이고,
	 * 낮출수록 같은 방향으로 계속 돈다. 처음 공전할 때는 직전 방향이 없어 반반으로 정한다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Orbit", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ReverseDirectionChance = 0.5f;

	/** 목표 반지름이 흔들리는 폭(cm). S자 궤적을 만듭니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Orbit|Sway")
	float SwayAmplitude = 250.0f;

	/** 반지름 흔들림의 초당 반복 횟수입니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Orbit|Sway")
	float SwayFrequency = 0.2f;

	/*
	 * 반지름을 맞출 때 접선에서 틀어질 수 있는 최대 각도(도).
	 *
	 * 작을수록 궤도로 곧장 뛰어들지 않고 완만한 나선으로 붙는다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Orbit|Sway",
	          meta = (ClampMin = "1.0", ClampMax = "80.0"))
	float MaxApproachAngle = 20.0f;

	/** 반지름 오차를 방향 보정으로 바꾸는 강도입니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Orbit|Sway")
	float DistanceCorrectionSpeed = 5.0f;

	/** 목표 높이를 기준으로 위아래로 흔들리는 폭(cm)입니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Orbit|Hover")
	float HoverAmplitude = 50.0f;

	/** 높이 흔들림의 초당 반복 횟수입니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Orbit|Hover")
	float HoverFrequency = 0.25f;
};

/**
 * 몬스터 한 종류의 이동 설정입니다.
 *
 * Movement Type을 고르면 그 방식에 필요한 설정만 편집 화면에 보인다.
 * 속도처럼 모든 방식이 쓰는 값은 항상 보인다.
 */
UCLASS(BlueprintType)
class TEAMPROJECT_FIN_API UEnemyMovementProfileDataAsset : public UDataAsset
{
	GENERATED_BODY()


public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Movement")
	EEnemyMovementType MovementType = EEnemyMovementType::Ground;

	/*
	 * 비행 최대 속도(cm/s). 0이면 이동 컴포넌트의 기본값을 씁니다.
	 *
	 * 지상 이동 속도는 MovementSpeed 어트리뷰트(스탯 DT, 둔화 디버프)가 정하므로 여기서 바꾸지 않는다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Movement|Common", meta = (ClampMin = "0.0"))
	float MoveSpeed = 0.0f;

	/** 이동 가속도(cm/s²). 0이면 이동 컴포넌트의 기본값을 씁니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Movement|Common", meta = (ClampMin = "0.0"))
	float Acceleration = 0.0f;

	/** 지점 이동에서 목적지에 도착했다고 볼 수평 거리(cm)입니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Movement|Common", meta = (ClampMin = "0.0"))
	float ArrivalRadius = 100.0f;

	/** 이동 중 바라볼 대상 쪽으로 몸을 돌리는 보간 속도입니다. 클수록 빨리 돌아봅니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Movement|Common", meta = (ClampMin = "0.0"))
	float FacingTurnSpeed = 3.0f;

	/*
	 * 패턴 거리 조건(MinDistance, MaxDistance)을 잴 기준입니다.
	 *
	 * 고도를 크게 쓰는 보스는 수평으로 재고 고도는 패턴의 높이 조건으로 따로 본다.
	 * 기존 일반 몬스터의 사거리는 3D 기준으로 맞춰져 있다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Movement|Common")
	EEnemyDistanceMeasure PatternDistanceMeasure = EEnemyDistanceMeasure::Horizontal;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Movement",
	          meta = (EditCondition =
	                      "MovementType == EEnemyMovementType::Flying || MovementType == EEnemyMovementType::BossOrbit",
	                  EditConditionHides))
	FEnemyFlyingMovementSettings Flying;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Movement",
	          meta = (EditCondition = "MovementType == EEnemyMovementType::BossOrbit", EditConditionHides))
	FEnemyOrbitMovementSettings Orbit;
};

/*
 * 몬스터 종류별 기본 등장 연출이다.
 * SpawnPoint는 Floor/Wall/Ceiling 같은 배치 방식을 정하고, 랜덤 스폰 시에는
 * 실제로 선택된 몬스터 Definition의 이 값을 사용한다.
 */
USTRUCT(BlueprintType)
struct FEnemyEmergeDefinition
{
	GENERATED_BODY()


	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Emerge")
	TObjectPtr<UAnimMontage> FloorMontage = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Emerge")
	TObjectPtr<UAnimMontage> WallMontage = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Emerge")
	TObjectPtr<UAnimMontage> CeilingMontage = nullptr;
};

/**
 * 몬스터 한 종류의 전투 구성을 조립하는 최상위 Data Asset입니다.
 *
 * 여기에는 "이 몬스터가 무엇을 할 수 있는가"만 둔다(스탯, 능력 구성, 패턴, 이동 프로필).
 * "어떻게 배치되고 행동하는가"(시작 방식, 순찰 경로, 타깃 상실 대응, 시체 유지 시간)는
 * 배치마다 달라서 캐릭터 프로퍼티가 들고 있고, 피격·회피 반응과 사망 흐름은 StateTree와 GA가 맡는다.
 */
UCLASS(BlueprintType)
class TEAMPROJECT_FIN_API UEnemyDefinitionDataAsset : public UDataAsset
{
	GENERATED_BODY()


public:
	/** 일반 몬스터와 보스 HUD에서 공통으로 표시할 이름입니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|UI")
	FText DisplayName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy")
	FGameplayTag EnemyId;

	/** 기존 몬스터 Stat Table 행을 그대로 참조합니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Stats")
	FDataTableRowHandle StatTableRowHandle;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Ability")
	TObjectPtr<UEnemyAbilitySetDataAsset> AbilitySet;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Combat")
	TObjectPtr<UEnemyPatternSetDataAsset> PatternSet;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Movement")
	TObjectPtr<UEnemyMovementProfileDataAsset> MovementProfile;

	// SpawnPoint가 개별 몽타주를 강제하지 않을 때 사용할 몬스터 종류별 기본 등장 연출.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Emerge")
	FEnemyEmergeDefinition Emerge;
};

/** 보스 패턴 이동 중 바라볼 대상입니다. */
UENUM(BlueprintType)
enum class EEnemyBossFacingMode : uint8
{
	CombatTarget UMETA(DisplayName = "Combat Target"),
	ArenaCenter UMETA(DisplayName = "Arena Center")
};

/** 보스 페이즈 한 개의 전환·오버라이드 정의입니다. */
USTRUCT(BlueprintType)
struct TEAMPROJECT_FIN_API FEnemyBossPhaseDefinition
{
	GENERATED_BODY()


	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Boss Phase")
	FGameplayTag PhaseId;

	/** 이 체력 비율 이하에서 이 페이즈로 진입합니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Boss Phase",
	          meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float HealthThreshold = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Boss Phase")
	FGameplayTag TransitionAbilityTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Boss Phase")
	TObjectPtr<UEnemyPatternSetDataAsset> PatternSet;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Boss Phase")
	TObjectPtr<UEnemyMovementProfileDataAsset> MovementProfileOverride;

	/*
	 * 켜면 이 페이즈의 보스는 제자리에서만 싸운다.
	 *
	 * 패턴 사이의 기본 움직임(공전)을 하지 않고, 패턴의 이동 후보도 제자리 시전으로 바꾼다.
	 * 페이즈 전환 연출의 순간이동·상승 이동은 별도 경로라 영향을 받지 않는다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Boss Phase|Movement")
	bool bStationary = false;

	/** 패턴 이동 중 바라볼 대상입니다. Arena Center면 플레이어 대신 아레나 중심을 향합니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Boss Phase|Rotation")
	EEnemyBossFacingMode FacingMode = EEnemyBossFacingMode::CombatTarget;

	/** 이 페이즈에서 유지할 보스 메시의 기본 스케일 배율입니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Boss Phase|Form", meta = (ClampMin = "0.01"))
	FVector MeshScaleMultiplier = FVector::OneVector;

	/** 이 페이즈에서 유지할 몸 피격 Capsule 반경 배율입니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Boss Phase|Form", meta = (ClampMin = "0.01"))
	float CapsuleRadiusMultiplier = 1.0f;

	/** 이 페이즈에서 유지할 몸 피격 Capsule 반높이 배율입니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Boss Phase|Form", meta = (ClampMin = "0.01"))
	float CapsuleHalfHeightMultiplier = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Boss Phase")
	bool bLockCombatDuringTransition = true;
};

/** 보스가 순서대로 진입하는 모든 페이즈입니다. */
UCLASS(BlueprintType)
class TEAMPROJECT_FIN_API UBossPhaseSetDataAsset : public UDataAsset
{
	GENERATED_BODY()


public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Boss Phase")
	TArray<FEnemyBossPhaseDefinition> Phases;
};

/** 공용 Enemy Definition에 보스 페이즈 데이터만 추가합니다. */
UCLASS(BlueprintType)
class TEAMPROJECT_FIN_API UBossEnemyDefinitionDataAsset : public UEnemyDefinitionDataAsset
{
	GENERATED_BODY()


public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Boss")
	TObjectPtr<UBossPhaseSetDataAsset> BossPhaseSet;
};
