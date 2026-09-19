// Background sound beds and one-shots from loose WAVs in <Project>/RawAudio
// (synthesized by scripts, no asset import). A profile is a set of loops plus
// pools of one-shots fired at random intervals -- "facility": the asteroid
// base bed (three layers with mutually prime lengths, each started at a
// random offset and drifting slowly in level, so the mix never lines up),
// the respirator, near fans, and pools of distant thuds, duct ticks, metal
// creaks and pressure hisses. Loops are kept alive by re-queueing their PCM
// whenever the procedural wave runs low. Used by the title screens and by
// the gameplay controller in the facility.
#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "AmbientPlayer.generated.h"

class UAudioComponent;
class USoundWaveProcedural;

UCLASS()
class REPLICAN_API UAmbientPlayer : public UObject
{
	GENERATED_BODY()

public:
	// Starts the named profile in the world; a running profile is stopped first.
	void Start(UWorld* World, const FString& Profile);
	void Stop();
	bool IsPlaying() const { return Loops.Num() > 0; }
	// Scales every loop (and silences the one-shot pools) over Seconds: 0 = passed out.
	void SetFade(float Level, float Seconds);
	FString Describe() const;

	// Plays <RawAudio>/<File> once (2D). Returns its length, 0 if missing.
	// One-shots are ducked by the last shot's overpressure (see NoteShot) unless they ARE the shot.
	static float PlayOneShot(UObject* Outer, UWorld* World, const FString& File, float Volume, float Pitch = 1.0f, bool bIgnoreDuck = false);
	// A SHOT. Everything else drops by Depth at once and comes back over Seconds -- the ambience
	// through the loops' fade, the impacts and clicks through PlayOneShot's duck -- so the report
	// is the loudest thing on the deck and the room is heard returning after it.
	void Duck(UWorld* World, float Depth, float Seconds);
	static void NoteShot(float Depth, float Seconds);
	static float CurrentDuck();
	static FString RawAudioDir();

private:
	struct FLoop
	{
		TArray<uint8> Pcm;
		float BaseVolume = 1.0f;
	};
	// A family of one-shots fired at random gaps with random level and pitch.
	struct FPool
	{
		TArray<FString> Files;
		FVector2D Interval = FVector2D(8.0f, 20.0f);
		FVector2D Volume = FVector2D(0.5f, 1.0f);
		FVector2D Pitch = FVector2D(0.9f, 1.1f);
		FTimerHandle Timer;
	};
	// A loop is 2D unless At is given: then it plays from that point, full level inside Inner cm
	// and gone Falloff cm further out (the respirator lives at the bay's machines).
	void AddLoop(UWorld* World, const FString& File, float Volume, const FVector* At = nullptr, float Inner = 0.0f, float Falloff = 0.0f);
	// Positional hisses for the vents Tools/facility_layout.py placed; see UI/SteamVents.json.
	void AddSteamVents(UWorld* World);
	void AddPool(const TArray<FString>& Files, FVector2D Interval, FVector2D Volume, FVector2D Pitch);
	void SchedulePool(int32 Index);
	void FirePool(int32 Index);
	void DriftLevels();

	UPROPERTY() TArray<TObjectPtr<UAudioComponent>> Components;
	UPROPERTY() TArray<TObjectPtr<USoundWaveProcedural>> Waves;
	TArray<TSharedPtr<FLoop>> Loops;   // stable storage for the audio-thread requeue
	float PreDuckFade = 1.0f;
	FTimerHandle DuckTimer;
	TArray<TSharedPtr<FPool>> Pools;
	float Fade = 1.0f;
	FTimerHandle DriftTimer;
	TWeakObjectPtr<UWorld> ThudWorld;
	FString CurrentProfile;
};
