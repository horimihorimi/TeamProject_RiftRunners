#pragma once

#include "CoreMinimal.h"
#include "GameCore/Character/BaseCharacter.h"
#include "GameCore/Interface/HoverPawnInterface.h"
#include "BossCharacter.generated.h"

/**
 * GAS 캐릭터 기반의 공중 보스 공용 클래스.
 */
UCLASS()
class TEAMPROJECT_FIN_API ABossCharacter
	: public ABaseCharacter
	, public IHoverPawnInterface
{
	GENERATED_BODY()

public:
	ABossCharacter();

	virtual float GetHoverHeight() const override { return HoverHeight; }
	virtual AActor* GetArenaCenter() const override;
	virtual float GetOrbitRadius() const override { return OrbitRadius; }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Hover")
	float OrbitRadius = 1500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Hover")
	TObjectPtr<AActor> ArenaCenter = nullptr;

	UPROPERTY(EditAnywhere, Category = "Boss|Hover")
	FName ArenaCenterTag = TEXT("BossArenaCenter");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Hover")
	float HoverHeight = 300.0f;

protected:
	virtual void BeginPlay() override;

private:
	void ResolveArenaCenter();
};
