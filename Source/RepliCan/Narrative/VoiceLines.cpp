#include "VoiceLines.h"
#include "Audio.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Sound/SoundGroups.h"
#include "Sound/SoundWaveProcedural.h"

bool VoiceLines::LoadPcm(const FString& Path, TArray<uint8>& OutPcm, int32& OutSampleRate, int32& OutChannels)
{
	// A missing file is the normal "no voice for this line" case: stay quiet.
	if (!IFileManager::Get().FileExists(*Path)) { return false; }
	TArray<uint8> Bytes;
	if (!FFileHelper::LoadFileToArray(Bytes, *Path)) { return false; }
	FWaveModInfo Info;
	FString Error;
	if (!Info.ReadWaveInfo(Bytes.GetData(), Bytes.Num(), &Error))
	{
		UE_LOG(LogTemp, Warning, TEXT("VoiceLines: %s is not a readable WAV (%s)"), *Path, *Error);
		return false;
	}
	if (*Info.pBitsPerSample != 16 || *Info.pChannels < 1 || Info.SampleDataSize == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("VoiceLines: %s must be 16-bit PCM (got %d-bit, %d ch)"), *Path, *Info.pBitsPerSample, *Info.pChannels);
		return false;
	}
	OutPcm.SetNumUninitialized(static_cast<int32>(Info.SampleDataSize));
	FMemory::Memcpy(OutPcm.GetData(), Info.SampleDataStart, Info.SampleDataSize);
	OutSampleRate = static_cast<int32>(*Info.pSamplesPerSec);
	OutChannels = *Info.pChannels;
	return true;
}

USoundWaveProcedural* VoiceLines::MakeWave(UObject* Outer, int32 SampleRate, int32 Channels, int32 PcmBytes)
{
	// UE 5.8's mixer has no "play this raw PCM" path for a plain USoundWave
	// any more, so the samples go through a procedural wave's queue. Such a
	// wave never reports itself finished; the caller stops it after Duration.
	USoundWaveProcedural* Sound = NewObject<USoundWaveProcedural>(Outer);
	Sound->SetSampleRate(SampleRate);
	Sound->NumChannels = Channels;
	Sound->SampleByteSize = 2;
	Sound->SoundGroup = SOUNDGROUP_Voice;
	Sound->bLooping = false;
	const int32 NumFrames = PcmBytes / (2 * FMath::Max(1, Channels));
	Sound->Duration = SampleRate > 0 ? NumFrames / static_cast<float>(SampleRate) : 0.0f;
	return Sound;
}

USoundWave* VoiceLines::LoadWav(UObject* Outer, const FString& Path, float& OutSeconds)
{
	OutSeconds = 0.0f;
	TArray<uint8> Pcm;
	int32 SampleRate = 0, Channels = 0;
	if (!LoadPcm(Path, Pcm, SampleRate, Channels)) { return nullptr; }
	USoundWaveProcedural* Sound = MakeWave(Outer, SampleRate, Channels, Pcm.Num());
	Sound->QueueAudio(Pcm.GetData(), Pcm.Num());
	OutSeconds = Sound->Duration;
	return Sound;
}
