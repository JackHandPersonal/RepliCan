// SoundDump: every audio component alive right now, and what it is playing.
//
// The clicking in this game has been chased four times by reasoning about the code, and each hunt
// found a real fault that was not the one being heard. That is what guessing costs. This asks the
// engine instead: walk every UAudioComponent in existence, group them by the sound they hold, and
// say how many there are, how many are still playing, and how many have been left behind.
//
// The number that matters is the one that GROWS. A leak shows as a count that climbs and never
// falls; a designed sound shows as a count that holds steady while you can hear it. Run it twice a
// minute apart and the difference names the culprit without any theorising at all.
//
// Writes Saved/ClaudeAssist/sound_dump.json as well as logging, so the answer can be read from
// outside the running game.

#include "CoreMinimal.h"
#include "Components/AudioComponent.h"
#include "Sound/SoundBase.h"
#include "UObject/UObjectIterator.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Engine/Engine.h"
#include "HAL/IConsoleManager.h"

namespace
{
	struct FTally
	{
		int32 Total = 0;
		int32 Playing = 0;
		int32 Idle = 0;      // alive but not playing: the shape a starved procedural wave leaves
		int32 Persistent = 0;   // outered to the game instance rather than a world: the 2D ambient beds
	};

	void DumpSounds(const TArray<FString>& Args)
	{
		TMap<FString, FTally> BySound;
		FTally All;
		for (TObjectIterator<UAudioComponent> It; It; ++It)
		{
			UAudioComponent* C = *It;
			if (!IsValid(C) || C->IsTemplate()) { continue; }
			// A COMPONENT WITH NO WORLD IS NOT A COMPONENT TO SKIP. SpawnSound2D with
			// bPersistAcrossLevelTransition outers the component to the game instance so it survives
			// a level change, and GetWorld() then returns null -- which is every one of the 2D
			// ambient beds. A world filter alone hid exactly the sounds most worth counting, and the
			// first reading taken with it showed two components in a station that hums continuously.
			const UWorld* CW = C->GetWorld();
			const bool bGame = CW && CW->IsGameWorld();
			const bool bPersistent = (CW == nullptr);
			if (!bGame && !bPersistent) { continue; }   // an editor-world component is not what is being counted
			const FString Name = C->Sound ? C->Sound->GetName() : TEXT("(no sound)");
			FTally& T = BySound.FindOrAdd(Name);
			const bool bPlaying = C->IsPlaying();
			T.Total++; All.Total++;
			if (bPlaying) { T.Playing++; All.Playing++; }
			else { T.Idle++; All.Idle++; }
			if (bPersistent) { T.Persistent++; All.Persistent++; }
		}

		BySound.ValueSort([](const FTally& A, const FTally& B) { return A.Total > B.Total; });

		TArray<FString> Lines;
		Lines.Add(TEXT("{"));
		Lines.Add(FString::Printf(TEXT("  \"components\": %d, \"playing\": %d, \"idle\": %d, \"persistent\": %d,"), All.Total, All.Playing, All.Idle, All.Persistent));
		Lines.Add(TEXT("  \"by_sound\": ["));
		int32 Written = 0;
		for (const TPair<FString, FTally>& P : BySound)
		{
			Lines.Add(FString::Printf(TEXT("    { \"sound\": \"%s\", \"total\": %d, \"playing\": %d, \"idle\": %d, \"persistent\": %d }%s"),
				*P.Key, P.Value.Total, P.Value.Playing, P.Value.Idle, P.Value.Persistent,
				(++Written < BySound.Num()) ? TEXT(",") : TEXT("")));
			UE_LOG(LogTemp, Log, TEXT("SoundDump: %-34s total %3d  playing %3d  idle %3d  persistent %3d"),
				*P.Key, P.Value.Total, P.Value.Playing, P.Value.Idle, P.Value.Persistent);
		}
		Lines.Add(TEXT("  ]"));
		Lines.Add(TEXT("}"));
		UE_LOG(LogTemp, Log, TEXT("SoundDump: %d audio components, %d playing, %d idle"), All.Total, All.Playing, All.Idle);

		const FString Path = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("ClaudeAssist"), TEXT("sound_dump.json"));
		FFileHelper::SaveStringToFile(FString::Join(Lines, TEXT("\n")), *Path);
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 6.0f, FColor::Green,
				FString::Printf(TEXT("SoundDump: %d components, %d playing, %d idle -- written to Saved/ClaudeAssist"), All.Total, All.Playing, All.Idle));
		}
	}

	FAutoConsoleCommand GSoundDump(
		TEXT("SoundDump"),
		TEXT("Counts every audio component in the running game, grouped by sound, and writes Saved/ClaudeAssist/sound_dump.json. Run it twice a minute apart: a count that climbs is a leak, a count that holds is a sound that is meant to be there."),
		FConsoleCommandWithArgsDelegate::CreateStatic(&DumpSounds));
}
