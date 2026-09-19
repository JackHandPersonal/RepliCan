// SoundWatch: every sound that STARTS over a window, with the time it started.
//
// SoundDump answers "what is playing right now", and for the clicking that turned out to be the
// wrong question. A tick is a tenth of a second long; a snapshot taken once a minute has about a
// one-in-six-hundred chance of catching one. So the dump kept coming back clean -- five ambient
// loops, no leak -- while the clicking went on, and the clean reading was mistaken for evidence.
//
// This watches instead of sampling: once a frame it walks the live audio components and records the
// ones it has not seen before. A sound too short to ever appear in a snapshot cannot hide from it.
// What identifies a culprit is the GAP -- something firing every few seconds, over and over, with
// no gameplay reason, is the thing being heard.
//
//   SoundWatch 60      -- watch for a minute, then write Saved/ClaudeAssist/sound_watch.json

#include "CoreMinimal.h"
#include "Components/AudioComponent.h"
#include "Sound/SoundBase.h"
#include "UObject/UObjectIterator.h"
#include "UObject/ObjectKey.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Containers/Ticker.h"
#include "Engine/Engine.h"
#include "HAL/IConsoleManager.h"

namespace
{
	struct FStart { double T; FString Sound; bool bPersistent; };

	struct FWatch
	{
		TSet<FObjectKey> Seen;
		TArray<FStart> Starts;
		double Began = 0.0;
		double Window = 60.0;
		FTSTicker::FDelegateHandle Handle;
		bool bRunning = false;
	};
	FWatch GWatch;

	void Finish()
	{
		GWatch.bRunning = false;
		if (GWatch.Handle.IsValid()) { FTSTicker::GetCoreTicker().RemoveTicker(GWatch.Handle); GWatch.Handle.Reset(); }

		// Group by sound: how many times it started, and how often. A steady small gap is a pool
		// firing on a timer; a single start is a one-off nobody would call "all the time".
		TMap<FString, TArray<double>> When;
		for (const FStart& S : GWatch.Starts) { When.FindOrAdd(S.Sound).Add(S.T); }

		TArray<TPair<FString, TArray<double>>> Rows;
		for (const TPair<FString, TArray<double>>& P : When) { Rows.Add(P); }
		Rows.Sort([](const TPair<FString, TArray<double>>& A, const TPair<FString, TArray<double>>& B) { return A.Value.Num() > B.Value.Num(); });

		TArray<FString> L;
		L.Add(TEXT("{"));
		L.Add(FString::Printf(TEXT("  \"window_seconds\": %.1f, \"sounds_started\": %d,"), GWatch.Window, GWatch.Starts.Num()));
		L.Add(TEXT("  \"by_sound\": ["));
		for (int32 i = 0; i < Rows.Num(); ++i)
		{
			const TArray<double>& T = Rows[i].Value;
			double Gap = -1.0;
			if (T.Num() > 1) { Gap = (T.Last() - T[0]) / double(T.Num() - 1); }
			L.Add(FString::Printf(TEXT("    { \"sound\": \"%s\", \"starts\": %d, \"first_s\": %.1f, \"last_s\": %.1f, \"mean_gap_s\": %.2f }%s"),
				*Rows[i].Key, T.Num(), T[0], T.Last(), Gap, (i + 1 < Rows.Num()) ? TEXT(",") : TEXT("")));
			UE_LOG(LogTemp, Log, TEXT("SoundWatch: %-30s started %3d times, one every %.2f s"), *Rows[i].Key, T.Num(), Gap);
		}
		L.Add(TEXT("  ]"));
		L.Add(TEXT("}"));
		FFileHelper::SaveStringToFile(FString::Join(L, TEXT("\n")),
			*FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("ClaudeAssist"), TEXT("sound_watch.json")));
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 8.0f, FColor::Green,
				FString::Printf(TEXT("SoundWatch: %d sounds started over %.0f s -- written to Saved/ClaudeAssist"), GWatch.Starts.Num(), GWatch.Window));
		}
	}

	bool Poll(float)
	{
		const double Now = FPlatformTime::Seconds();
		for (TObjectIterator<UAudioComponent> It; It; ++It)
		{
			UAudioComponent* C = *It;
			if (!IsValid(C) || C->IsTemplate()) { continue; }
			const UWorld* CW = C->GetWorld();
			const bool bPersistent = (CW == nullptr);   // 2D beds are outered to the game instance
			if (!bPersistent && !(CW && CW->IsGameWorld())) { continue; }
			const FObjectKey Key(C);
			if (GWatch.Seen.Contains(Key)) { continue; }
			GWatch.Seen.Add(Key);
			GWatch.Starts.Add({ Now - GWatch.Began, C->Sound ? C->Sound->GetName() : TEXT("(no sound)"), bPersistent });
		}
		if (Now - GWatch.Began >= GWatch.Window) { Finish(); return false; }
		return true;
	}

	void Begin(const TArray<FString>& Args)
	{
		if (GWatch.bRunning) { Finish(); }
		GWatch.Seen.Empty(); GWatch.Starts.Empty();
		GWatch.Window = (Args.Num() > 0) ? FMath::Max(2.0, FCString::Atod(*Args[0])) : 60.0;
		GWatch.Began = FPlatformTime::Seconds();
		GWatch.bRunning = true;
		// The components alive at this instant are the ones ALREADY playing, not starts: seed the
		// seen set with them so the beds do not show up as if they had just fired.
		for (TObjectIterator<UAudioComponent> It; It; ++It)
		{
			if (IsValid(*It) && !(*It)->IsTemplate()) { GWatch.Seen.Add(FObjectKey(*It)); }
		}
		GWatch.Handle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateStatic(&Poll), 0.0f);
		UE_LOG(LogTemp, Log, TEXT("SoundWatch: watching for %.0f seconds"), GWatch.Window);
		if (GEngine) { GEngine->AddOnScreenDebugMessage(-1, 4.0f, FColor::Yellow, FString::Printf(TEXT("SoundWatch: listening for %.0f s"), GWatch.Window)); }
	}

	FAutoConsoleCommand GSoundWatch(
		TEXT("SoundWatch"),
		TEXT("Records every sound that STARTS over the next N seconds (default 60) and writes Saved/ClaudeAssist/sound_watch.json. Use this rather than SoundDump for short repeating sounds: a tenth-of-a-second tick is far too brief for a snapshot to catch."),
		FConsoleCommandWithArgsDelegate::CreateStatic(&Begin));
}
