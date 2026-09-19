#include "Narrative/SequenceData.h"
#include "Core/JsonDataFile.h"
#include "Narrative/ConversationData.h"
#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

int32 FSequence::FindLabel(const FString& Label) const
{
	for (int32 i = 0; i < Steps.Num(); ++i)
	{
		if (Steps[i].Type == ESequenceStepType::Label && Steps[i].Label == Label) { return i; }
	}
	return INDEX_NONE;
}

FString SequenceFile::GetDirectory()
{
	return FPaths::ConvertRelativePathToFull(FPaths::Combine(JsonData::DataDir(), TEXT("Sequences")));
}

FString SequenceFile::GetPath(const FString& Name)
{
	return FPaths::Combine(GetDirectory(), Name + TEXT(".json"));
}

bool SequenceFile::Exists(const FString& Name)
{
	return !Name.IsEmpty() && IFileManager::Get().FileExists(*GetPath(Name));
}

FString SequenceFile::GetVoicePath(const FString& Name, int32 StepIndex)
{
	return FPaths::Combine(ConversationFile::GetDirectory(), TEXT("Voice"), TEXT("Sequences"), Name, FString::Printf(TEXT("%d.wav"), StepIndex));
}

namespace
{
	FVector ReadVector(const TSharedPtr<FJsonObject>& Obj, const TCHAR* Field, const FVector& Default)
	{
		const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
		if (Obj.IsValid() && Obj->TryGetArrayField(Field, Arr) && Arr->Num() >= 3)
		{
			return FVector((*Arr)[0]->AsNumber(), (*Arr)[1]->AsNumber(), (*Arr)[2]->AsNumber());
		}
		return Default;
	}

	float ReadFloat(const TSharedPtr<FJsonObject>& Obj, const TCHAR* Field, float Default)
	{
		double V = Default;
		if (Obj.IsValid() && Obj->TryGetNumberField(Field, V)) { return static_cast<float>(V); }
		return Default;
	}
}

