// The Reference screen: a searchable, scrolling catalogue of the game's items, one card per
// entry with its icon square, friendly name, kind and pack, and a description. Clicking a
// card opens the detail column: a live render of the weapon in its own booth (drag to turn,
// FIRE to see it kick) with the catalogue's grip, sight, fore-grip, optic-mount and muzzle
// points drawn over it, and the name, description, sound and hip-fire flag as editable
// fields saved back to UI/Weapons.json. Weapons for now; the category row is where other item types will go.
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "ReferenceWidget.generated.h"

class ABasePlayerController;
class UHorizontalBox;
class UEditableTextBox;
class UMultiLineEditableTextBox;
class UImage;
class UScrollBox;
class USizeBox;
class UTextBlock;
class UVerticalBox;
namespace WeaponCatalog { struct FWeapon; }

USTRUCT()
struct FReferenceEntry
{
	GENERATED_BODY()
	FString Key;          // "Space/Wep_Pistol_01"
	FString Category;     // weapons | armor | equipment | consumables | other -- which tab, and which catalogue file
	FString Name;         // friendly name
	FString Kind;         // "Pistol", "Rifle", "Sword"...
	FString Pack;         // "Space", "Worlds", "CyberCity"
	FString Description;
	FString Icon;         // icon key: T_Icon_<Icon>
	FString Sound;        // the wav FIRE plays, from RawAudio
	FString Stance;       // Rifle, Pistol, Shotgun, Blade: the animation folder the body holds it with
	FString Mesh;         // the asset
	bool bHipFire = false;   // fired from the hip (special weapons): the carry logic reads this through the catalogue
	bool bWeaponsFile = false;   // lives in UI/Weapons.json (weapons, and shields shown as armour) and is saved back there
	TMap<FString, FString> Fields;   // every ItemFields key the entry carries, as text (see ItemCatalog::ReadFields)
};

UCLASS()
class REPLICAN_API UReferenceCardBinding : public UObject
{
	GENERATED_BODY()
public:
	TWeakObjectPtr<class UReferenceWidget> Widget;
	int32 Index = 0;
	UFUNCTION() void OnClicked();
};

// One per bool field on the form: the button's click lands here with the field's key.
UCLASS()
class REPLICAN_API UReferenceFieldBinding : public UObject
{
	GENERATED_BODY()
public:
	TWeakObjectPtr<class UReferenceWidget> Widget;
	FString Key;
	UFUNCTION() void OnToggle();
	UFUNCTION() void OnCycle();
};

