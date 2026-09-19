#include "Weapons/WeaponCatalog.h"
#include "Items/ItemInstance.h"
#include "Policies/PrettyJsonPrintPolicy.h"
#include "Serialization/JsonWriter.h"
#include "Misc/FileHelper.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Dom/JsonObject.h"
#include "Animation/AnimSequence.h"

namespace
{
	TMap<FString, WeaponCatalog::FWeapon> GByName;
	struct FStance
	{
		TArray<FName> Roots;
		FString Fallback;
		bool bTwoHanded = false;
		// "carry": where the grip sits in the eye's frame (x forward, y right, z up, cm) for each
		// posture. A stance without one uses the character's defaults, which were tuned on rifles.
		bool bHasCarry = false;
		FVector CarryShouldered, CarryHipFire, CarryLowReady, CarryLowReadyFirstPerson;
		float ElbowDown = 0.0f;   // "elbow_down"
		TMap<FString, FString> Clips;   // "clips": a clip name -> an asset path, looked up before the Lyra folder (a stance built from another pack)
		FVector GripNudge = FVector::ZeroVector;   // "grip_nudge": moves where the trigger hand closes, in the weapon's own space (x forward, z up, cm), for every weapon on the stance
		FVector CarryAds = FVector::ZeroVector;   // "ads": where the rear sight sits down the sights (x = how far out from the eye); zero = the character default
	};
	TMap<FString, FStance> GStances;
	TArray<WeaponCatalog::FFingerPreset> GFingerPresets;
	TMap<FString, WeaponCatalog::FOptic> GOptics;
	bool GWeaponCatalogLoaded = false;

	FString CatalogueFile()
	{
		return FPaths::Combine(FPaths::ProjectDir(), TEXT("UI"), TEXT("Weapons.json"));
	}

	// A list of Count numbers, zero-filled where the file has fewer; empty when the field is absent.
	TArray<float> ReadFloats(const TSharedPtr<FJsonObject>& Entry, const TCHAR* Field, int32 Count)
	{
		TArray<float> Out;
		const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
		if (!Entry->TryGetArrayField(Field, Arr) || !Arr) { return Out; }
		Out.SetNumZeroed(Count);
		for (int32 i = 0; i < Count && i < Arr->Num(); ++i) { Out[i] = (float)(*Arr)[i]->AsNumber(); }
		return Out;
	}

	// One number for all three carries, or a list of three (low ready, shouldered, sights).
	void ReadTriple(const TSharedPtr<FJsonObject>& Entry, const TCHAR* Field, float (&Out)[3])
	{
		double N = 0.0;
		if (Entry->TryGetNumberField(Field, N)) { Out[0] = Out[1] = Out[2] = (float)N; return; }
		const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
		if (!Entry->TryGetArrayField(Field, Arr) || !Arr) { return; }
		for (int32 i = 0; i < 3 && i < Arr->Num(); ++i) { Out[i] = (float)(*Arr)[i]->AsNumber(); }
	}

	FVector ReadVector(const TSharedPtr<FJsonObject>& Entry, const TCHAR* Field)
	{
		const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
		if (!Entry->TryGetArrayField(Field, Arr) || !Arr || Arr->Num() < 3) { return FVector::ZeroVector; }
		return FVector((*Arr)[0]->AsNumber(), (*Arr)[1]->AsNumber(), (*Arr)[2]->AsNumber());
	}