bool SequenceFile::Load(const FString& Name, FSequence& Out)
{
	TSharedPtr<FJsonObject> Root = JsonData::LoadObject(GetPath(Name), TEXT("Sequence"));
	if (!Root.IsValid()) { return false; }

	Out = FSequence();
	Out.Name = Name;
	Root->TryGetBoolField(TEXT("unskippable"), Out.bUnskippable);
	Root->TryGetStringArrayField(TEXT("skips"), Out.SkipLabels);

	const TSharedPtr<FJsonObject>* ActorsObj = nullptr;
	if (Root->TryGetObjectField(TEXT("actors"), ActorsObj) && ActorsObj)
	{
		for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : (*ActorsObj)->Values)
		{
			const TSharedPtr<FJsonObject> A = Pair.Value->AsObject();
			if (!A.IsValid()) { continue; }
			FSequenceActor Actor;
			Actor.Id = Pair.Key;
			A->TryGetStringField(TEXT("character"), Actor.Character);
			A->TryGetStringField(TEXT("relativeTo"), Actor.RelativeTo);
			Actor.Offset = ReadVector(A, TEXT("offset"), Actor.Offset);
			Actor.Yaw = ReadFloat(A, TEXT("yaw"), Actor.Yaw);
			A->TryGetBoolField(TEXT("facePlayer"), Actor.bFacePlayer);
			Out.Actors.Add(MoveTemp(Actor));
		}
	}

	const TArray<TSharedPtr<FJsonValue>>* Steps = nullptr;
	if (!Root->TryGetArrayField(TEXT("steps"), Steps)) { return false; }
	for (const TSharedPtr<FJsonValue>& V : *Steps)
	{
		const TSharedPtr<FJsonObject> S = V->AsObject();
		if (!S.IsValid()) { continue; }
		FSequenceStep Step;
		const TSharedPtr<FJsonObject>* Obj = nullptr;
		const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
		FString Str;
		if (S->TryGetObjectField(TEXT("fade"), Obj))
		{
			Step.Type = ESequenceStepType::Fade;
			Step.FadeFrom = ReadFloat(*Obj, TEXT("from"), 1.0f);
			Step.FadeTo = ReadFloat(*Obj, TEXT("to"), 0.0f);
			Step.Seconds = ReadFloat(*Obj, TEXT("seconds"), 1.0f);
		}
		else if (S->TryGetObjectField(TEXT("camera"), Obj))
		{
			Step.Type = ESequenceStepType::Camera;
			(*Obj)->TryGetStringField(TEXT("anchor"), Step.Shot.Anchor);
			(*Obj)->TryGetStringField(TEXT("lookAt"), Step.Shot.LookAt);
			Step.Shot.Offset = ReadVector(*Obj, TEXT("offset"), Step.Shot.Offset);
			Step.Shot.LookOffset = ReadVector(*Obj, TEXT("lookOffset"), Step.Shot.LookOffset);
			Step.Shot.Fov = ReadFloat(*Obj, TEXT("fov"), Step.Shot.Fov);
			Step.Shot.Blend = ReadFloat(*Obj, TEXT("blend"), Step.Shot.Blend);
		}
		else if (S->TryGetObjectField(TEXT("say"), Obj))
		{
			Step.Type = ESequenceStepType::Say;
			(*Obj)->TryGetStringField(TEXT("who"), Step.Who);
			FString Raw;
			(*Obj)->TryGetStringField(TEXT("text"), Raw);
			Step.Text = ConversationFile::StripVoiceMarkup(Raw);
			(*Obj)->TryGetBoolField(TEXT("wait"), Step.bWaitForInput);
			Step.Seconds = ReadFloat(*Obj, TEXT("seconds"), 0.0f);   // optional override of the hold time
		}
		else if (S->TryGetArrayField(TEXT("choice"), Arr))
		{
			Step.Type = ESequenceStepType::Choice;
			for (const TSharedPtr<FJsonValue>& OV : *Arr)
			{
				const TSharedPtr<FJsonObject> O = OV->AsObject();
				if (!O.IsValid()) { continue; }
				FSequenceOption Opt;
				O->TryGetStringField(TEXT("text"), Opt.Text);
				O->TryGetStringField(TEXT("goto"), Opt.Goto);
				O->TryGetStringField(TEXT("set"), Opt.SetFlag);
				Step.Options.Add(MoveTemp(Opt));
			}
		}
		else if (S->TryGetObjectField(TEXT("look"), Obj))
		{
			Step.Type = ESequenceStepType::Look;
			(*Obj)->TryGetStringField(TEXT("anchor"), Step.Shot.Anchor);
			Step.Shot.Offset = ReadVector(*Obj, TEXT("offset"), FVector::ZeroVector);
			Step.Shot.Fov = ReadFloat(*Obj, TEXT("fov"), 70.0f);
			Step.LookPeriod = ReadFloat(*Obj, TEXT("period"), 4.0f);
			Step.Seconds = ReadFloat(*Obj, TEXT("seconds"), 20.0f);
			Step.Numbers.Add(TEXT("yawBias"), ReadFloat(*Obj, TEXT("yawBias"), 0.0f));
			Step.Numbers.Add(TEXT("turnSpeed"), ReadFloat(*Obj, TEXT("turnSpeed"), 1.1f));
			Step.Numbers.Add(TEXT("wander"), ReadFloat(*Obj, TEXT("wander"), 1.0f));
			const TArray<TSharedPtr<FJsonValue>>* Targets = nullptr;
			if ((*Obj)->TryGetArrayField(TEXT("targets"), Targets))
			{
				for (const TSharedPtr<FJsonValue>& T : *Targets) { Step.LookTargets.Add(T->AsString()); }
			}
		}
		else if (S->HasField(TEXT("waitBlinks")))
		{
			Step.Type = ESequenceStepType::WaitBlinks;
			Step.Seconds = ReadFloat(S, TEXT("waitBlinks"), 1.0f);   // count, kept in Seconds
		}
		else if (S->TryGetObjectField(TEXT("sound"), Obj))
		{
			Step.Type = ESequenceStepType::Sound;
			(*Obj)->TryGetStringField(TEXT("file"), Step.Text);
			Step.Numbers.Add(TEXT("volume"), ReadFloat(*Obj, TEXT("volume"), 1.0f));
			Step.Numbers.Add(TEXT("pitch"), ReadFloat(*Obj, TEXT("pitch"), 1.0f));
		}
		else if (S->TryGetObjectField(TEXT("move"), Obj))
		{
			Step.Type = ESequenceStepType::Move;
			Step.Position = ReadVector(*Obj, TEXT("to"), FVector::ZeroVector);
			Step.Seconds = ReadFloat(*Obj, TEXT("seconds"), 2.0f);
			Step.Numbers.Add(TEXT("sway"), ReadFloat(*Obj, TEXT("sway"), 1.0f));
		}
		else if (S->TryGetObjectField(TEXT("ambient"), Obj))
		{
			// "ambient": {"volume": 0.0, "seconds": 2.5} -- fades the background bed (not one-shots like the shock)
			Step.Type = ESequenceStepType::Ambient;
			Step.Seconds = ReadFloat(*Obj, TEXT("seconds"), 2.0f);
			Step.Numbers.Add(TEXT("volume"), ReadFloat(*Obj, TEXT("volume"), 1.0f));
		}
		else if (S->TryGetObjectField(TEXT("turn"), Obj))
		{
			// "turn": {"who": "officer", "yaw": 115, "seconds": 1.4} -- the actor swings to that world yaw, blocking for the seconds
			Step.Type = ESequenceStepType::Turn;
			(*Obj)->TryGetStringField(TEXT("who"), Step.Who);
			Step.Seconds = ReadFloat(*Obj, TEXT("seconds"), 1.0f);
			Step.Numbers.Add(TEXT("yaw"), ReadFloat(*Obj, TEXT("yaw"), 0.0f));
		}
		else if (S->HasField(TEXT("appearance")))
		{
			Step.Type = ESequenceStepType::Appearance;
		}
		else if (S->HasField(TEXT("panel")))
		{
			Step.Type = ESequenceStepType::Panel;
			Step.Text = S->GetStringField(TEXT("panel"));
		}
		else if (S->TryGetObjectField(TEXT("remote"), Obj))
		{
			Step.Type = ESequenceStepType::Remote;
			(*Obj)->TryGetStringField(TEXT("who"), Step.Who);
			(*Obj)->TryGetBoolField(TEXT("off"), Step.bFlag);
		}
		else if (S->TryGetObjectField(TEXT("shock"), Obj))
		{
			Step.Type = ESequenceStepType::Shock;
			(*Obj)->TryGetStringField(TEXT("file"), Step.Text);
			Step.Seconds = ReadFloat(*Obj, TEXT("build"), 2.2f);
			Step.Numbers.Add(TEXT("volume"), ReadFloat(*Obj, TEXT("volume"), 1.0f));
			Step.Numbers.Add(TEXT("pitch"), ReadFloat(*Obj, TEXT("pitch"), 1.0f));
			Step.Numbers.Add(TEXT("spasm"), ReadFloat(*Obj, TEXT("spasm"), 0.8f));
		}
		else if (S->TryGetObjectField(TEXT("blink"), Obj))
		{
			Step.Type = ESequenceStepType::Blink;
			(*Obj)->TryGetBoolField(TEXT("off"), Step.bFlag);
			for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : (*Obj)->Values)
			{
				if (Pair.Value->Type == EJson::Number) { Step.Numbers.Add(Pair.Key, static_cast<float>(Pair.Value->AsNumber())); }
			}
		}
		else if (S->TryGetObjectField(TEXT("place"), Obj))
		{
			Step.Type = ESequenceStepType::Place;
			(*Obj)->TryGetStringField(TEXT("who"), Step.Who);
			Step.Position = ReadVector(*Obj, TEXT("at"), FVector::ZeroVector);
			Step.Numbers.Add(TEXT("yaw"), ReadFloat(*Obj, TEXT("yaw"), 0.0f));
			(*Obj)->TryGetBoolField(TEXT("hidden"), Step.bFlag);
		}
		else if (S->TryGetStringField(TEXT("label"), Str))
		{
			Step.Type = ESequenceStepType::Label;
			Step.Label = Str;
		}
		else if (S->TryGetStringField(TEXT("goto"), Str))
		{
			Step.Type = ESequenceStepType::Goto;
			Step.Label = Str;
			S->TryGetStringField(TEXT("if"), Step.Condition);
		}
		else if (S->HasField(TEXT("wait")))
		{
			Step.Type = ESequenceStepType::Wait;
			Step.Seconds = ReadFloat(S, TEXT("wait"), 1.0f);
		}
		else if (S->TryGetObjectField(TEXT("release"), Obj))
		{
			Step.Type = ESequenceStepType::Release;
			Step.Position = ReadVector(*Obj, TEXT("goal"), FVector::ZeroVector);
			Step.Numbers.Add(TEXT("radius"), ReadFloat(*Obj, TEXT("radius"), 150.0f));
			Step.Numbers.Add(TEXT("speed"), ReadFloat(*Obj, TEXT("speed"), 95.0f));
		}
		else if (S->HasField(TEXT("end")))
		{
			Step.Type = ESequenceStepType::End;
			Step.Seconds = S->TryGetObjectField(TEXT("end"), Obj) ? ReadFloat(*Obj, TEXT("fade"), 0.5f) : 0.5f;
		}
		else
		{
			continue;
		}
		Out.Steps.Add(MoveTemp(Step));
	}
	return Out.Steps.Num() > 0;
}
