#include "World/AmbientPlayer.h"
#include "Core/JsonDataFile.h"
#include "Sound/SoundAttenuation.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonReader.h"
#include "Narrative/VoiceLines.h"
#include "Components/AudioComponent.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Sound/SoundWaveProcedural.h"
#include "Sound/SoundAttenuation.h"
#include "TimerManager.h"

FString UAmbientPlayer::RawAudioDir()
{
	return FPaths::ConvertRelativePathToFull(FPaths::Combine(JsonData::DataDir(), TEXT("RawAudio")));
}

void UAmbientPlayer::Start(UWorld* World, const FString& Profile)
{
	Stop();
	if (!World) { return; }
	CurrentProfile = Profile;
	ThudWorld = World;
	if (Profile == TEXT("facility"))
	{
		// The base itself, three layers whose lengths share no factor.
		AddLoop(World, TEXT("base_floor_loop.wav"), 0.30f);
		AddLoop(World, TEXT("base_fans_loop.wav"), 0.26f);
		AddLoop(World, TEXT("base_hum_loop.wav"), 0.22f);
		{ const FVector Machines(500.0f, 300.0f, 110.0f); AddLoop(World, TEXT("respirator_loop.wav"), 0.34f, &Machines, 300.0f, 650.0f); }   // only near the bay's medical machines
		AddLoop(World, TEXT("fan_loop.wav"), 0.14f);
		AddPool({ TEXT("thud_01.wav"), TEXT("thud_02.wav"), TEXT("thud_03.wav") }, FVector2D(9.0f, 24.0f), FVector2D(0.27f, 0.45f), FVector2D(0.85f, 1.1f));
		// NO TICK POOL. This is the clicking that was reported six times and hunted five times through
		// the code, where there was nothing to find: nothing was broken, this bed was simply playing a
		// tenth-of-a-second tick every three to eleven seconds, everywhere in the map, forever, with
		// its pitch scattered over 0.8-1.25 -- which is what made it read as an insect rather than as
		// machinery. Wall clocks are not what this room should sound like; the creaks and hisses below
		// are the atmosphere, and they are rare enough to be atmosphere. The wavs are left on disk.
		AddPool({ TEXT("emb_creak_01.wav"), TEXT("emb_creak_02.wav"), TEXT("emb_creak_03.wav") }, FVector2D(12.0f, 40.0f), FVector2D(0.03f, 0.07f), FVector2D(0.85f, 1.15f));
		AddPool({ TEXT("emb_hiss_01.wav"), TEXT("emb_hiss_02.wav"), TEXT("emb_hiss_03.wav") }, FVector2D(10.0f, 30.0f), FVector2D(0.04f, 0.09f), FVector2D(0.9f, 1.1f));
	}
	else if (Profile == TEXT("deck"))
	{
		// THE SERVICE DECK (Tools/make_deck_sounds): the base's floor hum lower, a compressor that
		// cycles up and down on its own, a cable hum, drips into a puddle, a far clank now and then,
		// and the pressure hisses. The player controller switches to this bed below the lift's
		// lowest stop and back to the facility's above it.
		AddLoop(World, TEXT("base_floor_loop.wav"), 0.18f);
		AddLoop(World, TEXT("deck_compressor_loop.wav"), 0.30f);
		AddLoop(World, TEXT("cable_hum_loop.wav"), 0.10f);
		AddPool({ TEXT("drip_01.wav"), TEXT("drip_02.wav"), TEXT("drip_03.wav"), TEXT("drip_04.wav"), TEXT("drip_05.wav"), TEXT("drip_06.wav") }, FVector2D(3.0f, 11.0f), FVector2D(0.14f, 0.32f), FVector2D(0.85f, 1.2f));
		AddPool({ TEXT("deck_clank_01.wav"), TEXT("deck_clank_02.wav"), TEXT("deck_clank_03.wav") }, FVector2D(14.0f, 45.0f), FVector2D(0.10f, 0.22f), FVector2D(0.8f, 1.1f));
		AddPool({ TEXT("emb_hiss_01.wav"), TEXT("emb_hiss_02.wav"), TEXT("emb_hiss_03.wav") }, FVector2D(12.0f, 32.0f), FVector2D(0.04f, 0.09f), FVector2D(0.9f, 1.1f));
		AddSteamVents(World);
		World->GetTimerManager().SetTimer(DriftTimer, this, &UAmbientPlayer::DriftLevels, 7.0f, true);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("Ambient: unknown profile '%s'"), *Profile);
	}
}

