// HAND TUNING: a page for one weapon. Two orthographic renders of the player holding it -- from the
// side and from above -- with the hold as it really runs (the sight solve, the reach rule, the IK),
// and under them a table of every number the hold is made of: the trigger hand's grip point and
// turn and finger curls, the support hand's fore grip and turn and finger curls. Click a cell to
// pick it, roll the mouse wheel over a cell to change it (shift: fine, ctrl: coarse); the pawn in
// the renders takes the change the same frame. The carry (low ready / shouldered / sights) and the
// aim (high / middle / low) are switched to check every posture. RESET is the catalogue's numbers;
// SAVE writes grip / hand_rot / fingers_r / fore_grip / fore_hand_rot / fingers_l / hunch / pull to
// Weapons.json. The page tunes a STAND-IN (a copy of the player in a booth off the map, see
// ABasePlayerController::ShowHandTune): the player and the world change only on SAVE. Opened from
// the Reference page's weapon detail, under GIVE. Nothing here is estimated: the numbers in the
// cells are the numbers in the file after SAVE.
#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "HandTuneWidget.generated.h"

UCLASS()
class REPLICAN_API UHandTuneWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	// No weapon name: the page asks the controller which weapon it is on, and shows it in the
	// selector. It was only ever passed here to title the header, which it no longer does.
	void Open(class ABasePlayerController* InController, class UTextureRenderTarget2D* Feed);
	void Refresh();
	FSimpleDelegate OnClose;

