#include "SheetSpec.h"
#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

FString FSheetSpec::Path()
{
	return FPaths::Combine(FPaths::ProjectDir(), TEXT("UI"), TEXT("CharacterSheet.json"));
}

FSheetSpec FSheetSpec::Defaults()
{
	FSheetSpec S;
	auto Slot = [&](const TCHAR* Name, int32 Row, int32 Col, bool bEnabled, const TCHAR* Label, std::initializer_list<const TCHAR*> Kinds)
	{
		FSheetGearSlot G; G.Name = Name; G.Row = Row; G.Col = Col; G.bEnabled = bEnabled; G.Label = Label;
		for (const TCHAR* K : Kinds) { G.Kinds.Add(K); }
		S.Slots.Add(G);
	};
	// Equipment 2x2 at the left (rows 1-2), a spacer column, then the apparel 2x3: upper body parts
	// over their lower halves, captions above the top row and below the bottom row.
	Slot(TEXT("Slot 1"), 1, 0, true, TEXT("above"), { TEXT("Rifle"), TEXT("Primary") });
	Slot(TEXT("Slot 2"), 1, 1, true, TEXT("above"), { TEXT("Sidearm") });
	Slot(TEXT("Slot 3"), 2, 0, false, TEXT("below"), {});
	Slot(TEXT("Slot 4"), 2, 1, false, TEXT("below"), {});
	Slot(TEXT("Arms"), 1, 3, true, TEXT("above"), { TEXT("Sleeves"), TEXT("Arms") });
	Slot(TEXT("Head"), 1, 4, true, TEXT("above"), { TEXT("Helmet"), TEXT("Head") });
	Slot(TEXT("Legs"), 1, 5, true, TEXT("above"), { TEXT("Legwear"), TEXT("Legs") });
	Slot(TEXT("Hands"), 2, 3, true, TEXT("below"), { TEXT("Gloves"), TEXT("Hands") });
	Slot(TEXT("Chest"), 2, 4, true, TEXT("below"), { TEXT("Armor"), TEXT("Chest") });
	Slot(TEXT("Feet"), 2, 5, true, TEXT("below"), { TEXT("Boots"), TEXT("Feet") });
	S.Spacers.Add(FIntVector(1, 2, 36));
	S.StatRows = { TEXT("DESIGNATION  REPLICANT"), TEXT("CLASS        SEMI-ORGANIC"), TEXT("SERIES       7"), TEXT("STATUS       ACTIVE"), TEXT("MEMORY       PARTIAL"), TEXT(""),
	               TEXT("--[ VITALS ]-"), TEXT("PULSE        --"), TEXT("SYNC         --"), TEXT("CHARGE       --"), TEXT(""),
	               TEXT("--[ STANDING ]-"), TEXT("ACCOUNT      SEE LEDGER"), TEXT("INFRACTIONS  0") };
	return S;
}

namespace
{
	FVector ReadVec(const TSharedPtr<FJsonObject>& O, const TCHAR* Key, const FVector& Default)
	{
		const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
		if (O->TryGetArrayField(Key, Arr) && Arr && Arr->Num() >= 3) { return FVector((*Arr)[0]->AsNumber(), (*Arr)[1]->AsNumber(), (*Arr)[2]->AsNumber()); }
		return Default;
	}
	template <typename T> void Num(const TSharedPtr<FJsonObject>& O, const TCHAR* Key, T& Out) { double V; if (O->TryGetNumberField(Key, V)) { Out = static_cast<T>(V); } }
	void Str(const TSharedPtr<FJsonObject>& O, const TCHAR* Key, FString& Out) { FString V; if (O->TryGetStringField(Key, V)) { Out = V; } }
	void Lines(const TSharedPtr<FJsonObject>& O, const TCHAR* Key, TArray<FString>& Out)
	{
		const TArray<TSharedPtr<FJsonValue>>* Rows = nullptr;
		if (O->TryGetArrayField(Key, Rows) && Rows) { Out.Reset(); for (const TSharedPtr<FJsonValue>& R : *Rows) { Out.Add(R->AsString()); } }
	}

