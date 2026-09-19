#include "Items/ItemCatalog.h"
#include "Core/JsonDataFile.h"
#include "Items/ItemInstance.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
	TArray<ItemCatalog::FRecord> GRecords;
	TMap<FString, int32> GByName, GByKey;
	FDateTime GStampWeapons, GStampItems;
	bool GLoaded = false;

	FString WeaponsFile() { return FPaths::Combine(JsonData::DataDir(), TEXT("UI"), TEXT("Weapons.json")); }
	FString ItemsFile() { return FPaths::Combine(JsonData::DataDir(), TEXT("UI"), TEXT("Items.json")); }

	FString NumberText(double V)
	{
		// The shortest exact text: 30 not 30.0, 3.8 not 3.800000.
		if (FMath::IsNearlyEqual(V, FMath::RoundToDouble(V), 1e-9)) { return FString::Printf(TEXT("%lld"), static_cast<int64>(FMath::RoundToDouble(V))); }
		FString S = FString::Printf(TEXT("%.4f"), V);
		while (S.EndsWith(TEXT("0"))) { S.LeftChopInline(1); }
		if (S.EndsWith(TEXT("."))) { S.LeftChopInline(1); }
		return S;
	}

	void LoadFile(const FString& Path, const TCHAR* RootKey, const TCHAR* DefaultCategory)
	{
		TSharedPtr<FJsonObject> Root = JsonData::LoadObject(Path, TEXT("ItemCatalog"));
		if (!Root.IsValid()) { return; }
		const TSharedPtr<FJsonObject>* Entries = nullptr;
		if (!Root->TryGetObjectField(RootKey, Entries) || !Entries) { return; }
		for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : (*Entries)->Values)
		{
			if (Pair.Key.StartsWith(TEXT("_"))) { continue; }   // a documentation key, not an entry
			const TSharedPtr<FJsonObject>* O = nullptr;
			if (!Pair.Value->TryGetObject(O) || !O) { continue; }
			ItemCatalog::FRecord R; R.Key = Pair.Key; R.Category = DefaultCategory;
			(*O)->TryGetStringField(TEXT("name"), R.Name); (*O)->TryGetStringField(TEXT("description"), R.Description);
			(*O)->TryGetStringField(TEXT("icon"), R.Icon); (*O)->TryGetStringField(TEXT("mesh"), R.Mesh);
			if (FCString::Strcmp(DefaultCategory, TEXT("weapons")) != 0) { (*O)->TryGetStringField(TEXT("category"), R.Category); }
			ItemCatalog::ReadFields(*O, R.Fields);
			if (R.Name.IsEmpty()) { R.Name = Pair.Key; }
			const int32 At = GRecords.Add(MoveTemp(R));
			GByName.Add(GRecords[At].Name.ToLower(), At); GByKey.Add(GRecords[At].Key, At);
		}
	}
}

void ItemCatalog::Reload(bool bForce)
{
	const FDateTime W = IFileManager::Get().GetTimeStamp(*WeaponsFile());
	const FDateTime I = IFileManager::Get().GetTimeStamp(*ItemsFile());
	if (GLoaded && !bForce && W == GStampWeapons && I == GStampItems) { return; }
	GRecords.Reset(); GByName.Reset(); GByKey.Reset();
	LoadFile(WeaponsFile(), TEXT("weapons"), TEXT("weapons"));
	LoadFile(ItemsFile(), TEXT("items"), TEXT("other"));
	GStampWeapons = W; GStampItems = I; GLoaded = true;
	UE_LOG(LogTemp, Log, TEXT("ItemCatalog: %d records"), GRecords.Num());
}

const ItemCatalog::FRecord* ItemCatalog::FindRecord(const FString& Name)
{
	Reload();
	// As in WeaponCatalog::Find: an instance handle is looked up as the thing it is a copy of.
	const int32* At = GByName.Find(ItemHandle::NameOf(Name).ToLower());
	return At ? &GRecords[*At] : nullptr;
}

