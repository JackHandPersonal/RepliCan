// One actor that owns the level's atmosphere, so the facility map itself is never rewritten to
// change it. The director creates and drives what it needs at play time -- the post process
// volume, the height fog, the dust in the air, the unsteadiness in the lamps -- reading each
// feature's on/off and strength from URepliCanUserSettings. Everything it makes is transient and
// tagged, so a re-entry replaces its own work rather than accumulating copies.
//
// Why an actor rather than placed assets: Tools/facility_layout.py is additive and honours the
// level edits made by hand, so regenerating the map to add a fog actor is exactly the thing that
// must not happen. A director also gives every feature a runtime switch, which is what the
// settings page needs.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EnvironmentDirector.generated.h"

class APostProcessVolume;
class AExponentialHeightFog;
class APointLight;
class UInstancedStaticMeshComponent;

UCLASS()
class REPLICAN_API AEnvironmentDirector : public AActor
{
	GENERATED_BODY()

public:
	AEnvironmentDirector();

	// Finds the director in the world, spawning one if there is not already one. Called by the
	// game mode; safe to call more than once.
	static AEnvironmentDirector* Ensure(UWorld* World);

	// Re-reads every setting and rebuilds whatever changed. The settings page calls this after
	// a toggle so the change is visible without a reload.
	UFUNCTION(Exec, BlueprintCallable, Category = "Environment") void ApplyEnvironment();

	// Console switches, so each feature can be judged on its own while tuning.
	UFUNCTION(Exec) void EnvFog(int32 OnOff);
	UFUNCTION(Exec) void EnvVolumetric(int32 OnOff);
	UFUNCTION(Exec) void EnvAO(int32 OnOff);
	UFUNCTION(Exec) void EnvGrade(int32 OnOff);
	UFUNCTION(Exec) void EnvFixedExposure(int32 OnOff);
	UFUNCTION(Exec) void EnvExposure(int32 Step);        // 0..4
	UFUNCTION(Exec) void EnvDust(int32 OnOff);
	UFUNCTION(Exec) void EnvAOStrength(int32 Step);   // 0..7 the usable ladder; 8 and 9 are deliberately overdone, to see what AO does
	// Spawn a pest immediately, in front of the player instead of behind them. They are designed
	// to appear only where you are NOT looking, which makes "are they working at all?" an
	// unanswerable question without this.
	UFUNCTION(Exec) void EnvPestNow();
	UFUNCTION(Exec) void EnvFlicker(int32 OnOff);
	UFUNCTION(Exec) void EnvFogDensity(int32 Step);      // 0..4
	UFUNCTION(Exec) void EnvDustDensity(int32 Step);     // 0..4
	// Vermin. On by default: an abandoned-feeling station with nothing alive in it reads as
	// empty rather than as abandoned, and something small moving at the edge of vision is the
	// cheapest way there is to make a corridor feel like a place.
	UFUNCTION(Exec) void EnvPests(int32 OnOff);
	UFUNCTION(Exec) void EnvPestRate(int32 Step);        // 0..4, how often they turn up

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
	void ApplyPostProcess();
	void ApplyFog();
	void ApplyDust();
	void GatherLamps();
	void TickDust(float DeltaSeconds);
	void TickFlicker(float DeltaSeconds);

	// ---- Pests ---------------------------------------------------------------
	// The trick to "appearing and disappearing" is that neither is ever witnessed. A pest is
	// spawned only at a spot the player CANNOT currently see -- behind them, or behind
	// something -- and sent to another such spot. What the player sees is the middle of the
	// journey: a thing darting across the room and vanishing behind a crate. It was never
	// seen to arrive and it will not be seen to leave.
	void TickPests(float DeltaSeconds);
	// Spawns one at Where, heading for Goal. Shared by the timer and by EnvPestNow.
	bool SpawnPestAt(const FVector& Where, const FVector& Goal);
	void LoadPestMeshes();
	// SSAO only darkens AMBIENT light, so it needs some to work on. Remembered at BeginPlay and
	// pushed up by the two demo steps.
	float SkyLightBaseIntensity = -1.0f;
	void ApplySkyLightForAO(int32 Step);
	// A point on the floor, roughly this far from the player, that the player cannot see.
	// Returns false when it could not find one in a reasonable number of tries -- in an open
	// room with nothing to hide behind, that is the correct answer and no pest appears.
	bool FindHiddenFloorPoint(const FVector& Around, float MinRadius, float MaxRadius, FVector& Out) const;
	bool CanPlayerSee(const FVector& Point) const;

	// Seconds until the next one. Reset to a fresh random gap each time one is spawned.
	float PestCountdown = 0.0f;
	UPROPERTY() TArray<TObjectPtr<class APestActor>> Pests;
	// The three generated bodies, loaded once. See Tools/make_pests.py.
	UPROPERTY() TArray<TObjectPtr<class UStaticMesh>> PestMeshes;
	// Never more than this on screen at once: two is a nest, five is an infestation and reads
	// as a gameplay event rather than as atmosphere.
	UPROPERTY(EditAnywhere, Category = "Environment|Pests") int32 MaxPests = 6;      // was 2: they come in numbers now
	UPROPERTY(EditAnywhere, Category = "Environment|Pests") FVector2D PestGapSeconds = FVector2D(4.0f, 14.0f);   // was 14..48: about three times as often
	// Close enough to notice, far enough not to be examined.
	UPROPERTY(EditAnywhere, Category = "Environment|Pests") float PestNearCm = 320.0f;
	UPROPERTY(EditAnywhere, Category = "Environment|Pests") float PestFarCm = 950.0f;
	UPROPERTY(EditAnywhere, Category = "Environment|Pests") float PestScale = 1.3f;   // a little bigger than the pack ships them

	// The two world actors the director owns. Both are spawned transient: nothing it makes is
	// ever saved into the map.
	UPROPERTY() TObjectPtr<APostProcessVolume> Volume;
	UPROPERTY() TObjectPtr<AExponentialHeightFog> Fog;
	// Motes are one instanced mesh rather than many actors: a few hundred instances cost one
	// draw call, and they only ever exist in a box around the camera.
	UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> Dust;

	// A lamp the director makes unsteady, with the intensity it had before it started.
	struct FLamp
	{
		TWeakObjectPtr<APointLight> Light;
		float BaseIntensity = 0.0f;
		float Phase = 0.0f;
		float Rate = 1.0f;
		float Depth = 0.05f;
	};
	TArray<FLamp> Lamps;
	bool bLampsGathered = false;

	// One mote's drift, in world units per second, plus where it is inside the box.
	TArray<FVector> DustVelocity;
	TArray<FVector> DustPosition;
	FVector DustBoxCentre = FVector::ZeroVector;
	float Clock = 0.0f;
};