// Every visible steam vent gets an audible one, from the list Tools/facility_layout.py writes
// when it places them. Reading the file rather than repeating the coordinates here is the whole
// point: a vent moved in the layout script moves its own sound, and a vent nobody placed makes
// no noise. A missing file is not an error -- a level with no steam in it is a valid level.
void UAmbientPlayer::AddSteamVents(UWorld* World)
{
	TSharedPtr<FJsonObject> Root = JsonData::Load(TEXT("UI"), TEXT("SteamVents.json"), TEXT("SteamVents"));
	if (!Root.IsValid()) { return; }
	const TArray<TSharedPtr<FJsonValue>>* Vents = nullptr;
	if (!Root->TryGetArrayField(TEXT("vents"), Vents) || !Vents) { return; }

	int32 Added = 0;
	for (const TSharedPtr<FJsonValue>& V : *Vents)
	{
		const TSharedPtr<FJsonObject> Obj = V->AsObject();
		if (!Obj.IsValid()) { continue; }
		FString Sound;
		if (!Obj->TryGetStringField(TEXT("sound"), Sound) || Sound.IsEmpty()) { continue; }
		const TArray<TSharedPtr<FJsonValue>>* At = nullptr;
		if (!Obj->TryGetArrayField(TEXT("at"), At) || !At || At->Num() < 3) { continue; }
		const FVector Where((*At)[0]->AsNumber(), (*At)[1]->AsNumber(), (*At)[2]->AsNumber());
		double Volume = 0.2, Inner = 260.0, Falloff = 900.0;
		Obj->TryGetNumberField(TEXT("volume"), Volume);
		Obj->TryGetNumberField(TEXT("inner"), Inner);
		Obj->TryGetNumberField(TEXT("falloff"), Falloff);
		AddLoop(World, Sound.EndsWith(TEXT(".wav")) ? Sound : Sound + TEXT(".wav"), Volume, &Where, Inner, Falloff);
		++Added;
	}
	UE_LOG(LogTemp, Log, TEXT("Ambient: %d steam vents"), Added);
}