	bool Parse(const FString& Json, FSheetSpec& Out)
	{
		TSharedPtr<FJsonObject> Root;
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
		if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid()) { return false; }
		Out = FSheetSpec::Defaults();
		const TSharedPtr<FJsonObject>* O = nullptr;
		if (Root->TryGetObjectField(TEXT("panel"), O)) { Num(*O, TEXT("left"), Out.Panel.Left); Num(*O, TEXT("top"), Out.Panel.Top); Num(*O, TEXT("right"), Out.Panel.Right); Num(*O, TEXT("bottom"), Out.Panel.Bottom); Num(*O, TEXT("min_width"), Out.PanelMinWidth); Num(*O, TEXT("min_height"), Out.PanelMinHeight); }
		if (Root->TryGetObjectField(TEXT("columns"), O)) { Num(*O, TEXT("stats"), Out.StatsWeight); Num(*O, TEXT("mirror"), Out.MirrorWeight); Num(*O, TEXT("right"), Out.RightWeight); Num(*O, TEXT("gap"), Out.ColumnGap); Str(*O, TEXT("divider"), Out.Divider); }
		if (Root->TryGetObjectField(TEXT("font"), O)) { Num(*O, TEXT("rule"), Out.RuleSize); Num(*O, TEXT("row"), Out.RowSize); Num(*O, TEXT("caption"), Out.CaptionSize); Num(*O, TEXT("info_name"), Out.InfoNameSize); Num(*O, TEXT("info_text"), Out.InfoTextSize); Num(*O, TEXT("name"), Out.NameSize); Num(*O, TEXT("art"), Out.ArtSize); }
		if (Root->TryGetObjectField(TEXT("header"), O)) { Str(*O, TEXT("left"), Out.HeaderLeft); Str(*O, TEXT("right"), Out.HeaderRight); Str(*O, TEXT("end"), Out.HeaderEnd); Str(*O, TEXT("rule_chars"), Out.RuleChars); Str(*O, TEXT("hint"), Out.Hint); }
		if (Root->TryGetObjectField(TEXT("footer"), O)) { Str(*O, TEXT("left"), Out.FooterLeft); Str(*O, TEXT("end"), Out.FooterEnd); }
		if (Root->TryGetObjectField(TEXT("inventory"), O)) { Num(*O, TEXT("columns"), Out.InventoryColumns); Num(*O, TEXT("cell"), Out.InventoryCell); Num(*O, TEXT("gap"), Out.InventoryGap); Str(*O, TEXT("caption"), Out.InventoryCaption); }
		if (Root->TryGetObjectField(TEXT("gear"), O))
		{
			Str(*O, TEXT("caption"), Out.GearCaption); Num(*O, TEXT("cell"), Out.GearCell); Str(*O, TEXT("info_caption"), Out.InfoCaption);
			const TArray<TSharedPtr<FJsonValue>>* Sp = nullptr;
			if ((*O)->TryGetArrayField(TEXT("spacers"), Sp) && Sp)
			{
				Out.Spacers.Reset();
				for (const TSharedPtr<FJsonValue>& V : *Sp)
				{
					const TSharedPtr<FJsonObject>* SO = nullptr;
					if (!V->TryGetObject(SO)) { continue; }
					int32 R = 0, C = 0, W = 0; Num(*SO, TEXT("row"), R); Num(*SO, TEXT("col"), C); Num(*SO, TEXT("width"), W);
					Out.Spacers.Add(FIntVector(R, C, W));
				}
			}
			const TArray<TSharedPtr<FJsonValue>>* Slots = nullptr;
			if ((*O)->TryGetArrayField(TEXT("slots"), Slots) && Slots)
			{
				Out.Slots.Reset();
				for (const TSharedPtr<FJsonValue>& V : *Slots)
				{
					const TSharedPtr<FJsonObject>* SO = nullptr;
					if (!V->TryGetObject(SO)) { continue; }
					FSheetGearSlot G;
					Str(*SO, TEXT("name"), G.Name); Num(*SO, TEXT("row"), G.Row); Num(*SO, TEXT("col"), G.Col); Str(*SO, TEXT("label"), G.Label);
					bool bE = true; if ((*SO)->TryGetBoolField(TEXT("enabled"), bE)) { G.bEnabled = bE; }
					const TArray<TSharedPtr<FJsonValue>>* Kinds = nullptr;
					if ((*SO)->TryGetArrayField(TEXT("kinds"), Kinds) && Kinds) { for (const TSharedPtr<FJsonValue>& K : *Kinds) { G.Kinds.Add(K->AsString()); } }
					Out.Slots.Add(G);
				}
			}
		}
		if (Root->TryGetObjectField(TEXT("stats"), O))
		{
			Str(*O, TEXT("caption"), Out.StatsCaption);
			Lines(*O, TEXT("art"), Out.StatsArt);
			Lines(*O, TEXT("rows"), Out.StatRows);
			Out.StatSections.Reset();
			const TArray<TSharedPtr<FJsonValue>>* Sections = nullptr;
			if ((*O)->TryGetArrayField(TEXT("sections"), Sections) && Sections)
			{
				for (const TSharedPtr<FJsonValue>& Value : *Sections)
				{
					const TSharedPtr<FJsonObject> Obj = Value->AsObject();
					if (!Obj.IsValid()) { continue; }
					FSheetStatSection Section;
					Str(Obj, TEXT("caption"), Section.Caption);
					Lines(Obj, TEXT("rows"), Section.Rows);
					Out.StatSections.Add(MoveTemp(Section));
				}
			}
			// The old single-block form is just a sheet with one section.
			if (Out.StatSections.Num() == 0) { Out.StatSections.Add({ Out.StatsCaption, Out.StatRows }); }
		}
		if (Root->TryGetObjectField(TEXT("mirror"), O))
		{
			Str(*O, TEXT("caption"), Out.MirrorCaption); Str(*O, TEXT("footer"), Out.MirrorFooter);
			const TSharedPtr<FJsonObject>* V = nullptr;
			if ((*O)->TryGetObjectField(TEXT("body"), V)) { Out.BodyOffset = ReadVec(*V, TEXT("offset"), Out.BodyOffset); Out.BodyLook = ReadVec(*V, TEXT("look"), Out.BodyLook); Num(*V, TEXT("fov"), Out.BodyFov); }
			if ((*O)->TryGetObjectField(TEXT("head"), V)) { Out.HeadOffset = ReadVec(*V, TEXT("offset"), Out.HeadOffset); Out.HeadLook = ReadVec(*V, TEXT("look"), Out.HeadLook); Num(*V, TEXT("fov"), Out.HeadFov); }
			Num(*O, TEXT("face_click_fraction"), Out.FaceClickFraction);
		}
		return true;
	}
}

