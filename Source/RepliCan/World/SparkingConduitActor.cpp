#include "World/SparkingConduitActor.h"
#include "Weapons/SparkFx.h"
#include "Narrative/VoiceLines.h"
#include "World/AmbientPlayer.h"
#include "Weapons/TimedFx.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "TimerManager.h"
#include "Misc/Paths.h"

ASparkingConduitActor::ASparkingConduitActor()
{
	PrimaryActorTick.bCanEverTick = false;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
}

void ASparkingConduitActor::BeginPlay()
{
	Super::BeginPlay();
	Schedule();
}

void ASparkingConduitActor::Schedule()
{
	if (!GetWorld()) { return; }
	GetWorld()->GetTimerManager().SetTimer(Timer, this, &ASparkingConduitActor::Fire, FMath::FRandRange(FMath::Max(0.5f, IntervalMin), FMath::Max(IntervalMin + 0.5f, IntervalMax)), false);
}

void ASparkingConduitActor::Fire()
{
	UWorld* World = GetWorld();
	if (!World) { return; }
	const FVector At = GetActorLocation();
	const FVector Dir = Normal.GetSafeNormal().IsNearlyZero() ? FVector(0.0f, 0.0f, -1.0f) : Normal.GetSafeNormal();
	const bool bSurge = FMath::FRand() < SurgeChance;
	SparkFx::Burst(World, At, Dir, bSurge ? Count * 2 : Count, Scale, FLinearColor(0.55f, 0.75f, 1.0f, 1.0f), FLinearColor(1.0f, 0.85f, 0.3f, 1.0f));
	if (bSurge)
	{
		static UNiagaraSystem* Surge = LoadObject<UNiagaraSystem>(nullptr, TEXT("/Game/Synty/PolygonSciFiHorror/FX/NS_Electricity_Surge_01.NS_Electricity_Surge_01"), nullptr, LOAD_NoWarn | LOAD_Quiet);
		if (Surge) { TimedFx::Spawn(World, Surge, At, Dir.Rotation(), FVector(0.6f), SurgeFadeAfterSeconds); }
	}
	UAmbientPlayer::PlayFileAt(this, World, FString::Printf(TEXT("spark_crackle_%02d.wav"), FMath::RandRange(1, 3)),
		At, Volume * (bSurge ? 1.4f : 1.0f), FMath::FRandRange(0.9f, 1.1f));
	Schedule();
}