void UAmbientPlayer::AddLoop(UWorld* World, const FString& File, float Volume, const FVector* At, float Inner, float Falloff)
{
	// THE IMPORTED LOOP FIRST: no underflow callback, no seam, no click. See the note above
	// AddLoop -- the seam is the mixer's problem once the sound is an asset, and the mixer does
	// not drop a sample at it.
	if (USoundBase* Sample = SampleFor(File))
	{
		USoundAttenuation* Att = nullptr;
		if (At)
		{
			Att = NewObject<USoundAttenuation>(this);
			Att->Attenuation.bAttenuate = true;
			Att->Attenuation.bSpatialize = true;
			Att->Attenuation.AttenuationShape = EAttenuationShape::Sphere;
			Att->Attenuation.AttenuationShapeExtents = FVector(Inner, 0.0f, 0.0f);
			Att->Attenuation.FalloffDistance = Falloff;
		}
		UAudioComponent* C = At
			? UGameplayStatics::SpawnSoundAtLocation(World, Sample, *At, FRotator::ZeroRotator, Volume, 1.0f, 0.0f, Att, nullptr, false)
			: UGameplayStatics::SpawnSound2D(World, Sample, Volume, 1.0f, 0.0f, nullptr, true, false);
		if (C)
		{
			// Loops, Waves and Components are walked in step by the volume drift and the shot duck,
			// so all three take an entry even though an imported sound needs no wave of its own.
			TSharedPtr<FLoop> Imported = MakeShared<FLoop>();
			Imported->BaseVolume = Volume;
			Loops.Add(Imported);
			Waves.Add(nullptr);
			Components.Add(C);
			return;
		}
	}
	TSharedPtr<FLoop> Loop = MakeShared<FLoop>();
	Loop->BaseVolume = Volume;
	int32 Rate = 0, Channels = 0;
	const FString Path = FPaths::Combine(RawAudioDir(), File);
	if (!VoiceLines::LoadPcm(Path, Loop->Pcm, Rate, Channels))
	{
		UE_LOG(LogTemp, Warning, TEXT("Ambient: %s missing or not 16-bit PCM"), *Path);
		return;
	}
	USoundWaveProcedural* Wave = VoiceLines::MakeWave(this, Rate, Channels, Loop->Pcm.Num());
	// Start somewhere random inside the loop (on a frame boundary), then a
	// full copy, then one more each time the wave runs low. That callback
	// fires on the audio thread; the PCM lives in a shared block that
	// outlives the wave, so it is safe to read there.
	const int32 Frame = 2 * FMath::Max(1, Channels);
	const int32 Offset = FMath::RandRange(0, FMath::Max(1, Loop->Pcm.Num() / Frame - 1)) * Frame;
	Wave->QueueAudio(Loop->Pcm.GetData() + Offset, Loop->Pcm.Num() - Offset);
	Wave->QueueAudio(Loop->Pcm.GetData(), Loop->Pcm.Num());
	Wave->OnSoundWaveProceduralUnderflow.BindLambda([Loop](USoundWaveProcedural* W, int32)
	{
		if (Loop.IsValid() && Loop->Pcm.Num() > 0) { W->QueueAudio(Loop->Pcm.GetData(), Loop->Pcm.Num()); }
	});
	UAudioComponent* Comp = nullptr;
	if (At)
	{
		USoundAttenuation* Att = NewObject<USoundAttenuation>(this);
		Att->Attenuation.bAttenuate = true;
		Att->Attenuation.bSpatialize = true;
		Att->Attenuation.AttenuationShape = EAttenuationShape::Sphere;
		Att->Attenuation.AttenuationShapeExtents = FVector(Inner, 0.0f, 0.0f);
		Att->Attenuation.FalloffDistance = Falloff;
		Comp = UGameplayStatics::SpawnSoundAtLocation(World, Wave, *At, FRotator::ZeroRotator, Volume, 1.0f, 0.0f, Att, nullptr, true);
	}
	else { Comp = UGameplayStatics::SpawnSound2D(World, Wave, Volume, 1.0f, 0.0f, nullptr, true, false); }
	if (!Comp) { return; }
	Loops.Add(Loop);
	Waves.Add(Wave);
	Components.Add(Comp);
}

void UAmbientPlayer::AddPool(const TArray<FString>& Files, FVector2D Interval, FVector2D Volume, FVector2D Pitch)
{
	TSharedPtr<FPool> Pool = MakeShared<FPool>();
	Pool->Files = Files; Pool->Interval = Interval; Pool->Volume = Volume; Pool->Pitch = Pitch;
	Pools.Add(Pool);
	SchedulePool(Pools.Num() - 1);
}

void UAmbientPlayer::SchedulePool(int32 Index)
{
	UWorld* World = ThudWorld.Get();
	if (!World || !Pools.IsValidIndex(Index)) { return; }
	FPool& Pool = *Pools[Index];
	World->GetTimerManager().SetTimer(Pool.Timer, FTimerDelegate::CreateUObject(this, &UAmbientPlayer::FirePool, Index), FMath::FRandRange(Pool.Interval.X, Pool.Interval.Y), false);
}

void UAmbientPlayer::FirePool(int32 Index)
{
	UWorld* World = ThudWorld.Get();
	if (World && Fade > 0.05f && Pools.IsValidIndex(Index) && Pools[Index]->Files.Num() > 0)
	{
		const FPool& Pool = *Pools[Index];
		PlayOneShot(this, World, Pool.Files[FMath::RandRange(0, Pool.Files.Num() - 1)], FMath::FRandRange(Pool.Volume.X, Pool.Volume.Y), FMath::FRandRange(Pool.Pitch.X, Pool.Pitch.Y));
	}
	SchedulePool(Index);
}

void UAmbientPlayer::SetFade(float Level, float Seconds)
{
	Fade = FMath::Clamp(Level, 0.0f, 1.0f);
	for (int32 i = 0; i < Components.Num() && i < Loops.Num(); ++i)
	{
		if (UAudioComponent* C = Components[i])
		{
			// A component faded to nothing is stopped by the engine, so the floor stays audible-but-buried and a stopped loop is restarted.
			if (!C->IsPlaying() && Fade > 0.0f) { C->Play(); }
			C->AdjustVolume(FMath::Max(0.01f, Seconds), FMath::Max(0.02f, Loops[i]->BaseVolume * Fade));
		}
	}
}