	void LoadWeaponCatalogueIfNeeded()
	{
		if (GWeaponCatalogLoaded) { return; }
		GWeaponCatalogLoaded = true;
		GByName.Reset();

		FString Text;
		if (!FFileHelper::LoadFileToString(Text, *CatalogueFile()))
		{
			UE_LOG(LogTemp, Warning, TEXT("WeaponCatalog: could not read %s"), *CatalogueFile());
			return;
		}
		TSharedPtr<FJsonObject> Root;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Text);
		if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
		{
			UE_LOG(LogTemp, Warning, TEXT("WeaponCatalog: %s is not valid JSON"), *CatalogueFile());
			return;
		}
		// Stances first: a weapon's stance is only meaningful next to the folder metadata.
		GStances.Reset();
		GFingerPresets.Reset();
		const TSharedPtr<FJsonObject>* Presets = nullptr;
		if (Root->TryGetObjectField(TEXT("finger_presets"), Presets) && Presets)
		{
			for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : (*Presets)->Values)
			{
				if (Pair.Key.StartsWith(TEXT("_"))) { continue; }   // a documentation key, not an entry
				const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
				if (!Pair.Value.IsValid() || !Pair.Value->TryGetArray(Arr) || !Arr) { continue; }
				WeaponCatalog::FFingerPreset P; P.Name = Pair.Key; P.Values.SetNumZeroed(5);
				for (int32 i = 0; i < 5 && i < Arr->Num(); ++i) { P.Values[i] = (float)(*Arr)[i]->AsNumber(); }
				GFingerPresets.Add(MoveTemp(P));
			}
			GFingerPresets.Sort([](const WeaponCatalog::FFingerPreset& A, const WeaponCatalog::FFingerPreset& B) { return A.Name < B.Name; });
		}
		const TSharedPtr<FJsonObject>* Stances = nullptr;
		if (Root->TryGetObjectField(TEXT("stances"), Stances) && Stances)
		{
			for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : (*Stances)->Values)
			{
				if (Pair.Key.StartsWith(TEXT("_"))) { continue; }   // a documentation key, not an entry
				const TSharedPtr<FJsonObject> Obj = Pair.Value->AsObject();
				if (!Obj.IsValid()) { continue; }
				FStance S;
				Obj->TryGetStringField(TEXT("fallback"), S.Fallback);
				Obj->TryGetBoolField(TEXT("two_handed"), S.bTwoHanded);
				double Eb = 0.0; if (Obj->TryGetNumberField(TEXT("elbow_down"), Eb)) { S.ElbowDown = (float)Eb; }
				S.GripNudge = ReadVector(Obj, TEXT("grip_nudge"));
				const TSharedPtr<FJsonObject>* ClipsObj = nullptr;
				if (Obj->TryGetObjectField(TEXT("clips"), ClipsObj) && ClipsObj) { for (const auto& CP : (*ClipsObj)->Values) { S.Clips.Add(FString(CP.Key.ToView()), CP.Value->AsString()); } }
				const TSharedPtr<FJsonObject>* Carry = nullptr;
				if (Obj->TryGetObjectField(TEXT("carry"), Carry) && Carry)
				{
					S.bHasCarry = true;
					S.CarryShouldered = ReadVector(*Carry, TEXT("shouldered"));
					S.CarryHipFire = ReadVector(*Carry, TEXT("hip"));
					S.CarryLowReady = ReadVector(*Carry, TEXT("low_ready"));
					S.CarryLowReadyFirstPerson = ReadVector(*Carry, TEXT("low_ready_first_person"));
					S.CarryAds = ReadVector(*Carry, TEXT("ads"));
				}
				const TArray<TSharedPtr<FJsonValue>>* Roots = nullptr;
				if (Obj->TryGetArrayField(TEXT("roots"), Roots) && Roots)
				{
					for (const TSharedPtr<FJsonValue>& R : *Roots) { S.Roots.Add(FName(*R->AsString())); }
				}
				GStances.Add(Pair.Key, MoveTemp(S));
			}
		}

		GOptics.Reset();
		const TSharedPtr<FJsonObject>* Optics = nullptr;
		if (Root->TryGetObjectField(TEXT("optics"), Optics) && Optics)
		{
			for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : (*Optics)->Values)
			{
				if (Pair.Key.StartsWith(TEXT("_"))) { continue; }   // a documentation key, not an entry
				const TSharedPtr<FJsonObject> Obj = Pair.Value->AsObject();
				if (!Obj.IsValid()) { continue; }
				WeaponCatalog::FOptic O;
				using EOpticKind = WeaponCatalog::EOpticKind;   // this file sits outside the namespace
				Obj->TryGetStringField(TEXT("mesh"), O.MeshPath);
				Obj->TryGetStringField(TEXT("name"), O.Name);
				O.Eye = ReadVector(Obj, TEXT("eye"));
				O.Mount = ReadVector(Obj, TEXT("mount"));
				O.Offset = ReadVector(Obj, TEXT("offset"));
				{ double Z = 0.0; if (Obj->TryGetNumberField(TEXT("zoom"), Z) && Z > 0.01) { O.Zoom = (float)Z; } }
				{ bool B = false; if (Obj->TryGetBoolField(TEXT("smart"), B)) { O.bSmart = B; } }
				Obj->TryGetStringField(TEXT("reticle"), O.Reticle);
				{
					const TArray<TSharedPtr<FJsonValue>>* Levels = nullptr;
					if (Obj->TryGetArrayField(TEXT("zoom_levels"), Levels) && Levels)
					{
						for (const TSharedPtr<FJsonValue>& V : *Levels)
						{
							const float Z = (float)V->AsNumber();
							if (Z > 0.01f) { O.ZoomLevels.Add(Z); }
						}
						if (O.ZoomLevels.Num() > 0) { O.Zoom = O.ZoomLevels[0]; }
					}
					// THE KIND IS THE SOURCE OF TRUTH; the rest is only how an entry without one is
					// read. "scope" is black but for a circle, "zoomed" narrows the view and leaves
					// it otherwise alone, "red_dot" is neither. Older entries carry only "overlay"
					// and "zoom": an overlay of true was always a scope, and without one the
					// magnification is what separated a reflex sight from a magnified one.
					FString K;
					if (Obj->TryGetStringField(TEXT("kind"), K))
					{
						K = K.ToLower().Replace(TEXT(" "), TEXT("")).Replace(TEXT("-"), TEXT("_"));
						O.Kind = (K == TEXT("scope")) ? EOpticKind::Scope
							: (K == TEXT("zoomed") || K == TEXT("zoom")) ? EOpticKind::Zoomed
							: EOpticKind::RedDot;
					}
					else
					{
						bool B = false;
						const bool bWasOverlay = Obj->TryGetBoolField(TEXT("overlay"), B) ? B : (O.Zoom > 1.01f);
						O.Kind = bWasOverlay ? EOpticKind::Scope
							: (O.Zoom > 1.01f) ? EOpticKind::Zoomed : EOpticKind::RedDot;
					}
					// A magnified kind with no magnification listed is a contradiction -- both
					// "zoomed" and "scope" mean "the view narrows" -- so it reads as a red dot
					// rather than claiming a zoom of 1 and then behaving like one anyway.
					if (O.Kind != EOpticKind::RedDot && O.Zoom <= 1.01f) { O.Kind = EOpticKind::RedDot; }
					O.bOverlay = (O.Kind == EOpticKind::Scope);
				}
				{
					const FVector C = ReadVector(Obj, TEXT("reticle_colour"));
					if (!C.IsNearlyZero()) { O.ReticleColour = FLinearColor(C.X, C.Y, C.Z, 1.0f); }
				}
				const FVector R = ReadVector(Obj, TEXT("rot"));   // stored (pitch, yaw, roll), like every other rotation in this file
				O.Rot = FRotator(R.X, R.Y, R.Z);
				Obj->TryGetStringField(TEXT("skin"), O.Skin);
				Obj->TryGetStringField(TEXT("make"), O.Make);
				Obj->TryGetStringField(TEXT("model"), O.Model);
				GOptics.Add(Pair.Key, MoveTemp(O));
			}
		}

		const TSharedPtr<FJsonObject>* Weapons = nullptr;
		if (!Root->TryGetObjectField(TEXT("weapons"), Weapons) || !Weapons) { return; }

		for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : (*Weapons)->Values)
		{
			if (Pair.Key.StartsWith(TEXT("_"))) { continue; }   // a documentation key, not an entry
			const TSharedPtr<FJsonObject> Entry = Pair.Value->AsObject();
			if (!Entry.IsValid()) { continue; }
			WeaponCatalog::FWeapon W;
			W.Name = Entry->GetStringField(TEXT("name"));
			W.Key = Pair.Key;
			Entry->TryGetStringField(TEXT("skin"), W.Skin);
			double Moa = 0.0; if (Entry->TryGetNumberField(TEXT("moa"), Moa)) { W.MechanicalMoa = (float)FMath::Max(0.0, Moa); }
			Entry->TryGetStringField(TEXT("make"), W.Make);
			Entry->TryGetStringField(TEXT("model"), W.Model);
			W.Kind = Entry->GetStringField(TEXT("kind"));
			Entry->TryGetStringField(TEXT("sound"), W.Sound);
			Entry->TryGetStringField(TEXT("mesh"), W.MeshPath);
			Entry->TryGetStringField(TEXT("body_mesh"), W.BodyMeshPath);
			{ double Sc = 0.0; if (Entry->TryGetNumberField(TEXT("scale"), Sc) && Sc > 0.01) { W.Scale = (float)Sc; } }
			Entry->TryGetStringField(TEXT("pack"), W.Pack);
			Entry->TryGetStringField(TEXT("stance"), W.Stance);
			Entry->TryGetStringField(TEXT("space"), W.Space);
			Entry->TryGetBoolField(TEXT("ranged"), W.bRanged);
			{
				const TArray<TSharedPtr<FJsonValue>>* Modes = nullptr;
				if (Entry->TryGetArrayField(TEXT("fire_modes"), Modes) && Modes)
				{
					for (const TSharedPtr<FJsonValue>& V : *Modes)
					{
						WeaponCatalog::EFireMode Mode;
						if (WeaponCatalog::ParseFireMode(V->AsString(), Mode)) { W.FireModes.AddUnique(Mode); }
						else if (!V->AsString().TrimStartAndEnd().IsEmpty()) { UE_LOG(LogTemp, Warning, TEXT("Weapons.json %s: unknown fire mode '%s' (semi, burst, auto, laser)"), *W.Name, *V->AsString()); }
					}
				}
				double Rate = 0.0; if (Entry->TryGetNumberField(TEXT("fire_rate"), Rate)) { W.FireRate = (float)Rate; }
				FString AmmoText; if (Entry->TryGetStringField(TEXT("ammo_kind"), AmmoText) && !WeaponCatalog::ParseAmmoKind(AmmoText, W.Ammo) && !AmmoText.TrimStartAndEnd().IsEmpty()) { UE_LOG(LogTemp, Warning, TEXT("Weapons.json %s: unknown ammo kind '%s'"), *W.Name, *AmmoText); }
				double Rec = 0.0; if (Entry->TryGetNumberField(TEXT("recoil"), Rec)) { W.Recoil = (float)Rec; }
				double Mass = 0.0; if (Entry->TryGetNumberField(TEXT("mass_kg"), Mass) && Mass > 0.0) { W.MassKg = (float)Mass; }
				double Dmg = 0.0; if (Entry->TryGetNumberField(TEXT("damage"), Dmg) && Dmg > 0.0) { W.Damage = (float)Dmg; }
				double Mag = 0.0; if (Entry->TryGetNumberField(TEXT("magazine"), Mag) && Mag > 0.0) { W.Magazine = (float)Mag; }
				double HandsN = 0.0; if (Entry->TryGetNumberField(TEXT("hands"), HandsN)) { W.Hands = FMath::Clamp((int32)HandsN, 1, 2); }
				FString Set; if (Entry->TryGetStringField(TEXT("attack_set"), Set) && !Set.IsEmpty()) { W.AttackSet = Set.ToLower(); }
				Entry->TryGetBoolField(TEXT("blunt"), W.bBlunt);
			}
			Entry->TryGetStringField(TEXT("icon"), W.Icon);
			W.Muzzle = ReadVector(Entry, TEXT("muzzle"));
			W.Grip = ReadVector(Entry, TEXT("grip"));
			W.Eject = ReadVector(Entry, TEXT("eject"));
			{ const FVector HR = ReadVector(Entry, TEXT("hand_rot")); W.HandRot = FRotator(HR.X, HR.Y, HR.Z); }   // [pitch, yaw, roll]
			{ const FVector HR = ReadVector(Entry, TEXT("fore_hand_rot")); W.ForeHandRot = FRotator(HR.X, HR.Y, HR.Z); }
			W.FingersR = ReadFloats(Entry, TEXT("fingers_r"), 5); W.FingersL = ReadFloats(Entry, TEXT("fingers_l"), 5);
			{ double N = 0.0; if (Entry->TryGetNumberField(TEXT("hunch"), N)) { W.Hunch = (float)N; } }
			ReadTriple(Entry, TEXT("pull"), W.PullCm);
			ReadTriple(Entry, TEXT("lateral"), W.LateralCm);
			ReadTriple(Entry, TEXT("elbow_main"), W.ElbowMain);
			ReadTriple(Entry, TEXT("elbow_support"), W.ElbowSupport);
			{ bool B = false; if (Entry->TryGetBoolField(TEXT("hidden"), B)) { W.bHidden = B; } }
			ReadTriple(Entry, TEXT("elbow_main_aim"), W.ElbowMainAim);
			ReadTriple(Entry, TEXT("elbow_support_aim"), W.ElbowSupportAim);
			{
				const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
				if (Entry->TryGetArrayField(TEXT("low_ready"), Arr) && Arr && Arr->Num() >= 2)
				{
					W.LowReadyPitch = (float)(*Arr)[0]->AsNumber(); W.LowReadyYaw = (float)(*Arr)[1]->AsNumber();
				}
				// The older single pair of elbows -- one per arm, the same in every carry -- still reads,
				// BUT ONLY WHERE THE PER-CARRY FIELD IS ABSENT. This block used to run unconditionally
				// and after elbow_main had been read, so it flattened all three carries back to the old
				// single value: every save of elbow_main was thrown away on the very next read of the
				// file, and the tuning page showed the old number again the moment it reloaded. From
				// the outside that looks exactly like saving being broken, because the number does go
				// to disk -- it is simply overwritten on the way back in.
				if (Entry->TryGetArrayField(TEXT("elbow"), Arr) && Arr && Arr->Num() >= 2)
				{
					const bool bHasMain = Entry->HasField(TEXT("elbow_main"));
					const bool bHasSupport = Entry->HasField(TEXT("elbow_support"));
					for (int32 c = 0; c < 3; ++c)
					{
						if (!bHasMain) { W.ElbowMain[c] = (float)(*Arr)[0]->AsNumber(); }
						if (!bHasSupport) { W.ElbowSupport[c] = (float)(*Arr)[1]->AsNumber(); }
					}
				}
			}
			// The same trap, and the same guard: shoulder_side is what "lateral" was called when it was
			// a single number, and reading it over a weapon that has the per-carry field would discard
			// the middle carry every time the file was read.
			if (!Entry->HasField(TEXT("lateral")))
			{
				double N = 0.0; if (Entry->TryGetNumberField(TEXT("shoulder_side"), N)) { W.LateralCm[1] = (float)N; }
			}
			{ double N = 0.0; if (Entry->TryGetNumberField(TEXT("lean"), N)) { W.LeanDeg = (float)N; } }
			W.Shoulder = ReadVector(Entry, TEXT("shoulder"));
			W.Attachments.Reset();
			const TArray<TSharedPtr<FJsonValue>>* AttachArr = nullptr;
			if (Entry->TryGetArrayField(TEXT("attachments"), AttachArr) && AttachArr)
			{
				for (const TSharedPtr<FJsonValue>& V : *AttachArr)
				{
					const TArray<TSharedPtr<FJsonValue>>* P = nullptr;
					if (V->TryGetArray(P) && P && P->Num() >= 3) { W.Attachments.Add(FVector((*P)[0]->AsNumber(), (*P)[1]->AsNumber(), (*P)[2]->AsNumber())); }
				}
			}
			const TArray<TSharedPtr<FJsonValue>>* SightArr = nullptr;
			W.bHasSight = Entry->TryGetArrayField(TEXT("sight"), SightArr) && SightArr && SightArr->Num() >= 3;
			if (W.bHasSight) { W.Sight = ReadVector(Entry, TEXT("sight")); }
			double Pitch = 0.0;
			if (Entry->TryGetNumberField(TEXT("sight_pitch"), Pitch)) { W.SightPitch = Pitch; }
			Entry->TryGetBoolField(TEXT("bullpup"), W.bBullpup);
			Entry->TryGetStringField(TEXT("optic"), W.Optic);
			Entry->TryGetBoolField(TEXT("optic_fixed"), W.bOpticFixed);   // built in; cannot be swapped
			W.OpticMount = ReadVector(Entry, TEXT("optic_mount"));
			const TArray<TSharedPtr<FJsonValue>>* ForeArr = nullptr;
			W.bHasForeGrip = Entry->TryGetArrayField(TEXT("fore_grip"), ForeArr) && ForeArr && ForeArr->Num() >= 3;
			if (W.bHasForeGrip) { W.ForeGrip = ReadVector(Entry, TEXT("fore_grip")); }
			double ForePitch = 0.0;
			if (Entry->TryGetNumberField(TEXT("fore_grip_pitch"), ForePitch)) { W.ForeGripPitch = static_cast<float>(ForePitch); }
			bool bHip = false;
			if (Entry->TryGetBoolField(TEXT("hip_fire"), bHip)) { W.bHipFire = bHip; }
			if (W.Name.IsEmpty()) { continue; }
			// Keyed by display name, because that is what the inventory carries: items are plain
			// strings, and an equipped slot holds the same string the catalogue shows.
			GByName.Add(W.Name.ToLower(), MoveTemp(W));
		}
		UE_LOG(LogTemp, Log, TEXT("WeaponCatalog: %d weapons"), GByName.Num());
	}
}

