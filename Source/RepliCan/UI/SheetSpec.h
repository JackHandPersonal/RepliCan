// The character sheet's layout, read from <Project>/UI/CharacterSheet.json every time the
// sheet opens, so sizes, captions, the gear slot table, the stat rows, the ASCII dressing
// and the mirror framing are text edits that show on the next Tab press, with the game
// still running. Missing keys keep their defaults below; a broken file logs and keeps the
// last good spec. Any caption or row that ends in RuleChar pads itself with RuleChar to the
// full width of its column (a "rule").
#pragma once

#include "CoreMinimal.h"

struct FSheetGearSlot
{
	FString Name;            // caption
	int32 Row = 0, Col = 0;  // cell on the gear grid
	bool bEnabled = true;    // held-back slots draw cross-hatched and refuse items
	FString Label;           // none | left | right | above | below: where the caption sits
	TArray<FString> Kinds;   // ItemCatalog kinds that equip here
};

// One captioned block in the sheet's left column -- ATTRIBUTES, DEFENCES, STATUSES, TALENTS.
// Rows are plain text with {Token} substitutions resolved when the sheet refreshes.
struct FSheetStatSection
{
	FString Caption;
	TArray<FString> Rows;
};

struct FSheetSpec
{
	// the panel's inset from the screen edges (left, top, right, bottom); shared by the container screen
	FMargin Panel = FMargin(200.0f, 124.0f, 200.0f, 72.0f);
	// The frame never shrinks below this (logical px): below it the frame keeps this size and
	// its far edges leave the screen, rather than the content squeezing.
	float PanelMinWidth = 1520.0f, PanelMinHeight = 884.0f;
	// columns
	float StatsWeight = 0.6f, MirrorWeight = 0.7f, RightWeight = 1.7f, ColumnGap = 30.0f;
	FString Divider = TEXT("|");   // vertical rule between the columns (empty = none)
	// fonts (point sizes before the face's own scaling)
	int32 RuleSize = 13, RowSize = 12, CaptionSize = 13, InfoNameSize = 15, InfoTextSize = 12, NameSize = 15, ArtSize = 12;
	// header rule around the unit's name, its end cap, and the footer rule's caps
	// A caption or stat row ending in any of RuleChars pads itself with that character.
	FString HeaderLeft = TEXT("+===[ "), HeaderRight = TEXT(" ]="), HeaderEnd = TEXT("+"), RuleChars = TEXT("-="), Hint;
	FString FooterLeft = TEXT("+="), FooterEnd = TEXT("=+");
	// bag
	int32 InventoryColumns = 10; float InventoryCell = 64.0f, InventoryGap = 6.0f;
	FString InventoryCaption = TEXT("==[ INVENTORY ]=");
	// gear
	FString GearCaption = TEXT("==[ EQUIPMENT ]="); float GearCell = 64.0f;
	FString QuickbarCaption = TEXT("--[ QUICKBAR ]-");   // ten squares, 1-0: the keys that draw or use what sits in them
	TArray<FSheetGearSlot> Slots;
	TArray<FIntVector> Spacers;   // (row, col, width px): empty cells that hold a column open
	FString InfoCaption = TEXT("==[ INFO ]=");
	// stats column: ASCII art lines, then one or more captioned blocks. A sheet that only wants
	// one block can still write caption/rows directly and it becomes the first section, which is
	// how the file read before sections existed.
	TArray<FString> StatsArt;
	FString StatsCaption = TEXT("==[ UNIT ]=");
	TArray<FString> StatRows;
	TArray<FSheetStatSection> StatSections;
	// mirror: caption above, rule below, framing (head socket relative), horizontal FOV on the portrait target
	FString MirrorCaption = TEXT("==[ CHARACTER ]="), MirrorFooter = TEXT("=");
	FVector BodyOffset = FVector(250.0f, 34.0f, -36.0f), BodyLook = FVector(0, 0, -58.0f); float BodyFov = 26.0f;
	FVector HeadOffset = FVector(84.0f, 8.0f, 16.0f), HeadLook = FVector(0, 0, 5.0f); float HeadFov = 26.0f;
	float FaceClickFraction = 0.3f;   // clicks in the top this-much of the mirror zoom to the head

	// THE COLUMNS, from the width of the three-column body as Slate laid it out -- never from the
	// viewport, whose size and DPI scale are not the panel's. The right column hugs its grids when
	// its weight is 0 (a width known from the cell sizes alone, before anything has measured);
	// the stats and mirror columns share what is left by weight. The Appearance page uses the
	// same mirror width, so the portrait is the same size on every tab.
	float RightColumnWidth() const;
	float MirrorWidthFor(float BodyWidth) const;
	static FString Path();
	// The current spec, re-read when the file's timestamp changed (or when bForce).
	static const FSheetSpec& Get(bool bForce = false);
	// Which gear slot an item's kind fills, or -1.
	int32 SlotForKind(const FString& Kind) const;
	static FSheetSpec Defaults();
};
