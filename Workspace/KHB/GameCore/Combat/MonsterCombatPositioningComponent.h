#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayAbilitySpecHandle.h"
#include "GameplayTagContainer.h"
#include "MonsterCombatPositioningComponent.generated.h"

class AActor;
class UAbilitySystemComponent;
enum class EEnemyPatternPreparationStatus : uint8;

UENUM(BlueprintType)
enum class EMonsterPositioningDirectionPreference : uint8
{
	/** 방향을 가리지 않는다. */
	Any UMETA(DisplayName = "Any"),
	/** 타깃이 바라보는 정면에 나타나는 것을 선호한다. */
	Front UMETA(DisplayName = "Front"),
	/** 타깃의 좌우 측면을 선호한다. 은신 순간이동에 가장 잘 맞는다. */
	Side UMETA(DisplayName = "Side"),
	/** 타깃의 등 뒤를 선호한다. */
	Rear UMETA(DisplayName = "Rear"),
};

UENUM(BlueprintType)
enum class EMonsterRepositionPolicy : uint8
{
	/** 재배치하지 않는다. 요구 거리를 벗어나 있으면 준비 자체가 실패한다. */
	Never UMETA(DisplayName = "Never"),
	/** 요구 거리나 시야를 벗어났을 때만 재배치한다. 일반 몬스터용. */
	WhenOutOfRange UMETA(DisplayName = "When Out Of Range"),
	/** 거리가 이미 맞아도 패턴마다 재배치한다. 엘리트의 은신 순간이동이 여기에 해당한다. */
	Always UMETA(DisplayName = "Always"),
};

/**
 * 공격 패턴 하나가 요구하는 위치 조건.
 *
 * 후보 지점은 몬스터가 아니라 타깃을 중심으로 뿌린다. 몬스터의 현재 위치는
 * 후보 생성에 관여하지 않고, 마지막 점수 단계에서 이동 거리로만 반영된다.
 */
USTRUCT(BlueprintType)
struct FMonsterActionPositionProfile
{
	GENERATED_BODY()

