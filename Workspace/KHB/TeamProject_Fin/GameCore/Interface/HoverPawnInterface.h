// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "HoverPawnInterface.generated.h"

UINTERFACE(MinimalAPI)
class UHoverPawnInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * 공중에 떠서 이동하는 폰이 구현한다.
 *
 * 부유 높이는 폰 자신의 속성이다.
 * 스폰, 궤도 비행, 복귀가 모두 같은 값을 봐야 하고
 * 페이즈에 따라 바뀌기도 하므로 폰이 단일 출처를 들고 있는다.
 *
 * AI 태스크가 특정 보스 클래스를 알지 않아도 되도록 인터페이스로 분리한다.
 */
class IHoverPawnInterface
{
	GENERATED_BODY()

public:
	// 기준면으로부터 유지할 높이(cm)
	virtual float GetHoverHeight() const = 0;

	/*
	 * 이동의 기준이 되는 아레나 중심 액터.
	 *
	 * StateTree는 콘텐츠 에셋이라 레벨 액터를 참조할 수 없다.
	 * 레벨에 존재하는 폰이 대신 참조를 들고 있다가 넘겨준다.
	 */
	virtual AActor* GetArenaCenter() const = 0;

	/*
	 * 아레나 중심으로부터 유지할 공전 반지름(cm).
	 *
	 * 공전 태스크와 접근 조건이 같은 값을 봐야 한다.
	 * 태스크마다 들고 있으면 상태 수만큼 중복되어 한쪽만 바뀌기 쉽다.
	 */
	virtual float GetOrbitRadius() const = 0;
};