const WeaponCatalog::FWeapon* WeaponCatalog::Find(const FString& ItemName)
{
	LoadWeaponCatalogueIfNeeded();
	if (ItemName.IsEmpty()) { return nullptr; }
	// A HANDLE IS A NAME HERE. Inventory entries for instanced items carry an id ("Spear 350#7");
	// stripping it at this one door is what lets every existing caller keep passing whatever it has.
	return GByName.Find(ItemHandle::NameOf(ItemName).ToLower());
}

TArray<FString> WeaponCatalog::OpticNames()
{
	LoadWeaponCatalogueIfNeeded();
	TArray<FString> Out; GOptics.GetKeys(Out); Out.Sort();
	return Out;
}

const WeaponCatalog::FOptic* WeaponCatalog::FindOptic(const FString& OpticName)
{
	LoadWeaponCatalogueIfNeeded();
	if (OpticName.IsEmpty()) { return nullptr; }
	return GOptics.Find(OpticName);
}

void WeaponCatalog::Reload() { GWeaponCatalogLoaded = false; }

static FString Composed(const FString& Make, const FString& Model, const FString& Fallback)
{
	if (!Make.IsEmpty() && !Model.IsEmpty()) { return Make + TEXT(" ") + Model; }
	if (!Model.IsEmpty()) { return Model; }
	return Fallback;
}