	/** 이 프로필이 적용될 공격 태그. Choose Weighted Action이 고른 SelectedAction과 비교한다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Position")
	FGameplayTag ActionTag;

	/** 타깃 기준 후보 링의 안쪽 반지름. 이보다 가까운 곳에는 후보를 만들지 않는다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Position", meta = (ClampMin = "0.0"))
	float DesiredMinDistance = 0.0f;

	/** 타깃 기준 후보 링의 바깥쪽 반지름. 이 패턴의 공격이 실제로 닿는 거리여야 한다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Position", meta = (ClampMin = "0.0"))
	float DesiredMaxDistance = 0.0f;

	/** 켜면 타깃이 보이지 않는 후보를 탈락시킨다. 투사체 패턴에 사용한다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Position")
	bool bRequireLineOfSight = false;

	/**
	 * 재배치를 언제 할지 단독으로 결정한다.
	 * 거리 값은 "재배치할지"가 아니라 "어디로 재배치할지"만 정한다는 점에 주의한다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reposition")
	EMonsterRepositionPolicy RepositionPolicy = EMonsterRepositionPolicy::WhenOutOfRange;

	/** 재배치에 사용할 GA의 태그. SelectedAction은 바꾸지 않고 이 어빌리티만 따로 실행한다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reposition",
		meta = (EditCondition = "RepositionPolicy != EMonsterRepositionPolicy::Never"))
	FGameplayTag RepositionAbilityTag;

	/**
	 * 이 정도는 움직였으면 하는 목표 이동 거리. 탈락 조건이 아니라 감점 기준이다.
	 * 이 거리를 넘긴 후보는 모두 동점이 되므로, DesiredMinDistance 이하로 잡아야
	 * 링의 상당 부분이 후보로 살아남는다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reposition", meta = (ClampMin = "0.0"))
	float MinimumRepositionDistance = 250.0f;

	/** 타깃 기준 어느 방향에 나타나는 것을 선호할지. 점수에만 영향을 준다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Candidates")
	EMonsterPositioningDirectionPreference DirectionPreference = EMonsterPositioningDirectionPreference::Any;

	/** 후보를 뿌릴 방향의 개수. 8이면 45도 간격이다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Candidates", meta = (ClampMin = "4", ClampMax = "32"))
	int32 DirectionSampleCount = 8;

	/** 각 방향마다 Min~Max 사이를 몇 단계로 나눌지. 총 후보 수는 방향 수 × 이 값이다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Candidates", meta = (ClampMin = "1", ClampMax = "8"))
	int32 DistanceSampleCount = 3;

	/** 바닥 경사가 이보다 급하면 후보를 탈락시킨다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Validation", meta = (ClampMin = "0.0"))
	float MaximumFloorSlopeDegrees = 45.0f;

	/** 주변이 이보다 좁으면 후보를 탈락시킨다. 벽이나 기둥에 끼는 것을 막는다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Validation", meta = (ClampMin = "0.0"))
	float MinimumClearance = 100.0f;

	/** 주변 여유 공간 점수가 만점이 되는 기준치. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Validation", meta = (ClampMin = "0.0"))
	float IdealClearance = 350.0f;

	/** 최근 도착지에서 이 반경 안에 있으면 감점한다. 탈락 조건은 아니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Validation", meta = (ClampMin = "0.0"))
	float RecentPositionRadius = 300.0f;

	/** 링 한가운데에 가까울수록 받는 가산점. 바깥쪽 끝이 아니라 중앙이 만점이다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Score", meta = (ClampMin = "0.0"))
	float DistanceWeight = 40.0f;

	/** DirectionPreference에 맞을수록 받는 가산점. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Score", meta = (ClampMin = "0.0"))
	float DirectionWeight = 25.0f;

	/** 타깃이 보이는 후보에 주는 가산점. 보임/안 보임 두 값만 있다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Score", meta = (ClampMin = "0.0"))
	float LineOfSightWeight = 20.0f;

	/** 주변이 트인 후보일수록 받는 가산점. IdealClearance에서 만점이다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Score", meta = (ClampMin = "0.0"))
	float ClearanceWeight = 15.0f;

	/** 최근에 갔던 자리에 가까울수록 받는 감점. 같은 곳을 반복하는 것을 줄인다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Score", meta = (ClampMin = "0.0"))
	float RecentPositionPenalty = 10.0f;

	/**
	 * 이동 거리가 MinimumRepositionDistance에 못 미칠수록 받는 감점.
	 * 다른 가중치보다 크게 잡아야 짧은 순간이동이 최후의 수단이 된다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Score", meta = (ClampMin = "0.0"))
	float ShortRepositionPenalty = 60.0f;
};

UENUM(BlueprintType)
enum class EMonsterActionPositionPreparationResult : uint8
{
	/** 재배치 없이 현재 위치에서 바로 시전해도 된다. */
	Ready UMETA(DisplayName = "Ready"),
	/** 도착지를 찾았다. RepositionAbilityTag의 GA를 실행해야 한다. */
	NeedsReposition UMETA(DisplayName = "Needs Reposition"),
	/** 갈 곳도 없고 현재 위치도 쓸 수 없다. 패턴을 포기한다. */
	Failed UMETA(DisplayName = "Failed"),
};

/**
 * 전투 행동에 필요한 위치를 찾는 공용 컴포넌트.
 *
 * 판정은 두 단계다. 먼저 그 자리에서 패턴이 성립하지 않는 조건(요구 거리, 충돌,
 * 경사, 시야, 높이차)으로 후보를 탈락시키고, 살아남은 후보 중에서 점수가 가장
 * 높은 하나를 고른다. 연출상의 취향(짧은 이동, 같은 자리 반복)은 탈락이 아니라
 * 점수로만 다뤄서, 데이터 값 조합 때문에 후보가 전멸하는 일이 없게 한다.
 *
 * CombatArea, Kill/NoTeleport Volume, 플레이어 도달 가능성은 레벨 규칙이
 * 확정된 뒤 추가할 TODO다.
 *
 * 패턴 준비 단계의 재배치(Reposition For Action)도 이 컴포넌트가 끝까지 맡는다.
 * 도착지 탐색, 재배치 GA 발동, GA 종료 대기, 결과 판정이 모두 순간이동 절차라
 * 걷거나 나는 방식과 무관하다. 이동 컴포넌트는 실제 이동만 담당한다.
 */