const FSheetSpec& FSheetSpec::Get(bool bForce)
{
	static FSheetSpec Current = Defaults();
	static FDateTime Stamp = FDateTime::MinValue();
	static bool bEverLoaded = false;
	const FString File = Path();
	const FDateTime Now = IFileManager::Get().GetTimeStamp(*File);
	if (bForce || !bEverLoaded || Now != Stamp)
	{
		bEverLoaded = true; Stamp = Now;
		FString Json;
		if (FFileHelper::LoadFileToString(Json, *File))
		{
			FSheetSpec Parsed;
			if (Parse(Json, Parsed)) { Current = Parsed; UE_LOG(LogTemp, Log, TEXT("SheetSpec: loaded %s (%d slots, %d stat rows)"), *File, Current.Slots.Num(), Current.StatRows.Num()); }
			else { UE_LOG(LogTemp, Warning, TEXT("SheetSpec: %s did not parse; keeping the last good spec"), *File); }
		}
	}
	return Current;
}

float FSheetSpec::RightColumnWidth() const
{
	// The bag: every column a cell padded by half a gap each side. The gear grid: the same for
	// each column that holds a slot, plus the spacers at their own widths (unpadded).
	const float Bag = InventoryColumns * (InventoryCell + InventoryGap);
	TSet<int32> SlotCols;
	for (const FSheetGearSlot& G : Slots) { SlotCols.Add(G.Col); }
	float Gear = SlotCols.Num() * (GearCell + InventoryGap);
	for (const FIntVector& Sp : Spacers) { if (!SlotCols.Contains(Sp.Y)) { Gear += Sp.Z; } }
	return FMath::Max(Bag, Gear);
}

float FSheetSpec::MirrorWidthFor(float BodyWidth) const
{
	// The gaps and the two divider glyph columns come off the top; a divider is its glyph and
	// half a gap, the same as the sheet lays it.
	const float Dividers = Divider.IsEmpty() ? 0.0f : 2.0f * (ColumnGap * 0.5f + 12.0f);
	const float Usable = FMath::Max(300.0f, BodyWidth - 2.0f * ColumnGap - Dividers);
	const float RightW = RightWeight > 0.0f
		? FMath::Floor(Usable * RightWeight / FMath::Max(0.01f, StatsWeight + MirrorWeight + RightWeight))
		: RightColumnWidth();
	const float Rest = FMath::Max(200.0f, Usable - RightW);
	return FMath::Floor(Rest * MirrorWeight / FMath::Max(0.01f, StatsWeight + MirrorWeight));
}

int32 FSheetSpec::SlotForKind(const FString& Kind) const
{
	for (int32 i = 0; i < Slots.Num(); ++i) { for (const FString& K : Slots[i].Kinds) { if (K.Equals(Kind, ESearchCase::IgnoreCase)) { return i; } } }
	return -1;
}
