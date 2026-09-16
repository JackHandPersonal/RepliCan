#include "WeaponCatalog.h"
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
					for (const TSharedPtr<FJsonValue>& V : *Modes) { const FString M = V->AsString().ToLower().TrimStartAndEnd(); if (!M.IsEmpty()) { W.FireModes.Add(M); } }
				}
				double Rate = 0.0; if (Entry->TryGetNumberField(TEXT("fire_rate"), Rate)) { W.FireRate = (float)Rate; }
			}
			Entry->TryGetStringField(TEXT("icon"), W.Icon);
			W.Muzzle = ReadVector(Entry, TEXT("muzzle"));
			W.Grip = ReadVector(Entry, TEXT("grip"));
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

const WeaponCatalog::FOptic* WeaponCatalog::FindOptic(const FString& OpticName)
{
	LoadIfNeeded();
	if (OpticName.IsEmpty()) { return nullptr; }
	return GOptics.Find(OpticName);
}

void WeaponCatalog::Reload() { GWeaponCatalogLoaded = false; }

int32 WeaponCatalog::Num() { LoadIfNeeded(); return GByName.Num(); }

const TArray<FName>& WeaponCatalog::StanceRoots(const FString& Stance)
{
	LoadIfNeeded();
	static const TArray<FName> None;
	const FStance* S = GStances.Find(Stance);
	return S ? S->Roots : None;
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