protected:
	virtual void NativeOnInitialized() override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;          // escape closes
	virtual FReply NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;   // ...even with a button focused
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;   // a click on a cell picks it
	virtual FReply NativeOnMouseWheel(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;        // the wheel edits the cell under the cursor, else the picked one
	virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;     // letting go of a pane puts the view back
	virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;         // dragging a pane moves it
	virtual void NativeOnMouseLeave(const FPointerEvent& InMouseEvent) override;
	UFUNCTION() void OnPresetR0(); UFUNCTION() void OnPresetR1(); UFUNCTION() void OnPresetR2(); UFUNCTION() void OnPresetR3(); UFUNCTION() void OnPresetR4();
	UFUNCTION() void OnPresetL0(); UFUNCTION() void OnPresetL1(); UFUNCTION() void OnPresetL2(); UFUNCTION() void OnPresetL3(); UFUNCTION() void OnPresetL4();
	UFUNCTION() void OnZoomIn();
	UFUNCTION() void OnZoomOut();
	// The standing places the one picture can be seen from.
	UFUNCTION() void OnViewLeft(); UFUNCTION() void OnViewRight(); UFUNCTION() void OnViewTop();
	UFUNCTION() void OnViewFront(); UFUNCTION() void OnViewQuarter();
	UFUNCTION() void OnCarryLow();
	UFUNCTION() void OnCarryShoulder();
	UFUNCTION() void OnCarryAds();
	UFUNCTION() void OnAimHigh();
	UFUNCTION() void OnAimMid();
	UFUNCTION() void OnAimLow();
	UFUNCTION() void OnPrevWeapon();
	UFUNCTION() void OnNextWeapon();
	UPROPERTY() TObjectPtr<class UTextBlock> WeaponLabel;
	UPROPERTY() TObjectPtr<class UTextBlock> OpticLabel;
	UPROPERTY() TObjectPtr<class UTextBlock> SaveLabel;
	UPROPERTY() TObjectPtr<class UWidget> OpticPrev;
	UPROPERTY() TObjectPtr<class UWidget> OpticNext;
	UPROPERTY() TObjectPtr<class UWidget> SkinPrev;
	UFUNCTION() void OnPrevSkin();
	UFUNCTION() void OnPrevOptic();
	UFUNCTION() void OnNextOptic();
	// The sight own paint, under the sight it belongs to.
	UFUNCTION() void OnPrevOpticSkin();
	UFUNCTION() void OnNextOpticSkin();
	UPROPERTY() TObjectPtr<class UTextBlock> OpticSkinLabel;
	UPROPERTY() TObjectPtr<class UWidget> OpticSkinPrev;
	UPROPERTY() TObjectPtr<class UWidget> OpticSkinNext;
	// The whole row, so it can be taken off the page rather than left as a caption with nothing
	// beside it -- hiding only the arrows left the word PAINT sitting there meaning nothing.
	UPROPERTY() TObjectPtr<class UWidget> OpticSkinRow;
	// The column the 3D picture lives in, so CARRY and AIM can sit under it.
	UPROPERTY() TObjectPtr<class UVerticalBox> LeftPane;

public:
	// THE SHAPE OF THE PICTURE, in one place. The render target is made to this same shape by
	// ABasePlayerController, because a square target shown in a box of another shape is stretched --
	// the render was 900x900 in a 620x450 frame, so everything in it was a third wider than it is.
	// A figure whose proportions change with the size of its window is worse than useless for
	// judging a hold, which is the entire purpose of this page.
	static constexpr float PaneWidth = 620.0f;
	static constexpr float PaneHeight = 450.0f;
private:
	// One row of "< label >", so the three selectors are built once and look alike.
	// The label is a TObjectPtr member on this widget, so the parameter has to be one too: a
	// TObjectPtr does not bind to a raw pointer reference.
	class UHorizontalBox* Selector(const TCHAR* Caption, TObjectPtr<class UTextBlock>& OutLabel,
		class UWidget*& OutPrev, class UWidget*& OutNext);
	// Weapon navigation with a guard: unsaved work asks first.
	void StepWeaponGuarded(int32 Dir);
	UFUNCTION() void OnCycleSkin();
	UFUNCTION() void OnSetDefaultSkin();
	UPROPERTY() TObjectPtr<class UTextBlock> SkinDefaultLabel;
	UPROPERTY() TObjectPtr<class UWidget> SkinDefaultButton;
	UFUNCTION() void OnReset();
	UFUNCTION() void OnSave();
	UFUNCTION() void OnBack();

private:
	UPROPERTY() TObjectPtr<class ABasePlayerController> Controller;
	UPROPERTY() TObjectPtr<class UTextBlock> Title;
	UPROPERTY() TObjectPtr<class UImage> ViewImage;
	UPROPERTY() TObjectPtr<class UTextBlock> Note;
	UPROPERTY() TObjectPtr<class UTextBlock> SkinLabel;
	UPROPERTY() TObjectPtr<class UWidget> SkinButton;
	// The choice buttons' labels, recoloured to show which is live.
	UPROPERTY() TMap<FString, TObjectPtr<class UTextBlock>> Labels;
	// A labelled choice button, into a row or a column: the carry and aim choices moved from a
	// row under the pictures into the strip beside them, and they are the same buttons either way.
	class UButton* Choice(class UPanelWidget* Into, const FString& Id, const FString& Text);
	void Highlight(const FString& Id, bool bOn);
	// One cell of the table: which controller row / column it edits, its box and its text, its unit.
	struct FCell
	{
		int32 Row = 0, Col = 0;
		class UBorder* Box = nullptr;      // owned by the widget tree
		class UTextBlock* Text = nullptr;
		const TCHAR* Axis = TEXT("");
		bool bDegrees = false;
		bool bWhole = false;               // finger curls read as whole degrees
	};
	TArray<FCell> Cells;
	int32 Picked = -1;
	// The one picture, for the drag and the zoom.
	UPROPERTY() TObjectPtr<class UBorder> ViewPane;
	// The AIM caption and its three buttons together, so they can go when the carry has no aim.
	UPROPERTY() TObjectPtr<class UVerticalBox> AimGroup;
	bool PaneHovered() const;
	// Named hand shapes from the catalogue: five slots per hand, labelled and shown at Open.
	UPROPERTY() TArray<TObjectPtr<class UWidget>> PresetButtons;
	UPROPERTY() TArray<TObjectPtr<class UTextBlock>> PresetLabels;
	void Preset(int32 Index, bool bSupport);
	bool bDragging = false;
	FVector2D DragFrom = FVector2D::ZeroVector;
	int32 HoveredCell() const;
	void Pick(int32 Index);
	void PaintCell(int32 Index);
	// A cell the carry in view does not use -- the hunch away from the sights, low ready's angles
	// anywhere else, the pull column belonging to another carry -- is greyed and inert.
	bool CellLive(const FCell& Cell) const;
	void Adjust(int32 Index, float Notches, bool bFine, bool bCoarse);
};