FString WeaponCatalog::DisplayName(const FString& ItemName)
{
	const FWeapon* W = Find(ItemName);
	return W ? Composed(W->Make, W->Model, W->Name) : ItemHandle::NameOf(ItemName);   // never show the instance id
}

FString WeaponCatalog::OpticDisplayName(const FString& OpticKey)
{
	const FOptic* O = FindOptic(OpticKey);
	return O ? Composed(O->Make, O->Model, O->Name.IsEmpty() ? OpticKey : O->Name) : OpticKey;
}

bool WeaponCatalog::WriteStringField(const FString& Key, const TCHAR* Field, const FString& Value)
{
	FString Json;
	if (!FFileHelper::LoadFileToString(Json, *CatalogueFile())) { return false; }
	TSharedPtr<FJsonObject> Root;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid()) { return false; }
	const TSharedPtr<FJsonObject>* Weapons = nullptr;
	if (!Root->TryGetObjectField(TEXT("weapons"), Weapons) || !Weapons) { return false; }
	const TSharedPtr<FJsonObject>* Entry = nullptr;
	if (!(*Weapons)->TryGetObjectField(Key, Entry) || !Entry) { return false; }
	(*Entry)->SetStringField(Field, Value);
	FString Out;
	TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> Writer = TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>::Create(&Out);
	if (!FJsonSerializer::Serialize(Root.ToSharedRef(), Writer)) { return false; }
	const bool bSaved = FFileHelper::SaveStringToFile(Out, *CatalogueFile(), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	if (bSaved) { Reload(); }
	return bSaved;
}

