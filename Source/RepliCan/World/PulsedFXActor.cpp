#include "PulsedFXActor.h"
#include "AmbientPlayer.h"
#include "VoiceLines.h"
#include "NiagaraComponent.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundWave.h"
#include "Misc/Paths.h"

APulsedFXActor::APulsedFXActor()
{
	PrimaryActorTick.bCanEverTick = true;

	FX = CreateDefaultSubobject<UNiagaraComponent>(TEXT("FX"));
	SetRootComponent(FX);
	// The whole point is that it is off most of the time, so it must not start itself.
	FX->bAutoActivate = false;
	// Not clickable in the editor, for the same reason as AAtmosphereFXActor: a steam plume in
	// front of a crate should not be what a click on the crate selects.
	FX->bSelectable = false;
	FX->SetAutoDestroy(false);
}

void APulsedFXActor::BeginPlay()
{
	Super::BeginPlay();
	if (FX) { FX->Deactivate(); }

	// Offset by a hash of the actor's own name. Two vents in one room placed with the same
	// timings would otherwise fire in lockstep from the first frame and read as one effect.
	const uint32 Seed = GetTypeHash(GetName());
	Left = QuietSeconds * ((Seed % 1000) / 1000.0f);
	bBursting = false;
}

float APulsedFXActor::Roll(float Base) const
{
	const float J = FMath::Clamp(Jitter, 0.0f, 0.9f);
	return FMath::Max(0.1f, Base * FMath::FRandRange(1.0f - J, 1.0f + J));
}

void APulsedFXActor::Fire()
{
	if (FX) { FX->Activate(true); }
	if (BurstSound.IsEmpty() || BurstVolume <= 0.0f || !GetWorld()) { return; }

	float Seconds = 0.0f;
	USoundWave* Wave = VoiceLines::LoadWav(this, FPaths::Combine(UAmbientPlayer::RawAudioDir(), BurstSound), Seconds);
	if (!Wave) { return; }

	USoundAttenuation* Att = NewObject<USoundAttenuation>(this);
	Att->Attenuation.bAttenuate = true;
	Att->Attenuation.AttenuationShapeExtents = FVector(FMath::Max(1.0f, SoundInnerCm), 0.0f, 0.0f);
	Att->Attenuation.FalloffDistance = FMath::Max(1.0f, SoundFalloffCm);
	// Slight pitch scatter so repeated bursts from one vent are not the same sound file twice.
	UGameplayStatics::SpawnSoundAtLocation(GetWorld(), Wave, GetActorLocation(), FRotator::ZeroRotator,
	                                       BurstVolume, FMath::FRandRange(0.92f, 1.08f), 0.0f, Att);
}

void APulsedFXActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Left -= DeltaSeconds;
	if (Left > 0.0f) { return; }

	if (bBursting)
	{
		// Deactivate, not Stop: the plume already in the air is allowed to thin out and die on
		// its own, which is what a real valve closing looks like.
		if (FX) { FX->Deactivate(); }
		bBursting = false;
		Left = Roll(QuietSeconds);
	}
	else
	{
		Fire();
		bBursting = true;
		Left = Roll(BurstSeconds);
	}
}
