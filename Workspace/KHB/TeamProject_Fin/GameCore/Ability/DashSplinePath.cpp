#include "DashSplinePath.h"

#include "Components/SplineComponent.h"

ADashSplinePath::ADashSplinePath()
{
	Spline = CreateDefaultSubobject<USplineComponent>(TEXT("Spline"));
	SetRootComponent(Spline);
	SetActorEnableCollision(false);
}