// Both optic writers differ only in what they put on the entry, so the load-edit-save is written
// once and the difference handed in.
static bool WriteOpticEntry(const FString& OpticKey, TFunctionRef<void(const TSharedPtr<FJsonObject>&)> Edit)
{
	if (OpticKey.IsEmpty()) { return false; }
	FString Json;
	if (!FFileHelper::LoadFileToString(Json, *CatalogueFile())) { return false; }
	TSharedPtr<FJsonObject> Root;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid()) { return false; }
	const TSharedPtr<FJsonObject>* Optics = nullptr;
	if (!Root->TryGetObjectField(TEXT("optics"), Optics) || !Optics) { return false; }
	const TSharedPtr<FJsonObject>* Entry = nullptr;
	if (!(*Optics)->TryGetObjectField(OpticKey, Entry) || !Entry) { return false; }
	Edit(*Entry);
	FString Out;
	TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> Writer = TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>::Create(&Out);
	if (!FJsonSerializer::Serialize(Root.ToSharedRef(), Writer)) { return false; }
	const bool bSaved = FFileHelper::SaveStringToFile(Out, *CatalogueFile(), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	if (bSaved) { WeaponCatalog::Reload(); }
	return bSaved;
}

bool WeaponCatalog::WriteOpticString(const FString& OpticKey, const TCHAR* Field, const FString& Value)
{
	return WriteOpticEntry(OpticKey, [&](const TSharedPtr<FJsonObject>& E) { E->SetStringField(Field, Value); });
}