void UAmbientPlayer::DriftLevels()
{
	// Each loop wanders within +-25% of its base level over several seconds,
	// so even the continuous layers never sit at one fixed balance.
	for (int32 i = 0; i < Components.Num() && i < Loops.Num(); ++i)
	{
		if (UAudioComponent* C = Components[i])
		{
			C->AdjustVolume(FMath::FRandRange(4.0f, 9.0f), Loops[i]->BaseVolume * FMath::FRandRange(0.75f, 1.25f) * Fade);
		}
	}
}

void UAmbientPlayer::Stop()
{
	if (UWorld* World = ThudWorld.Get())
	{
		World->GetTimerManager().ClearTimer(DriftTimer);
		for (const TSharedPtr<FPool>& Pool : Pools) { World->GetTimerManager().ClearTimer(Pool->Timer); }
	}
	for (USoundWaveProcedural* Wave : Waves) { if (Wave) { Wave->OnSoundWaveProceduralUnderflow.Unbind(); } }
	for (UAudioComponent* Comp : Components)
	{
		if (Comp) { Comp->Stop(); Comp->DestroyComponent(); }
	}
	Components.Reset();
	Waves.Reset();
	Loops.Reset();
	Pools.Reset();
	CurrentProfile.Reset();
}

namespace
{
	double GShotAt = -1000.0; float GShotDepth = 0.0f; float GShotSeconds = 1.0f;
}

void UAmbientPlayer::NoteShot(float Depth, float Seconds)
{
	GShotAt = FPlatformTime::Seconds(); GShotDepth = FMath::Clamp(Depth, 0.0f, 1.0f); GShotSeconds = FMath::Max(0.05f, Seconds);
}

float UAmbientPlayer::CurrentDuck()
{
	const float T = (float)(FPlatformTime::Seconds() - GShotAt) / GShotSeconds;
	if (T >= 1.0f) { return 0.0f; }
	const float Left = 1.0f - T;
	return GShotDepth * Left * Left;   // comes back slowly at first, then all at once, like hearing after a bang
}

void UAmbientPlayer::Duck(UWorld* World, float Depth, float Seconds)
{
	if (!World) { return; }
	if (!World->GetTimerManager().IsTimerActive(DuckTimer)) { PreDuckFade = Fade; }
	SetFade(PreDuckFade * (1.0f - FMath::Clamp(Depth, 0.0f, 1.0f)), 0.02f);
	const float Restore = PreDuckFade;
	World->GetTimerManager().SetTimer(DuckTimer, FTimerDelegate::CreateWeakLambda(this, [this, Restore, Seconds]() { SetFade(Restore, FMath::Max(0.1f, Seconds * 0.8f)); }), 0.15f, false);
}

USoundBase* UAmbientPlayer::SampleFor(const FString& File)
{
	FString Stem = FPaths::GetBaseFilename(File);
	if (Stem.IsEmpty()) { return nullptr; }
	// Cached, but HITS ONLY. Caching a miss looked like the same optimisation and was a trap: the
	// table is a static, so it outlives a Play session, and a sound that was missing the first time
	// it was asked for stayed missing for the life of the editor process -- which meant importing
	// the file did nothing until the editor was restarted, and the clicking it was meant to fix
	// carried on with the fix sitting on disk. A miss now costs one quiet package lookup, which is
	// what a sound that genuinely has no asset should cost.
	static TMap<FString, TWeakObjectPtr<USoundBase>> Known;
	if (TWeakObjectPtr<USoundBase>* Hit = Known.Find(Stem))
	{
		if (Hit->IsValid()) { return Hit->Get(); }
		Known.Remove(Stem);       // it was collected, or it has been imported since: ask again
	}
	USoundBase* Found = LoadObject<USoundBase>(nullptr, *FString::Printf(TEXT("/Game/RepliCan/Audio/A_%s.A_%s"), *Stem, *Stem), nullptr, LOAD_NoWarn | LOAD_Quiet);
	if (Found) { Known.Add(Stem, Found); }
	return Found;
}

