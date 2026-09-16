// Baked NPC voice lines: loose 16-bit PCM WAV files written by
// Tools/bake_voices.py (a pre-build step, see Source/*.Target.cs) into
// <Project>/Conversations/Voice/<Character>/<node>_<lineIndex>.wav. They are
// loaded straight from disk at play time into a procedural sound wave, so no
// editor import is needed and a rebake is picked up on the next Talk.
#pragma once

#include "CoreMinimal.h"

class USoundWave;
class USoundWaveProcedural;

namespace VoiceLines
{
	// Loads a 16-bit PCM WAV into a playable sound. Returns null (and logs)
	// when the file is missing or not plain PCM. OutSeconds is its length.
	REPLICAN_API USoundWave* LoadWav(UObject* Outer, const FString& Path, float& OutSeconds);
	// The pieces LoadWav is made of, for callers that loop or reuse PCM.
	REPLICAN_API bool LoadPcm(const FString& Path, TArray<uint8>& OutPcm, int32& OutSampleRate, int32& OutChannels);
	REPLICAN_API USoundWaveProcedural* MakeWave(UObject* Outer, int32 SampleRate, int32 Channels, int32 PcmBytes);
}