const ItemCatalog::FRecord* ItemCatalog::FindRecordByKey(const FString& Key)
{
	Reload();
	const int32* At = GByKey.Find(Key);
	return At ? &GRecords[*At] : nullptr;
}

const TArray<ItemCatalog::FRecord>& ItemCatalog::Records() { Reload(); return GRecords; }

FString ItemCatalog::FieldToString(const TSharedPtr<FJsonValue>& Value, const ItemFields::FField& Field)
{
	if (!Value.IsValid() || Value->IsNull()) { return FString(); }
	switch (Field.Type)
	{
	case ItemFields::EType::Bool: { bool B = false; return Value->TryGetBool(B) ? (B ? TEXT("true") : TEXT("false")) : Value->AsString(); }
	case ItemFields::EType::Number: { double D = 0.0; return Value->TryGetNumber(D) ? NumberText(D) : Value->AsString(); }
	case ItemFields::EType::Points:
	{
		// [[x, y, z], ...] -> "x, y, z; x, y, z"
		const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
		if (!Value->TryGetArray(Arr) || !Arr) { return Value->AsString(); }
		TArray<FString> Points;
		for (const TSharedPtr<FJsonValue>& P : *Arr)
		{
			const TArray<TSharedPtr<FJsonValue>>* XYZ = nullptr;
			if (!P->TryGetArray(XYZ) || !XYZ) { continue; }
			TArray<FString> Parts;
			for (const TSharedPtr<FJsonValue>& V : *XYZ) { double D; Parts.Add(V->TryGetNumber(D) ? NumberText(D) : V->AsString()); }
			Points.Add(FString::Join(Parts, TEXT(", ")));
		}
		return FString::Join(Points, TEXT("; "));
	}
	case ItemFields::EType::List:
	case ItemFields::EType::Measured:
	{
		const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
		if (Value->TryGetArray(Arr) && Arr)
		{
			TArray<FString> Parts;
			for (const TSharedPtr<FJsonValue>& V : *Arr) { double D; Parts.Add(V->TryGetNumber(D) ? NumberText(D) : V->AsString()); }
			return FString::Join(Parts, Field.Type == ItemFields::EType::List ? TEXT(", ") : TEXT(" "));
		}
		double D; return Value->TryGetNumber(D) ? NumberText(D) : Value->AsString();
	}
	default: return Value->AsString();
	}
}

void ItemCatalog::StringToField(const TSharedPtr<FJsonObject>& Entry, const ItemFields::FField& Field, const FString& Text)
{
	if (!Entry.IsValid()) { return; }
	const FString T = Text.TrimStartAndEnd();
	switch (Field.Type)
	{
	case ItemFields::EType::Measured: return;   // regenerated from the mesh, never typed
	case ItemFields::EType::Bool: Entry->SetBoolField(Field.Key, T.Equals(TEXT("true"), ESearchCase::IgnoreCase) || T == TEXT("1") || T.Equals(TEXT("yes"), ESearchCase::IgnoreCase)); return;
	case ItemFields::EType::Number: Entry->SetNumberField(Field.Key, T.IsEmpty() ? 0.0 : FCString::Atod(*T)); return;
	case ItemFields::EType::Points:
	{
		// "x, y, z; x, y, z" -> [[x, y, z], ...]; a triple short of three numbers is dropped
		TArray<FString> Points; T.ParseIntoArray(Points, TEXT(";"), true);
		TArray<TSharedPtr<FJsonValue>> Arr;
		for (const FString& P : Points)
		{
			TArray<FString> Parts; P.ParseIntoArray(Parts, TEXT(","), true);
			if (Parts.Num() < 3) { continue; }
			TArray<TSharedPtr<FJsonValue>> XYZ;
			for (int32 i = 0; i < 3; ++i) { XYZ.Add(MakeShared<FJsonValueNumber>(FCString::Atod(*Parts[i].TrimStartAndEnd()))); }
			Arr.Add(MakeShared<FJsonValueArray>(XYZ));
		}
		Entry->SetArrayField(Field.Key, Arr); return;
	}
	case ItemFields::EType::List:
	{
		TArray<FString> Parts; T.ParseIntoArray(Parts, TEXT(","), true);
		TArray<TSharedPtr<FJsonValue>> Arr;
		for (FString& P : Parts)
		{
			P.TrimStartAndEndInline();
			if (P.IsEmpty()) { continue; }
			if (P.IsNumeric()) { Arr.Add(MakeShared<FJsonValueNumber>(FCString::Atod(*P))); } else { Arr.Add(MakeShared<FJsonValueString>(P)); }
		}
		Entry->SetArrayField(Field.Key, Arr); return;
	}
	default: Entry->SetStringField(Field.Key, T); return;
	}
}