USoundAttenuation* UAmbientPlayer::DefaultFalloff()
{
	// Built once and kept alive by hand: it is not an asset anyone authored, it is the rule that a
	// sound which happens SOMEWHERE should only be heard near there.
	static USoundAttenuation* A = nullptr;
	if (!A)
	{
		A = NewObject<USoundAttenuation>(GetTransientPackage(), TEXT("RepliCanPositionalFalloff"));
		A->AddToRoot();
		A->Attenuation.bAttenuate = true;
		A->Attenuation.bSpatialize = true;
		A->Attenuation.AttenuationShape = EAttenuationShape::Sphere;
		A->Attenuation.DistanceAlgorithm = EAttenuationDistanceModel::NaturalSound;
		A->Attenuation.AttenuationShapeExtents = FVector(FalloffStartCm, 0.0f, 0.0f);
		A->Attenuation.FalloffDistance = FalloffEndCm - FalloffStartCm;
	}
	return A;
}

UAudioComponent* UAmbientPlayer::PlayFileAt(UObject* Outer, UWorld* World, const FString& File,
	const FVector& At, float Volume, float Pitch, USoundAttenuation* Attenuation)
{
	if (!World || File.IsEmpty()) { return nullptr; }
	// A caller that names no attenuation is not asking to be heard everywhere; it simply has no
	// opinion. Give it the falloff rather than the whole level.
	if (!Attenuation) { Attenuation = DefaultFalloff(); }
	// The recording, if it has been imported: it ends by itself and the component goes with it.
	if (USoundBase* Sample = SampleFor(File))
	{
		return UGameplayStatics::SpawnSoundAtLocation(World, Sample, At, FRotator::ZeroRotator, Volume, Pitch, 0.0f, Attenuation, nullptr, true);
	}
	float Seconds = 0.0f;
	USoundWave* Wave = VoiceLines::LoadWav(Outer ? Outer : World, FPaths::Combine(RawAudioDir(), File), Seconds);
	if (!Wave) { return nullptr; }
	UAudioComponent* Comp = UGameplayStatics::SpawnSoundAtLocation(World, Wave, At, FRotator::ZeroRotator, Volume, Pitch, 0.0f, Attenuation, nullptr, false);
	if (!Comp) { return nullptr; }
	FTimerHandle Handle;
	TWeakObjectPtr<UAudioComponent> WeakComp = Comp;
	World->GetTimerManager().SetTimer(Handle, FTimerDelegate::CreateLambda([WeakComp]()
	{
		if (UAudioComponent* C = WeakComp.Get()) { C->Stop(); C->DestroyComponent(); }
	}), Seconds / FMath::Max(0.1f, Pitch) + 0.15f, false);
	return Comp;
}

float UAmbientPlayer::PlayOneShot(UObject* Outer, UWorld* World, const FString& File, float Volume, float Pitch, bool bIgnoreDuck)
{
	if (!bIgnoreDuck) { Volume *= (1.0f - CurrentDuck()); }
	if (!World) { return 0.0f; }
	// The recording, when there is one: it ends on its own, so it is spawned to auto-destroy and
	// no timer is involved at all.
	if (USoundBase* Sample = SampleFor(File))
	{
		UGameplayStatics::SpawnSound2D(World, Sample, Volume, Pitch, 0.0f, nullptr, false, true);
		return Sample->GetDuration();
	}
	float Seconds = 0.0f;
	USoundWave* Sound = VoiceLines::LoadWav(Outer ? Outer : World, FPaths::Combine(RawAudioDir(), File), Seconds);
	if (!Sound) { return 0.0f; }
	UAudioComponent* Comp = UGameplayStatics::SpawnSound2D(World, Sound, Volume, Pitch, 0.0f, nullptr, false, false);
	if (!Comp) { return 0.0f; }
	// A procedural wave never ends by itself; stop it once the clip is over.
	FTimerHandle Handle;
	TWeakObjectPtr<UAudioComponent> WeakComp = Comp;
	World->GetTimerManager().SetTimer(Handle, FTimerDelegate::CreateLambda([WeakComp]()
	{
		if (UAudioComponent* C = WeakComp.Get()) { C->Stop(); C->DestroyComponent(); }
	}), Seconds / FMath::Max(0.1f, Pitch) + 0.15f, false);
	return Seconds;
}

FString UAmbientPlayer::Describe() const
{
	int32 Playing = 0;
	for (const UAudioComponent* C : Components) { if (C && C->IsPlaying()) { ++Playing; } }
	return FString::Printf(TEXT("%s: %d/%d loops playing, %d one-shot pools"), *CurrentProfile, Playing, Components.Num(), Pools.Num());
}
