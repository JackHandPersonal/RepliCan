// What the game knows about an item beyond its name: the two catalogues (UI/Weapons.json,
// UI/Items.json) read into records keyed by name, with every field in ItemFields.h as text.
// The inventory carries plain strings; this is where a string becomes a thing with a mass, a
// material, a use and a description. Unknown items get a stock line.
#pragma once

#include "CoreMinimal.h"
#include "Items/ItemFields.h"
#include "Weapons/WeaponCatalog.h"

class FJsonObject;
class FJsonValue;

namespace ItemCatalog
{
	struct FRecord
	{
		FString Key;        // "Space/Wep_Pistol_01", "PolygonSciFiWorlds/PowerCell_02"
		FString Name;
		FString Category;   // weapons | armor | equipment | consumables | other
		FString Description;
		FString Icon, Mesh;
		TMap<FString, FString> Fields;   // ItemFields keys -> the value as text ("true", "3.8", "a, b")
		FString Get(const TCHAR* Field) const { const FString* V = Fields.Find(Field); return V ? *V : FString(); }
		double Number(const TCHAR* Field, double Default = 0.0) const { const FString V = Get(Field); return V.IsEmpty() ? Default : FCString::Atod(*V); }
		bool Flag(const TCHAR* Field) const { return Get(Field) == TEXT("true"); }
	};

	// Both files, re-read when either changes on disk (or when bForce).
	REPLICAN_API void Reload(bool bForce = false);
	REPLICAN_API const FRecord* FindRecord(const FString& Name);
	REPLICAN_API const FRecord* FindRecordByKey(const FString& Key);
	REPLICAN_API const TArray<FRecord>& Records();

	// A field's JSON value as the text the editors show, and back. Bool: "true"/"false".
	// Number: shortest exact form. List: "a, b, c". Measured: read-only text.
	REPLICAN_API FString FieldToString(const TSharedPtr<FJsonValue>& Value, const ItemFields::FField& Field);
	REPLICAN_API void StringToField(const TSharedPtr<FJsonObject>& Entry, const ItemFields::FField& Field, const FString& Text);
	// Every ItemFields key present on an entry, as text.
	REPLICAN_API void ReadFields(const TSharedPtr<FJsonObject>& Entry, TMap<FString, FString>& Out);

	// The weapon catalogue answers first for the kind: UI/Weapons.json carries a hundred weapons
	// and an item named after one of them equips as that kind.
	REPLICAN_API FString Kind(const FString& Name);
	// The info panel's body: the description and one line of the numbers that matter.
	REPLICAN_API FString Describe(const FString& Name);
	// The info panel's title line: NAME   [ KIND ], empty for no item.
	inline FString InfoTitle(const FString& Name) { return Name.IsEmpty() ? FString() : WeaponCatalog::DisplayName(Name).ToUpper() + TEXT("   [ ") + Kind(Name).ToUpper() + TEXT(" ]"); }   // a weapon reads MAKE MODEL
}