void ItemCatalog::ReadFields(const TSharedPtr<FJsonObject>& Entry, TMap<FString, FString>& Out)
{
	if (!Entry.IsValid()) { return; }
	for (const ItemFields::FField& F : ItemFields::Table)
	{
		const TSharedPtr<FJsonValue> V = Entry->TryGetField(F.Key);
		if (V.IsValid()) { Out.Add(F.Key, FieldToString(V, F)); }
	}
}

FString ItemCatalog::Kind(const FString& Name)
{
	if (const WeaponCatalog::FWeapon* W = WeaponCatalog::Find(Name)) { return W->Kind; }
	const FRecord* R = FindRecord(Name);
	if (R) { const FString K = R->Get(TEXT("kind")); return K.IsEmpty() ? R->Category : K; }
	return TEXT("Item");
}

FString ItemCatalog::Describe(const FString& Name)
{
	const FRecord* R = FindRecord(Name);
	if (!R) { return TEXT("No data on file."); }
	// The description, then the numbers a person deciding whether to carry it wants.
	TArray<FString> Bits;
	if (!R->Get(TEXT("mass_kg")).IsEmpty()) { Bits.Add(R->Get(TEXT("mass_kg")) + TEXT(" kg")); }
	if (!R->Get(TEXT("material")).IsEmpty()) { Bits.Add(R->Get(TEXT("material"))); }
	if (R->Category == TEXT("weapons"))
	{
		if (!R->Get(TEXT("damage")).IsEmpty()) { Bits.Add(TEXT("dmg ") + R->Get(TEXT("damage"))); }
		if (!R->Get(TEXT("magazine")).IsEmpty() && R->Get(TEXT("magazine")) != TEXT("0")) { Bits.Add(TEXT("mag ") + R->Get(TEXT("magazine"))); }
		if (!R->Get(TEXT("range_m")).IsEmpty()) { Bits.Add(R->Get(TEXT("range_m")) + TEXT(" m")); }
	}
	else
	{
		if (!R->Get(TEXT("use")).IsEmpty() && R->Get(TEXT("use")) != TEXT("none")) { Bits.Add(R->Get(TEXT("use"))); }
		if (!R->Get(TEXT("effects")).IsEmpty()) { Bits.Add(R->Get(TEXT("effects"))); }
		if (!R->Get(TEXT("stack_max")).IsEmpty() && R->Get(TEXT("stack_max")) != TEXT("1")) { Bits.Add(TEXT("stack ") + R->Get(TEXT("stack_max"))); }
	}
	if (!R->Get(TEXT("value")).IsEmpty()) { Bits.Add(R->Get(TEXT("value")) + TEXT(" cr")); }
	const FString Line = FString::Join(Bits, TEXT("  ·  "));
	if (R->Description.IsEmpty()) { return Line.IsEmpty() ? TEXT("No data on file.") : Line; }
	return Line.IsEmpty() ? R->Description : R->Description + TEXT("\n") + Line;
}
