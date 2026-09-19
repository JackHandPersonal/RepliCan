#include "WeaponCatalog.h"
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
	TMap<FString, WeaponCatalog::FOptic> GOptics;
	bool GWeaponCatalogLoaded = false;

	FString CatalogueFile()
	{
		return FPaths::Combine(FPaths::ProjectDir(), TEXT("UI"), TEXT("Weapons.json"));
	}

	FVector ReadVector(const TSharedPtr<FJsonObject>& Entry, const TCHAR* Field)
	{
		const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
		if (!Entry->TryGetArrayField(Field, Arr) || !Arr || Arr->Num() < 3) { return FVector::ZeroVector; }
		return FVector((*Arr)[0]->AsNumber(), (*Arr)[1]->AsNumber(), (*Arr)[2]->AsNumber());
	}

	void LoadIfNeeded()
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
		const TSharedPtr<FJsonObject>* Stances = nullptr;
		if (Root->TryGetObjectField(TEXT("stances"), Stances) && Stances)
		{
			for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : (*Stances)->Values)
			{
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
				const TSharedPtr<FJsonObject> Obj = Pair.Value->AsObject();
				if (!Obj.IsValid()) { continue; }
				WeaponCatalog::FOptic O;
				Obj->TryGetStringField(TEXT("mesh"), O.MeshPath);
				Obj->TryGetStringField(TEXT("name"), O.Name);
				O.Eye = ReadVector(Obj, TEXT("eye"));
				O.Mount = ReadVector(Obj, TEXT("mount"));
				Obj->TryGetStringField(TEXT("make"), O.Make);
				Obj->TryGetStringField(TEXT("model"), O.Model);
				GOptics.Add(Pair.Key, MoveTemp(O));
			}
		}

		const TSharedPtr<FJsonObject>* Weapons = nullptr;
		if (!Root->TryGetObjectField(TEXT("weapons"), Weapons) || !Weapons) { return; }

		for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : (*Weapons)->Values)
		{
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
				double HandsN = 0.0; if (Entry->TryGetNumberField(TEXT("hands"), HandsN)) { W.Hands = FMath::Clamp((int32)HandsN, 1, 2); }
				FString Set; if (Entry->TryGetStringField(TEXT("attack_set"), Set) && !Set.IsEmpty()) { W.AttackSet = Set.ToLower(); }
				Entry->TryGetBoolField(TEXT("blunt"), W.bBlunt);
			}
			Entry->TryGetStringField(TEXT("icon"), W.Icon);
			W.Muzzle = ReadVector(Entry, TEXT("muzzle"));
			W.Grip = ReadVector(Entry, TEXT("grip"));
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
	LoadIfNeeded();
	if (ItemName.IsEmpty()) { return nullptr; }
	return GByName.Find(ItemName.ToLower());
}

TArray<FString> WeaponCatalog::OpticNames()
{
	LoadIfNeeded();
	TArray<FString> Out; GOptics.GetKeys(Out); Out.Sort();
	return Out;
}

const WeaponCatalog::FOptic* WeaponCatalog::FindOptic(const FString& OpticName)
{
	LoadIfNeeded();
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
	return W ? Composed(W->Make, W->Model, W->Name) : ItemName;
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

int32 WeaponCatalog::Num() { LoadIfNeeded(); return GByName.Num(); }

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
	LoadIfNeeded();
	static const TArray<FName> None;
	const FStance* S = GStances.Find(Stance);
	return S ? S->Roots : None;
}

bool WeaponCatalog::StanceCarry(const FString& Stance, const TCHAR* Which, FVector& Out)
{
	LoadIfNeeded();
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
	LoadIfNeeded();
	const FStance* S = GStances.Find(Stance);
	return S ? S->GripNudge : FVector::ZeroVector;
}

float WeaponCatalog::StanceElbowDown(const FString& Stance)
{
	LoadIfNeeded();
	const FStance* S = GStances.Find(Stance);
	return S ? FMath::Clamp(S->ElbowDown, 0.0f, 1.0f) : 0.0f;
}

bool WeaponCatalog::StanceIsTwoHanded(const FString& Stance)
{
	LoadIfNeeded();
	const FStance* S = GStances.Find(Stance);
	return S && S->bTwoHanded;
}

UAnimSequence* WeaponCatalog::StanceClip(const FString& Stance, const TCHAR* Clip, bool bFeminine)
{
	LoadIfNeeded();
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