bool WeaponCatalog::WriteOpticNumbers(const FString& OpticKey, const TCHAR* Field, const TArray<double>& Values)
{
	return WriteOpticEntry(OpticKey, [&](const TSharedPtr<FJsonObject>& E)
	{
		TArray<TSharedPtr<FJsonValue>> Arr;
		for (double V : Values) { Arr.Add(MakeShared<FJsonValueNumber>(FMath::RoundToDouble(V * 100.0) / 100.0)); }
		E->SetArrayField(Field, Arr);
	});
}

int32 WeaponCatalog::Num() { LoadWeaponCatalogueIfNeeded(); return GByName.Num(); }

TArray<FString> WeaponCatalog::TunableNames()
{
	LoadWeaponCatalogueIfNeeded();
	TArray<FString> Out;
	Out.Reserve(GByName.Num());
	for (const TPair<FString, FWeapon>& It : GByName)
	{
		// No mesh or no stance and there is nothing to hold, so nothing to tune.
		if (It.Value.MeshPath.IsEmpty() || It.Value.Stance.IsEmpty()) { continue; }
		if (It.Value.bHidden) { continue; }   // set aside in the Reference: not offered for choosing
		Out.Add(It.Key);
	}
	// Sorted on the DISPLAYED name, so stepping through them on the page goes in the order the
	// page shows rather than in whatever order a hash map happened to store them.
	Out.Sort([](const FString& A, const FString& B) { return DisplayName(A) < DisplayName(B); });
	return Out;
}

