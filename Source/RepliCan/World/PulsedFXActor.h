// A Niagara effect that runs in bursts instead of running forever.
//
// A steam vent that hisses continuously is scenery you stop seeing after ten seconds. One that
// lets go every twenty or so is a thing that HAPPENS, and the room feels pressurised rather than
// decorated. So the system is not left running: it is activated for a short burst, deactivated
// (not stopped -- deactivate lets the particles already in the air finish their lives, where
// stop would blink them out of existence), and left quiet until the next one.
//
// Every vent is offset by a hash of its own name, so two jets placed in the same room never
// breathe together, and the gap is jittered so the player cannot learn the rhythm.
//
// The hiss travels with the burst: a one-shot played at the actor, attenuated, rather than the
// positional LOOP that Tools/facility_layout.py hangs on a continuous vent through
// UI/SteamVents.json. A periodic jet with a constant hiss is worse than no sound at all.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PulsedFXActor.generated.h"

class UNiagaraComponent;
class UNiagaraSystem;

UCLASS()
class REPLICAN_API APulsedFXActor : public AActor
{
	GENERATED_BODY()

public:
	APulsedFXActor();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(VisibleAnywhere, Category = "Pulsed FX") TObjectPtr<UNiagaraComponent> FX;

	// How long a burst lasts, and how long the quiet between bursts is. Both are jittered by
	// Jitter either way, so 18 s with 0.35 jitter means somewhere between 12 and 24.
	UPROPERTY(EditAnywhere, Category = "Pulsed FX") float BurstSeconds = 2.2f;
	UPROPERTY(EditAnywhere, Category = "Pulsed FX") float QuietSeconds = 17.0f;
	UPROPERTY(EditAnywhere, Category = "Pulsed FX", meta = (ClampMin = "0.0", ClampMax = "0.9"))
	float Jitter = 0.35f;

	// The burst sound, as a loose file under RawAudio. Empty for a silent effect.
	UPROPERTY(EditAnywhere, Category = "Pulsed FX") FString BurstSound = TEXT("steam_burst_1.wav");
	UPROPERTY(EditAnywhere, Category = "Pulsed FX") float BurstVolume = 0.30f;
	// Full level within Inner cm of the vent, inaudible Falloff cm beyond that.
	UPROPERTY(EditAnywhere, Category = "Pulsed FX") float SoundInnerCm = 300.0f;
	UPROPERTY(EditAnywhere, Category = "Pulsed FX") float SoundFalloffCm = 1400.0f;

private:
	void Fire();
	float Roll(float Base) const;

	bool bBursting = false;
	float Left = 0.0f;
};