UCLASS()
class REPLICAN_API UReferenceWidget : public UUserWidget
{
	GENERATED_BODY()
	friend class UReferenceFieldBinding;   // a bool field's button lands in ToggleBoolField
	friend class UReferenceCardBinding;   // a card click reads the double-click clock and hides

public:
	// Called when the [ X ] is pressed; the opener decides what to show next.
	FSimpleDelegate OnClose;
	void SetOwnerController(ABasePlayerController* In) { OwnerController = In; }
	// (Re)builds the layout from the sheet spec and re-reads the catalogue.
	void Rebuild();
	// Opens the detail column on an entry (a card click).
	void SelectEntry(int32 Index);
	// Jump to the card for a named entry (the sheet's INFO link): its tab, then the card open.
	bool OpenEntryByName(const FString& Name);

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	// The asset path scrolls when it is too long for its corner. Held here rather than worked out
	// each frame so it keeps its place across a repaint.
	UPROPERTY() TObjectPtr<class USizeBox> MeshMarquee;
	float MeshScroll = 0.0f;
	float MeshScrollWait = 0.0f;
	static constexpr float MeshScrollSpeed = 42.0f;        // pixels a second: slow enough to read
	static constexpr float MeshScrollHoldSeconds = 1.6f;   // still at each end, so both can be read
	virtual void NativeDestruct() override;
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

private:
	UFUNCTION() void OnCloseClicked();
	UFUNCTION() void OnSearchChanged(const FText& Text);
	UFUNCTION() void OnFire();
	UFUNCTION() void OnResetView();
	UFUNCTION() void OnGive();   // one of this weapon into the player's bag
	UFUNCTION() void OnSaveDetail();
	UFUNCTION() void OnCloseDetail();
	// The category tabs across the top: one catalogue of weapons, one of everything else.
	UFUNCTION() void OnTabWeapons();
	UFUNCTION() void OnTabOptics();
	UFUNCTION() void OnTabArmor();
	UFUNCTION() void OnTabEquipment();
	UFUNCTION() void OnTabConsumables();
	UFUNCTION() void OnTabOther();
	void SetCategory(const FString& Category);
	void ShowTabs();
	bool SaveItemEntry(const FReferenceEntry& E);
	FString CurrentCategory = TEXT("weapons");
	UPROPERTY() TArray<TObjectPtr<UTextBlock>> TabLabels;
	// Parts of the detail column that only mean something for a weapon.
	UPROPERTY() TObjectPtr<UWidget> FireWidget;
	UPROPERTY() TObjectPtr<UWidget> GiveWidget;   // the GIVE column beside the render; weapons only
	UPROPERTY() TObjectPtr<UWidget> ResetPointsWidget;   // only where there are points to reset (weapons, optics)
	UPROPERTY() TObjectPtr<UWidget> HipWidget;
	UPROPERTY() TObjectPtr<UWidget> StanceWidget;
	UPROPERTY() TObjectPtr<UWidget> OpticWidget;
	UPROPERTY() TObjectPtr<UWidget> MetaWidget;
	UFUNCTION() void OnHipFire();
	UFUNCTION() void OnToggleMeta();
	UFUNCTION() void OnCycleStance();
	// THE FIELDS: every ItemFields entry that applies to the open item, as a form under the
	// named controls -- text and numbers typed, lists comma-separated, bools toggled, measured
	// values shown. Saved with everything else. And the review: HIDDEN keeps an entry out of
	// the list (SHOW HIDDEN at the top reveals them), REVIEWED stamps today's date.
	UFUNCTION() void OnToggleShowHidden();
	UFUNCTION() void OnReviewedToday();
	void FillFields(const FReferenceEntry& E);
	void ReadFieldsFromForm(FReferenceEntry& E) const;
	void ToggleBoolField(const FString& Key);
	void CycleEnumField(const FString& Key);
	bool bShowHidden = false;
	UPROPERTY() TObjectPtr<UTextBlock> ShowHiddenLabel;
	UPROPERTY() TObjectPtr<UTextBlock> ReviewLabel;
	UPROPERTY() TObjectPtr<UVerticalBox> FieldsBox;
	UPROPERTY() TMap<FString, TObjectPtr<UEditableTextBox>> FieldBoxes;
	UPROPERTY() TMap<FString, TObjectPtr<UTextBlock>> FieldBoolLabels;
	TMap<FString, bool> FieldBoolValues;
	UPROPERTY() TMap<FString, TObjectPtr<UTextBlock>> FieldEnumLabels;
	TMap<FString, FString> FieldEnumValues;
	TMap<FString, TArray<FString>> FieldEnumOptions;
	UPROPERTY() TArray<TObjectPtr<UReferenceFieldBinding>> FieldBindings;
	void ShowStance();
	FString StanceValue;
	// The optic fitted: "" for irons, else a key from the catalogue's optics. Cycled like the stance.
	FString OpticValue;
	TArray<FString> OpticNames;
	UPROPERTY() TObjectPtr<UTextBlock> OpticLabel;
	UFUNCTION() void OnCycleOptic();
	void ShowOptic();
	bool SelectedOpticFixed() const;   // the sight is part of the gun; the OPTIC cycle refuses
	TArray<FString> StanceNames;   // the catalogue's stances, the order the button cycles them in
	UPROPERTY() TObjectPtr<UTextBlock> StanceLabel;
	// The catalogue's points for the weapon on show: filled dots over the render (PaintMarkers)
	// and the same points as a colour-coded table under it (FillMetaTable), both behind one
	// toggle on the viewer.
	void FillMetaTable(const WeaponCatalog::FWeapon* W);
	bool bShowMeta = true;
	UPROPERTY() TObjectPtr<UVerticalBox> MetaLegend;   // the points' names over the render, in their colours
	UPROPERTY() TObjectPtr<UTextBlock> MetaToggleLabel;
	// A rounded box rounded by half its height is a circle; tinted per point when drawn.
	FSlateRoundedBoxBrush DotBrush = FSlateRoundedBoxBrush(FLinearColor::White, FVector2D(12.0, 12.0));   // half-height rounding: a circle
	void OnTab(int32 Tab);
	void LoadCatalogue();
	bool SaveEntry(const FReferenceEntry& E);
	bool SaveOpticEntry(const FReferenceEntry& E);   // the optics block of Weapons.json
	void FillList();
	void BuildDetail(UVerticalBox* Into);
	void RefreshFeed();
	void PaintMarkers(FSlateWindowElementList& OutDrawElements, int32 Layer) const;
	// THE STICKY HEADER over the scrolling detail: name, path, review date, and the buttons.
	void BuildDetailHeader(UVerticalBox* Into);
	UFUNCTION() void OnRevert();
	UFUNCTION() void OnResetPoints();   // the POINTS back to what the file has; nothing else touched
	UFUNCTION() void OnTuneHands();     // the hand-tuning page for this weapon
	UFUNCTION() void OnCycleSkin();     // the next texture variant this weapon can wear
	UFUNCTION() void OnSetDefaultSkin();   // and the one that makes it this weapon's own
	// RECATEGORISING. One button per category the selected entry is not already in; pressing one
	// moves the record and the card follows to that tab.
	UFUNCTION() void OnMoveToWeapons();
	UFUNCTION() void OnMoveToOptics();
	UFUNCTION() void OnMoveToArmor();
	UFUNCTION() void OnMoveToEquipment();
	UFUNCTION() void OnMoveToConsumables();
	UFUNCTION() void OnMoveToOther();
	void MoveSelectedTo(const FString& NewCategory);
	// Writes the entry into its new home and only then takes it out of the old one, so a failure
	// halfway leaves a duplicate rather than nothing at all. Returns what to tell the user.
	FString MoveEntryTo(FReferenceEntry& E, const FString& NewCategory);
	UPROPERTY() TObjectPtr<class UVerticalBox> MoveBox;
	UPROPERTY() TArray<TObjectPtr<class UWidget>> MoveButtons;
	UPROPERTY() TObjectPtr<class UTextBlock> SkinDefaultLabel;
	UPROPERTY() TObjectPtr<class UWidget> SkinDefaultButton;
	FString TriedSkin;   // the paint on show; empty until the cycler is touched
	void RefreshSkinLabel();
	UPROPERTY() TObjectPtr<class UTextBlock> SkinLabel;   // "[ SKIN 2/6 ]", or hidden where there is no choice
	UPROPERTY() TObjectPtr<class UWidget> SkinButton;
	UFUNCTION() void OnToggleHidden();
	UPROPERTY() TObjectPtr<UTextBlock> HiddenLabel;
	UPROPERTY() TObjectPtr<class UScrollBox> DetailScroll;
	// THE GROUPS of the form, each folded from its own header (== folded, -- open).
	TSet<FString> ExpandedGroups;
	UPROPERTY() TMap<FString, TObjectPtr<UVerticalBox>> GroupBoxes;
	UPROPERTY() TMap<FString, TObjectPtr<UTextBlock>> GroupLabels;
	void ToggleGroup(const FString& Group);
	// DRAGGING A DOT moves the weapon's point: the mouse ray meets the plane through the dot
	// that faces the camera, the hit goes back to HAC1 space, the POINTS field takes it.
	struct FMarkerHit { FString Key; FVector Local; FLinearColor Colour; FString Name; };
	TArray<FMarkerHit> CurrentMarkers() const;
	bool ProjectLocal(const FVector& Local, FVector2D& OutFeedPx) const;
	bool UnprojectToPlane(const FVector2D& FeedPx, const FVector& PlanePointWorld, FVector& OutLocal) const;
	void SetPointField(const FString& Key, const FVector& Local);
	int32 DragMarker = -1;
	// THE ARMED POINT: click a legend row and a drag in the viewer moves that point, however
	// close the others are. Two of the dots sit 2 cm apart (an optic's mount and its eye), and
	// picking by distance alone grabbed whichever was nearer the pixel.
	FString ArmedKey;
	UPROPERTY() TArray<TObjectPtr<UReferenceFieldBinding>> LegendBindings;
	void ArmMarker(const FString& Key);
	bool FeedShown() const;   // the render is on screen: its image visible AND the detail pane open
	// ATTACHMENTS: none, one or many accessory mounts on a weapon. Added under the fore-end,
	// dragged like any point, removed while armed.
	void AddAttachment();
	void RemoveArmedAttachment();
	// Hold X, Y or Z while dragging a point and it moves along that weapon axis only: the pointer's
	// ray is resolved to the nearest point on the axis line through where the point was picked up.
	virtual FReply NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FReply NativeOnKeyUp(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual void NativeOnFocusLost(const FFocusEvent& InFocusEvent) override;
	int32 HeldAxis = -1;        // 0 x, 1 y, 2 z: the key currently down, from the key events that reach this widget
	int32 DragAxis = -1;        // the axis this drag is pinned to (-1: the camera-facing plane)
	FVector DragStartLocal = FVector::ZeroVector;   // the point when the drag began: the axis line and the free plane both run through it
	int32 AxisHeld() const;     // key events first, then the player's key state for keys that went to the game instead
	bool UnprojectToAxis(const FVector2D& FeedPx, const FVector& LineStartLocal, int32 Axis, FVector& OutLocal) const;
	// CTRL + a second click on a card within a moment hides the entry and saves it.
	int32 LastCardIndex = -1;
	double LastCardClickSeconds = 0.0;
	void HideEntry(int32 Index);

	UPROPERTY() TObjectPtr<ABasePlayerController> OwnerController;
	UPROPERTY() TObjectPtr<UVerticalBox> Column;
	UPROPERTY() TObjectPtr<UEditableTextBox> SearchBox;
	UPROPERTY() TObjectPtr<UScrollBox> List;
	UPROPERTY() TObjectPtr<UTextBlock> CountText;
	UPROPERTY() TObjectPtr<USizeBox> DetailBox;
	UPROPERTY() TObjectPtr<UImage> WeaponFeed;
	UPROPERTY() TObjectPtr<UTextBlock> DetailTitle;
	UPROPERTY() TObjectPtr<UTextBlock> DetailMesh;
	UPROPERTY() TObjectPtr<UEditableTextBox> NameBox;
	UPROPERTY() TObjectPtr<class UTextBlock> SoundLabel;
	FString SoundValue;
	TArray<FString> SoundNames() const;
	void ShowSound();
	UFUNCTION() void OnCycleSound();
	UFUNCTION() void OnPlaySound();
	UPROPERTY() TObjectPtr<UEditableTextBox> MakeBox;    // weapons and optics: MAKE and MODEL where NAME was
	UPROPERTY() TObjectPtr<UEditableTextBox> ModelBox;
	UPROPERTY() TObjectPtr<UTextBlock> NameLabel;
	UPROPERTY() TObjectPtr<UTextBlock> MakeLabel;
	UPROPERTY() TObjectPtr<UTextBlock> ModelLabel;
	static FString TitleOf(const FReferenceEntry& E);   // MAKE MODEL for gear, the name otherwise
	UPROPERTY() TObjectPtr<class UButton> HipFireButton;
	UPROPERTY() TObjectPtr<UTextBlock> HipFireLabel;
	bool bHipFireValue = false;
	void ShowHipFire();
	UPROPERTY() TObjectPtr<UMultiLineEditableTextBox> DescBox;
	UPROPERTY() TObjectPtr<UTextBlock> DetailNote;
	UPROPERTY() TArray<TObjectPtr<UReferenceCardBinding>> CardBindings;
	UPROPERTY() TArray<FReferenceEntry> Entries;
	TArray<int32> ShownIndices;   // card order -> entry index
	int32 SelectedIndex = -1;
	FString Filter;
	bool bDragging = false;
	// Ticks of layout settling to hide after a rebuild: the ASCII rules measure themselves
	// against their own rendered text, so the first painted frame is not the final one.
	// The page it replaced is held on screen until this reaches zero. See ShowConsolePage.
	int32 SettleTicks = 0;
	void BeginSettle() { SettleTicks = 2; SetRenderOpacity(0.0f); }
	FVector2D DragLast = FVector2D::ZeroVector;
	// How far the pointer moved while held, so a click can be told from a turn of the piece.
	float DragTravel = 0.0f;
	float Clock = 0.0f;
};