UCLASS(ClassGroup = Combat, meta = (BlueprintSpawnableComponent))
class TEAMPROJECT_FIN_API UMonsterCombatPositioningComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UMonsterCombatPositioningComponent();

	/*
	 * 패턴 준비 단계의 재배치를 시작한다. Prepare Enemy Pattern이 Reposition For Action 일 때 부른다.
	 *
	 * Teleport 특성 확인 → 패턴 AbilityTag로 프로필을 찾아 도착지 예약 → 프로필의 재배치 GA 발동.
	 * 프로필이 없거나 현재 위치로 충분하면 Ready를 돌려 제자리에서 시전하게 한다.
	 */
	EEnemyPatternPreparationStatus BeginReposition(FGameplayTag PatternAbilityTag, AActor* TargetActor);

	/** 재배치 GA가 도는 동안 Preparing, 끝나면 순간이동 성공 여부로 Ready 또는 Failed를 돌려준다. */
	EEnemyPatternPreparationStatus UpdateReposition();

	/** 준비 상태가 끊겼을 때 예약만 취소한다. 이미 시작된 순간이동은 GA가 스스로 마무리한다. */
	void CancelReposition();

	EMonsterActionPositionPreparationResult PrepareSelectedAction(
		AActor* TargetActor,
		FGameplayTag SelectedAction,
		FGameplayTag& OutRepositionAbilityTag);

	bool CommitPendingReposition();
	void CancelPendingReposition();
	bool WasLastRepositionSuccessful() const { return bLastRepositionSuccessful; }
	bool HasPendingReposition() const { return bHasPendingReposition; }

protected:
	/** 패턴별 위치 조건 목록. ActionTag로 SelectedAction과 짝지어진다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Position")
	TArray<FMonsterActionPositionProfile> ActionProfiles;

	/** 경사를 재기 위해 후보 위치에서 아래로 쏘는 트레이스의 길이. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Position", meta = (ClampMin = "0.0"))
	float GroundTraceDistance = 3000.0f;

	/** 바닥 위에 캡슐을 세울 때 캡슐 반높이에 더하는 여유. 크면 착지가 낙하로 처리된다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Position", meta = (ClampMin = "0.0"))
	float GroundPlacementPadding = 4.0f;

	/** 현재 높이와 이만큼 넘게 차이나는 후보는 탈락시킨다. 다른 층으로 순간이동하는 것을 막는다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Validation", meta = (ClampMin = "0.0"))
	float MaximumHeightDifference = 250.0f;

	/** 후보 지점을 화면에 그린다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Debug")
	bool bDebugDrawCandidates = false;

private:
	const FMonsterActionPositionProfile* FindProfile(FGameplayTag ActionTag) const;
	bool IsCurrentPositionValid(const FMonsterActionPositionProfile& Profile, const AActor* TargetActor) const;
	bool FindBestCandidate(const FMonsterActionPositionProfile& Profile, AActor* TargetActor, FVector& OutLocation);
	bool ResolveCandidateLocation(const FVector& RawLocation, FVector& OutLocation) const;
	bool ValidateCandidate(const FMonsterActionPositionProfile& Profile, const FVector& Location, const AActor* TargetActor, bool& bOutHasLineOfSight) const;
	float CalculateCandidateScore(const FMonsterActionPositionProfile& Profile, const FVector& Location, const AActor* TargetActor, bool bHasLineOfSight) const;
	float CalculateClearance(const FVector& Location, const AActor* TargetActor) const;
	float CalculateDirectionUtility(EMonsterPositioningDirectionPreference Preference, const FVector& Location, const AActor* TargetActor) const;
	float GetOwnerCapsuleHalfHeight() const;
	/** 바닥에서 띄워 둘 높이. 비행형은 비행 높이, 지상형은 0이다. */
	float GetOwnerHoverHeight() const;
	UAbilitySystemComponent* GetOwnerAbilitySystemComponent() const;
	/** 재배치 GA가 끝났을 때 기록된 순간이동 결과를 준비 결과로 바꾼다. */
	EEnemyPatternPreparationStatus ResolveRepositionResult() const;
	void FinalizeTeleport();
	void RememberTeleportLocation(const FVector& Location);

	/** 실행 중인 재배치 GA. 무효면 기다리는 재배치가 없다. */
	FGameplayAbilitySpecHandle RepositionSpecHandle;

	FVector PendingLocation = FVector::ZeroVector;
	FGameplayTag PendingAction;
	TWeakObjectPtr<AActor> PendingTarget;
	bool bHasPendingReposition = false;
	bool bLastRepositionSuccessful = false;
	TArray<FVector> RecentTeleportLocations;
};
