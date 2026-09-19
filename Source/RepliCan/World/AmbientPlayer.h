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
	const FString& GetProfile() const { return CurrentProfile; }
	// Scales every loop (and silences the one-shot pools) over Seconds: 0 = passed out.
	void SetFade(float Level, float Seconds);
	FString Describe() const;

	// Plays <RawAudio>/<File> once (2D). Returns its length, 0 if missing.
	// One-shots are ducked by the last shot's overpressure (see NoteShot) unless they ARE the shot.
	// A REAL RECORDING IN PLACE OF A SYNTHESISED ONE. Where an imported sample exists for a loose
	// .wav the code asks for -- /Game/RepliCan/Audio/A_<stem> -- it is played instead. An imported
	// sound knows its own length, ends by itself and needs no timer; the procedural waves that
	// stand in for the rest do not, which is why a one-shot whose stop timer never fired (a paused
	// game) left a starved wave clicking away. Tools/import_audio_samples.py makes them.
	static class USoundBase* SampleFor(const FString& File);   // (no REPLICAN_API: the class already carries it, and a member may not repeat it)
	// ONE WAY TO PLAY A LOOSE FILE AT A PLACE, and the only one that cleans up after itself.
	//
	// A USoundWaveProcedural never reports itself finished, so bAutoDestroy on the component does
	// nothing: the component stays alive forever, starved, and a starved procedural wave ticks. One
	// per shell casing, per steam burst, per conduit spark -- they accumulate for the whole session
	// until the room is full of them. That is the clicking. This prefers the IMPORTED asset, which
	// knows its own length and destroys itself, and stop-times the procedural fallback so even a
	// file that was never imported dies when its clip is over.
	static class UAudioComponent* PlayFileAt(UObject* Outer, UWorld* World, const FString& File,   // (no REPLICAN_API: the class already carries it)
		const FVector& At, float Volume, float Pitch = 1.0f, class USoundAttenuation* Attenuation = nullptr);
	static float PlayOneShot(UObject* Outer, UWorld* World, const FString& File, float Volume, float Pitch = 1.0f, bool bIgnoreDuck = false);
	// THE FALLOFF EVERY POSITIONED SOUND GETS WHEN IT ASKS FOR NONE. Spawning a sound "at a location"
	// with no attenuation does NOT make it quieter far away -- it plays at full volume across the
	// whole level, and the only thing the location buys is the stereo pan. Two sparking conduits on
	// the service deck were therefore crackling into every room in the facility, every few seconds,
	// all day: the clicking that was reported seven times and hunted six.
	static class USoundAttenuation* DefaultFalloff();
	static constexpr float FalloffStartCm = 300.0f;    // full volume this close, about half a room
	static constexpr float FalloffEndCm = 1500.0f;     // silent past fifteen metres: deck noise stays on the deck
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