namespace
{
	// The names the catalogue and the Reference use, in enum order.
	const TCHAR* GFireModeNames[] = { TEXT("semi"), TEXT("burst"), TEXT("auto"), TEXT("laser") };
	const TCHAR* GAmmoKindNames[] = { TEXT("none"), TEXT("light"), TEXT("medium"), TEXT("heavy"), TEXT("shell"), TEXT("cell"), TEXT("rocket") };
	template <int32 N> bool ParseName(const TCHAR* const (&Names)[N], const FString& Text, int32& Out)
	{
		const FString T = Text.ToLower().TrimStartAndEnd();
		for (int32 I = 0; I < N; ++I) { if (T == Names[I]) { Out = I; return true; } }
		return false;
	}
	template <int32 N> const TArray<FString>& AllNames(const TCHAR* const (&Names)[N], TArray<FString>& Cache)
	{
		if (Cache.Num() == 0) { for (const TCHAR* Name : Names) { Cache.Add(Name); } }
		return Cache;
	}
}

const TCHAR* WeaponCatalog::FireModeName(EFireMode Mode) { const int32 I = (int32)Mode; return I >= 0 && I < UE_ARRAY_COUNT(GFireModeNames) ? GFireModeNames[I] : GFireModeNames[0]; }
bool WeaponCatalog::ParseFireMode(const FString& Text, EFireMode& Out) { int32 I; if (!ParseName(GFireModeNames, Text, I)) { return false; } Out = (EFireMode)I; return true; }
const TArray<FString>& WeaponCatalog::FireModeNames() { static TArray<FString> Cache; return AllNames(GFireModeNames, Cache); }
const TCHAR* WeaponCatalog::AmmoKindName(EAmmoKind Kind) { const int32 I = (int32)Kind; return I >= 0 && I < UE_ARRAY_COUNT(GAmmoKindNames) ? GAmmoKindNames[I] : GAmmoKindNames[0]; }
bool WeaponCatalog::ParseAmmoKind(const FString& Text, EAmmoKind& Out) { int32 I; if (!ParseName(GAmmoKindNames, Text, I)) { return false; } Out = (EAmmoKind)I; return true; }
const TArray<FString>& WeaponCatalog::AmmoKindNames() { static TArray<FString> Cache; return AllNames(GAmmoKindNames, Cache); }

