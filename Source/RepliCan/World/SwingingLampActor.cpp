#include "World/SwingingLampActor.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "UObject/ConstructorHelpers.h"

ASwingingLampActor::ASwingingLampActor()
{
	PrimaryActorTick.bCanEverTick = true;
	Hang = CreateDefaultSubobject<USceneComponent>(TEXT("Hang"));
	SetRootComponent(Hang);
	Pivot = CreateDefaultSubobject<USceneComponent>(TEXT("Pivot"));
	Pivot->SetupAttachment(Hang);
	Cable = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Cable"));
	Cable->SetupAttachment(Pivot);
	Cable->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Cable->SetCastShadow(false);
	Lamp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Lamp"));
	Lamp->SetupAttachment(Pivot);
	Lamp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Light = CreateDefaultSubobject<UPointLightComponent>(TEXT("Light"));
	Light->SetupAttachment(Pivot);
	Light->SetMobility(EComponentMobility::Movable);
	Light->SetIntensityUnits(ELightUnits::Candelas);
	{
		static ConstructorHelpers::FObjectFinder<UStaticMesh> Cyl(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
		if (Cyl.Succeeded()) { Cable->SetStaticMesh(Cyl.Object); }
	}
	Hang->SetMobility(EComponentMobility::Movable);
	Pivot->SetMobility(EComponentMobility::Movable);
	Cable->SetMobility(EComponentMobility::Movable);
	Lamp->SetMobility(EComponentMobility::Movable);
}

void ASwingingLampActor::Arrange()
{
	// The cable is the engine's 100-tall cylinder, centred: scaled to the length and hung so its top is
	// at the pivot. The lamp sits at the cable's foot, the light just under the fitting.
	if (Cable) { Cable->SetRelativeLocation(FVector(0.0f, 0.0f, -CableLength * 0.5f)); Cable->SetRelativeScale3D(FVector(0.025f, 0.025f, CableLength / 100.0f)); }
	if (Lamp) { Lamp->SetStaticMesh(LampMesh); Lamp->SetRelativeLocation(FVector(0.0f, 0.0f, -CableLength)); Lamp->SetRelativeRotation(FRotator(0.0f, LampYaw, 0.0f)); }
	if (Light)
	{
		Light->SetRelativeLocation(FVector(0.0f, 0.0f, -CableLength - 14.0f));
		Light->SetIntensity(Intensity); Light->SetAttenuationRadius(Radius); Light->SetLightColor(Colour); Light->SetCastShadows(bShadows);
	}
	Amplitude = AmplitudeDegrees;
}

void ASwingingLampActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	Arrange();
}

void ASwingingLampActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!Pivot) { return; }
	Clock += DeltaSeconds;
	// The swing: a pendulum in a plane that drifts, its amplitude easing back toward the resting value
	// after each gust. A gust is a kick of amplitude and a shove to the plane, at random intervals.
	NextGust -= DeltaSeconds;
	if (NextGust <= 0.0f)
	{
		Amplitude += GustDegrees * FMath::FRandRange(0.5f, 1.0f);
		PlaneYaw += FMath::FRandRange(-50.0f, 50.0f);
		NextGust = GustEverySeconds * FMath::FRandRange(0.5f, 1.5f);
	}
	Amplitude = FMath::FInterpTo(Amplitude, AmplitudeDegrees, DeltaSeconds, 0.12f);
	PlaneYaw += DeltaSeconds * 4.0f;   // and a slow turn of the plane on its own
	const float Angle = Amplitude * FMath::Sin(2.0f * PI * Clock / FMath::Max(0.3f, PeriodSeconds) + Phase);
	const float Rad = FMath::DegreesToRadians(PlaneYaw);
	// Tilt about the horizontal axis perpendicular to the swing plane: roll and pitch share the angle.
	Pivot->SetRelativeRotation(FRotator(Angle * FMath::Sin(Rad), 0.0f, Angle * FMath::Cos(Rad)));
}
