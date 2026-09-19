#include "FlickerLightActor.h"
#include "Components/PointLightComponent.h"

AFlickerLightActor::AFlickerLightActor()
{
	PrimaryActorTick.bCanEverTick = true;
	Light = CreateDefaultSubobject<UPointLightComponent>(TEXT("Light"));
	RootComponent = Light;
	Light->SetMobility(EComponentMobility::Movable);
	Light->SetCastShadows(false);
	Light->SetIntensityUnits(ELightUnits::Candelas);
}

void AFlickerLightActor::ApplyStatic()
{
	if (!Light) { return; }
	Light->SetLightColor(Color);
	Light->SetAttenuationRadius(AttenuationRadius);
	Light->SetIntensity(BaseIntensity);
}

void AFlickerLightActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	ApplyStatic();
}

void AFlickerLightActor::BeginPlay()
{
	Super::BeginPlay();
	ApplyStatic();
	Clock = Seed * 7.31f;
}

void AFlickerLightActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!Light) { return; }
	Clock += DeltaSeconds * FMath::Max(0.05f, Speed);
	// Random flares: arrive at FlaresPerSecond on average, decay fast.
	if (FMath::FRand() < DeltaSeconds * FlaresPerSecond) { Flare = FMath::Max(Flare, FMath::FRandRange(0.35f, 0.9f)); }
	Flare = FMath::Max(0.0f, Flare - DeltaSeconds * 5.0f * Flare - DeltaSeconds * 0.4f);
	const float S = Seed;
	const float Irregular = 0.42f
		+ 0.28f * FMath::Sin(Clock * 1.7f + S)
		+ 0.18f * FMath::Sin(Clock * 4.3f + 2.1f * S)
		+ 0.12f * FMath::Sin(Clock * 9.7f + 3.7f * S)
		+ Flare;
	// The regular pulse: a smooth beat with a slightly sharpened peak.
	const float Phase = FMath::Fmod(Clock / FMath::Max(0.2f, Period) + S, 1.0f);
	const float Beat = FMath::Pow(0.5f + 0.5f * FMath::Sin(Phase * 2.0f * PI), 1.6f);
	const float Regular = 0.25f + 0.85f * Beat;
	const float Mix = FMath::Lerp(Irregular, Regular, FMath::Clamp(Regularity, 0.0f, 1.0f));
	Light->SetIntensity(BaseIntensity * FMath::Clamp(Mix, 0.06f, 1.3f));
}