const TArray<FName>& WeaponCatalog::StanceRoots(const FString& Stance)
{
	LoadWeaponCatalogueIfNeeded();
	static const TArray<FName> None;
	const FStance* S = GStances.Find(Stance);
	return S ? S->Roots : None;
}

const TArray<WeaponCatalog::FFingerPreset>& WeaponCatalog::FingerPresets()
{
	LoadWeaponCatalogueIfNeeded();
	return GFingerPresets;
}

bool WeaponCatalog::StanceCarry(const FString& Stance, const TCHAR* Which, FVector& Out)
{
	LoadWeaponCatalogueIfNeeded();
	const FStance* S = GStances.Find(Stance);
	if (!S || !S->bHasCarry) { return false; }
	const FString W(Which);
	if (W == TEXT("shouldered")) { Out = S->CarryShouldered; }
	else if (W == TEXT("hip")) { Out = S->CarryHipFire; }
	else if (W == TEXT("low_ready")) { Out = S->CarryLowReady; }
	else if (W == TEXT("low_ready_first_person")) { Out = S->CarryLowReadyFirstPerson; }
	else if (W == TEXT("ads")) { if (S->CarryAds.IsNearlyZero()) { return false; } Out = S->CarryAds; }
	else { return false; }
	return true;
}

FVector WeaponCatalog::StanceGripNudge(const FString& Stance)
{
	LoadWeaponCatalogueIfNeeded();
	const FStance* S = GStances.Find(Stance);
	return S ? S->GripNudge : FVector::ZeroVector;
}

float WeaponCatalog::StanceElbowDown(const FString& Stance)
{
	LoadWeaponCatalogueIfNeeded();
	const FStance* S = GStances.Find(Stance);
	return S ? FMath::Clamp(S->ElbowDown, 0.0f, 1.0f) : 0.0f;
}

bool WeaponCatalog::StanceIsTwoHanded(const FString& Stance)
{
	LoadWeaponCatalogueIfNeeded();
	const FStance* S = GStances.Find(Stance);
	return S && S->bTwoHanded;
}

UAnimSequence* WeaponCatalog::StanceClip(const FString& Stance, const TCHAR* Clip, bool bFeminine)
{
	LoadWeaponCatalogueIfNeeded();
	// Walk the fallback chain -- Shotgun has no locomotion of its own and borrows Rifle's.
	// Capped rather than trusted, because a JSON typo could make the chain a loop.
	FString Current = Stance;
	for (int32 Hop = 0; Hop < 4 && !Current.IsEmpty(); ++Hop)
	{
		// The stance's own clip for this name, when it names one.
		if (const FStance* Own = GStances.Find(Current)) { if (const FString* Path = Own->Clips.Find(Clip)) { if (UAnimSequence* Found = LoadObject<UAnimSequence>(nullptr, **Path, nullptr, LOAD_NoWarn | LOAD_Quiet)) { return Found; } } }
		// Feminine first where a set ships both; MM_ is the one every set has.
		const TCHAR* Prefixes[2] = { bFeminine ? TEXT("MF") : TEXT("MM"), bFeminine ? TEXT("MM") : TEXT("MF") };
		for (const TCHAR* Prefix : Prefixes)
		{
			const FString Name = FString::Printf(TEXT("%s_%s_%s"), Prefix, *Current, Clip);
			const FString Path = FString::Printf(TEXT("/Game/Characters/Animations/Lyra/%s/%s.%s"), *Current, *Name, *Name);
			if (UAnimSequence* Found = LoadObject<UAnimSequence>(nullptr, *Path, nullptr, LOAD_NoWarn | LOAD_Quiet))
			{
				return Found;
			}
		}
		const FStance* S = GStances.Find(Current);
		Current = S ? S->Fallback : FString();
	}
	return nullptr;
}
