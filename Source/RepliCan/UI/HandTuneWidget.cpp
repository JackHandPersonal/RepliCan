#include "UI/HandTuneWidget.h"
#include "Core/BasePlayerController.h"
#include "Weapons/WeaponCatalog.h"
#include "UI/CrtStyle.h"
#include "UI/CrtRuleWidget.h"
#include "UI/SheetSpec.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/BorderSlot.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/PanelWidget.h"
#include "Components/PanelSlot.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/TextureRenderTarget2D.h"

namespace
{
	const FLinearColor CellGround(0.015f, 0.05f, 0.025f, 1.0f);    // a cell at rest, a shade above the panel
	const FLinearColor CellPicked(0.05f, 0.18f, 0.08f, 1.0f);      // the picked cell
	const FLinearColor CellDead(0.010f, 0.028f, 0.016f, 1.0f);     // one this carry does not use
}

UButton* UHandTuneWidget::Choice(UPanelWidget* Into, const FString& Id, const FString& Text)
{
	const FSheetSpec& S = FSheetSpec::Get();
	UButton* B = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
	B->SetStyle(Crt::ButtonStyle());
	UTextBlock* L = Crt::FixedText(WidgetTree, Text, S.CaptionSize, Crt::DimGreen);
	B->AddChild(L);
	if (UButtonSlot* BS = Cast<UButtonSlot>(L->Slot)) { BS->SetPadding(FMargin(10.0f, 4.0f)); }
	Labels.Add(Id, L);
	// Not called Slot: UWidget has a member of that name and this project builds shadowing as an error.
	UPanelSlot* Placed = Into ? Into->AddChild(B) : nullptr;
	// Tight. These are a SET of choices -- one of which is on -- and a set reads as a set when its
	// members touch; spread apart they read as unrelated buttons that happen to be near each other.
	if (UHorizontalBoxSlot* H = Cast<UHorizontalBoxSlot>(Placed)) { H->SetPadding(FMargin(0, 0, 4, 0)); }
	else if (UVerticalBoxSlot* V = Cast<UVerticalBoxSlot>(Placed)) { V->SetPadding(FMargin(0, 0, 0, 2)); }
	return B;
}

void UHandTuneWidget::Highlight(const FString& Id, bool bOn)
{
	if (TObjectPtr<UTextBlock>* L = Labels.Find(Id)) { if (*L) { (*L)->SetColorAndOpacity(FSlateColor(bOn ? Crt::Green : Crt::DimGreen)); } }
}

void UHandTuneWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	SetIsFocusable(true);
	const FSheetSpec& S = FSheetSpec::Get();
	UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("HandTuneRoot"));
	WidgetTree->RootWidget = Root;
	USizeBox* Fit = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("HandTuneFit"));
	// THE SAME SIZE AS THE CHARACTER SHEET -- and that size is not decided here. The page is placed
	// by PlaceConsolePage into ConsolePageRect(), the one rectangle every console page shares, so
	// this fills whatever it is given rather than naming a width of its own. It was a 1180-wide box
	// centred on screen once, which left the strip beside the picture no room and hung SET DEFAULT
	// off the edge; filling the SCREEN instead fixed that and made the page full-bleed, which is a
	// different wrong. Filling the page rect is the answer to both.
	UOverlaySlot* FitSlot = Root->AddChildToOverlay(Fit);
	FitSlot->SetHorizontalAlignment(HAlign_Fill); FitSlot->SetVerticalAlignment(VAlign_Fill);
	UBorder* Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("HandTunePanel"));
	Panel->SetBrushColor(Crt::Panel);
	Panel->SetPadding(FMargin(34.0f, 26.0f));   // the sheet's margin, so the two pages sit alike
	Fit->AddChild(Panel);
	UVerticalBox* Outer = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("HandTuneOuter"));
	Panel->SetContent(Outer);

	// The spacer between things set into the header: a piece of the rule, not a hole in it.
	auto Gap = [&](UHorizontalBox* Into)
	{
		Into->AddChildToHorizontalBox(Crt::FixedText(WidgetTree, TEXT(" = "), S.RuleSize, Crt::Faint))->SetVerticalAlignment(VAlign_Center);
	};
	// The header: the rule, the title, save, reset, the close.
	UHorizontalBox* Head = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	Head->AddChildToHorizontalBox(Crt::FixedText(WidgetTree, S.HeaderLeft, S.RuleSize, Crt::Faint))->SetVerticalAlignment(VAlign_Center);
	Title = Crt::FixedText(WidgetTree, TEXT("HAND TUNING"), S.NameSize, Crt::Green);
	Head->AddChildToHorizontalBox(Title)->SetVerticalAlignment(VAlign_Center);
	// (The weapon selector used to sit here. It is one of three selectors in the strip now -- weapon,
	// paint, optic -- because they are the same kind of control and belong together, and because a
	// header is for saying which page this is.)
	UHorizontalBoxSlot* RuleSlot = Head->AddChildToHorizontalBox(UCrtRuleWidget::Make(GetOwningPlayer(), S.HeaderRight, S.RuleSize, Crt::Faint));
	RuleSlot->SetSize(ESlateSizeRule::Fill); RuleSlot->SetVerticalAlignment(VAlign_Center);
	// SAVE AND RESET LIVE IN THE HEADER. They are not adjustments -- they are what you do when the
	// adjusting is finished -- so they belong with the close, not at the top of the strip where they
	// were the first thing read on a page that is entirely about the rows below them.
	{
		UButton* Save = Crt::Button(WidgetTree, TEXT("[ SAVE   ]"), S.CaptionSize, Crt::Green);
		Save->OnClicked.AddDynamic(this, &UHandTuneWidget::OnSave);
		SaveLabel = Cast<UTextBlock>(Save->GetChildAt(0));   // the asterisk goes on this
		// The face is fixed-pitch, so the asterisk is spent inside a constant ten characters: the
		// brackets never move and RESET is never shoved along the header. A size box would hold the
		// width but not the brackets -- it centres the shorter label and puts the slack back at the
		// edges, which is the thing being removed.
		Head->AddChildToHorizontalBox(Save)->SetVerticalAlignment(VAlign_Center);
		// THE RULE CHARACTER IS THE SPACER. The header is a drawn rule with things set into it, so
		// the gap between those things should be more of the rule and not a hole in it. It also
		// keeps the buttons tight against each other instead of drifting apart on blank padding.
		Gap(Head);
		UButton* Reset = Crt::Button(WidgetTree, TEXT("[ RESET ]"), S.CaptionSize, Crt::DimGreen);
		Reset->OnClicked.AddDynamic(this, &UHandTuneWidget::OnReset);
		Head->AddChildToHorizontalBox(Reset)->SetVerticalAlignment(VAlign_Center);
		Gap(Head);
	}
	UButton* Close = Crt::Button(WidgetTree, TEXT("[ X ]"), S.NameSize, Crt::Green);
	Close->OnClicked.AddDynamic(this, &UHandTuneWidget::OnBack);
	Head->AddChildToHorizontalBox(Close)->SetVerticalAlignment(VAlign_Center);
	Outer->AddChildToVerticalBox(Head)->SetPadding(FMargin(0, 0, 0, 10));

	// THE ROW TABLE IS DEFINED FIRST, because the control strip now uses it too: SIZE sits under
	// the weapon paint and the optic offset under the optic paint, and those are table rows like
	// any other -- built by the same BuildRows so a cell behaves the same wherever it appears.
	// THE TABLE, IN TWO COLUMNS. The weapon's own numbers on the left, the posture and the carry and
	// the body's eyeline on the right: one column of thirteen rows ran off the bottom of the screen
	// and took SAVE and CLOSE with it, and left half the panel empty besides.
	struct FRowSpec { const TCHAR* Group; const TCHAR* Label; int32 Row; int32 Count; const TCHAR* const* Axes; bool bDegrees; bool bWhole; };
	static const TCHAR* const XYZ[3] = { TEXT("x"), TEXT("y"), TEXT("z") };
	static const TCHAR* const PYR[3] = { TEXT("p"), TEXT("y"), TEXT("r") };
	static const TCHAR* const THREE[3] = { TEXT("thumb"), TEXT("index"), TEXT("middle") };
	static const TCHAR* const CM[1] = { TEXT("cm") };
	static const TCHAR* const PERCENT[1] = { TEXT("%") };
	static const TCHAR* const DEGREE[1] = { TEXT("deg") };
	static const TCHAR* const CARRY3[3] = { TEXT("low"), TEXT("shldr"), TEXT("ads") };
	static const TCHAR* const EYE3[3] = { TEXT("side"), TEXT("up"), TEXT("fwd") };
	static const TCHAR* const PITCHYAW[2] = { TEXT("pitch"), TEXT("yaw") };
	static const TCHAR* const AIM3[3] = { TEXT("high"), TEXT("mid"), TEXT("low") };
	const FRowSpec LeftRows[] = {
		{ TEXT("MAIN"), TEXT("grip"),          0, 3, XYZ,   false, false },
		{ TEXT(""),        TEXT("rotation"),      1, 3, PYR,   true,  false },
		{ TEXT(""),        TEXT("fingers"),       2, 3, THREE, true,  true  },
		{ TEXT("SUPPORT"), TEXT("grip"),          3, 3, XYZ,   false, false },
		{ TEXT(""),        TEXT("rotation"),      4, 3, PYR,   true,  false },
		{ TEXT(""),        TEXT("fingers"),       5, 3, THREE, true,  true  },
	};
	// TWO TALL COLUMNS, BESIDE THE PICTURE -- not three short ones under it. Laid across the bottom
	// the table used the full width and left the whole right-hand third of the page empty, with the
	// picture and the controls huddled top-left. Standing the rows up in two columns puts them in
	// that empty space, and the page stops being mostly floor.
	const FRowSpec MidRows[] = {
		{ TEXT("POSTURE"), TEXT("hunch"),         6, 1, CM,    false, false },   // shoulders up, head down: at the sights only
		{ TEXT(""),        TEXT("lean"),          8, 1, DEGREE, true, false },   // the torso at the waist: at the sights only
		// ONE ROW, ONE CARRY. The three cells are x, y and z of the carry the page is SHOWING, not
		// three carries of one axis -- pick LOW, SHLDR or ADS beside the picture and this row follows.
		// x runs along the weapon's own bore (this was "pull"), y off to the trigger side (this was
		// "lateral"), z lifts it toward the eye line. Both old rows are gone: they were two axes of
		// this one vector wearing different names.
		{ TEXT("CARRY"),   TEXT("position"),     18, 3, XYZ,   false, false },
		{ TEXT(""),        TEXT("low ready"),    11, 2, PITCHYAW, true, false }, // degrees off the aim down there
	};
	const FRowSpec RightRows[] = {
		{ TEXT("ARMS"),    TEXT("elbow main"),   12, 3, CARRY3, true, false },   // the main elbow about its reach line, per carry
		{ TEXT(""),        TEXT("elbow support"),13, 3, CARRY3, true, false },
		// AND THE SAME ELBOW ACROSS THE AIM, which is the axis the carry columns cannot express: an
		// arm that sits right at the shoulder is wrong pointed at the ceiling. These three are read
		// at the AIM buttons' own angles and added to the carry's value, so zeros here mean "as it
		// was", and between and past them the value carries on at the same slope.
		{ TEXT(""),        TEXT("elbow main aim"),   14, 3, AIM3, true, false },
		{ TEXT(""),        TEXT("elbow sup aim"),    15, 3, AIM3, true, false },
	};
	// THE STRIP'S CELLS ARE WIDER THAN THE TABLE'S. The table is three columns of numbers repeated
	// down the page and lives or dies on fitting; the strip beside the picture has width going
	// spare, and its numbers -- the size, the optic offset, the eyeline -- are the ones read against
	// the render while adjusting it, so they get the room.
	const float StripCellWidth = 132.0f;

	auto BuildRows = [&](const FRowSpec* Specs, int32 Num, float CellW = 94.0f) -> UVerticalBox*
	{
		UVerticalBox* Col = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		for (int32 r = 0; r < Num; ++r)
		{
			const FRowSpec& Spec = Specs[r];
			UHorizontalBox* Line = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
			USizeBox* GroupBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass()); GroupBox->SetWidthOverride(62.0f);
			GroupBox->AddChild(Crt::FixedText(WidgetTree, Spec.Group, S.CaptionSize, Crt::Faint));
			Line->AddChildToHorizontalBox(GroupBox)->SetVerticalAlignment(VAlign_Center);
			USizeBox* LabelBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass()); LabelBox->SetWidthOverride(104.0f);
			LabelBox->AddChild(Crt::FixedText(WidgetTree, Spec.Label, S.RowSize, Crt::DimGreen));
			Line->AddChildToHorizontalBox(LabelBox)->SetVerticalAlignment(VAlign_Center);
			// A MODE ROW COLLAPSES TO ONE CELL. Where the three columns are carries or aims rather
			// than axes, only the one in view was ever live -- the other two sat dimmed, taking two
			// thirds of the row's width to show numbers nobody could touch. One cell, and it follows
			// the CARRY or AIM buttons.
			const bool bModeRow = (Spec.Axes == CARRY3 || Spec.Axes == AIM3);
			for (int32 c = 0; c < (bModeRow ? 1 : Spec.Count); ++c)
			{
				FCell Cell; Cell.Row = Spec.Row; Cell.Col = c; Cell.Axis = Spec.Axes[c]; Cell.bDegrees = Spec.bDegrees; Cell.bWhole = Spec.bWhole;
				if (bModeRow) { Cell.AxisSet = Spec.Axes; Cell.bActiveCol = true; Cell.bAim = (Spec.Axes == AIM3); }
				// THE CELLS CARRY THE WIDTH OF THE PAGE. Three columns of them at 112 plus a 78 gutter
				// and a 120 label ran the last column off the right edge; the numbers themselves need
				// about sixty. Narrower cells and gutters bring the whole table inside the panel and
				// read as one block rather than three drifting apart.
				USizeBox* CellBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass()); CellBox->SetWidthOverride(CellW);
				Cell.Box = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
				Cell.Box->SetBrushColor(CellGround);
				Cell.Box->SetPadding(FMargin(6.0f, 3.0f));
				Cell.Text = Crt::FixedText(WidgetTree, TEXT(""), S.RowSize, Crt::DimGreen);
				Cell.Box->SetContent(Cell.Text);
				CellBox->AddChild(Cell.Box);
				Line->AddChildToHorizontalBox(CellBox)->SetPadding(FMargin(0, 0, 5, 0));
				Cells.Add(Cell);
			}
			Col->AddChildToVerticalBox(Line)->SetPadding(FMargin(0, 0, 0, 3));
		}
		return Col;
	};

	// ONE RENDER, and the sides on call above it. Two fixed pictures took half the panel and still
	// never showed the angle a particular hold needed.
	UHorizontalBox* Views = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	{
		USizeBox* Box = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		// A little shorter than it was: the picture was taking height the rows needed, and nothing in
		// a hold is hidden by trimming seventy pixels off the bottom of the frame. The FRAME changes
		// shape, not the render -- the capture is made to match (see PaneWidth / PaneHeight).
		Box->SetWidthOverride(PaneWidth); Box->SetHeightOverride(PaneHeight);
		UOverlay* Stack = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
		UBorder* Frame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		ViewPane = Frame;
		Frame->SetBrushColor(FLinearColor(0.02f, 0.05f, 0.03f, 1.0f)); Frame->SetPadding(FMargin(2.0f));
		ViewImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
		ViewImage->SetColorAndOpacity(FLinearColor::White);
		Frame->SetContent(ViewImage);
		if (UOverlaySlot* FS = Stack->AddChildToOverlay(Frame)) { FS->SetHorizontalAlignment(HAlign_Fill); FS->SetVerticalAlignment(VAlign_Fill); }
		// The standing places, along the top of the picture.
		UHorizontalBox* ViewRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		Choice(ViewRow, TEXT("view_left"), TEXT("[ LEFT ]"))->OnClicked.AddDynamic(this, &UHandTuneWidget::OnViewLeft);
		Choice(ViewRow, TEXT("view_right"), TEXT("[ RIGHT ]"))->OnClicked.AddDynamic(this, &UHandTuneWidget::OnViewRight);
		Choice(ViewRow, TEXT("view_top"), TEXT("[ TOP ]"))->OnClicked.AddDynamic(this, &UHandTuneWidget::OnViewTop);
		Choice(ViewRow, TEXT("view_front"), TEXT("[ FRONT ]"))->OnClicked.AddDynamic(this, &UHandTuneWidget::OnViewFront);
		Choice(ViewRow, TEXT("view_quarter"), TEXT("[ 3/4 ]"))->OnClicked.AddDynamic(this, &UHandTuneWidget::OnViewQuarter);
		if (UOverlaySlot* VS = Stack->AddChildToOverlay(ViewRow))
		{
			VS->SetHorizontalAlignment(HAlign_Center); VS->SetVerticalAlignment(VAlign_Top); VS->SetPadding(FMargin(0, 8, 0, 0));
		}
		// The zoom stays where it was, bottom right, out of the way of the picture.
		UHorizontalBox* ZoomRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		UButton* Out = Crt::Button(WidgetTree, TEXT("[ - ]"), S.CaptionSize, Crt::DimGreen); Out->OnClicked.AddDynamic(this, &UHandTuneWidget::OnZoomOut);
		UButton* In = Crt::Button(WidgetTree, TEXT("[ + ]"), S.CaptionSize, Crt::DimGreen); In->OnClicked.AddDynamic(this, &UHandTuneWidget::OnZoomIn);
		ZoomRow->AddChildToHorizontalBox(Out)->SetPadding(FMargin(0, 0, 6, 0));
		ZoomRow->AddChildToHorizontalBox(In);
		if (UOverlaySlot* ZS = Stack->AddChildToOverlay(ZoomRow))
		{
			ZS->SetHorizontalAlignment(HAlign_Right); ZS->SetVerticalAlignment(VAlign_Bottom); ZS->SetPadding(FMargin(0, 0, 10, 8));
		}
		// CARRY AND AIM, ON THE PICTURE. Both of them change what the picture is SHOWING, so they
		// belong on it rather than in a strip elsewhere -- and over the render they cost no page
		// height at all, which is the thing this page is short of.
		{
			// ONE NARROW COLUMN DOWN THE LEFT OF THE PICTURE: the two headers with their buttons
			// stacked under each. Laid out across the bottom they ran under the figure; standing up
			// the side they sit on the empty background beside it, where they hide nothing.
			UVerticalBox* Poses = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
			auto Stacked = [&](UVerticalBox* Into, const TCHAR* Caption)
			{
				Into->AddChildToVerticalBox(Crt::FixedText(WidgetTree, Caption, S.CaptionSize, Crt::Faint))->SetPadding(FMargin(0, 0, 0, 1));
			};
			UVerticalBox* CarryCol = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
			Stacked(CarryCol, TEXT("CARRY"));
			Choice(CarryCol, TEXT("carry_low"), TEXT("[ LOW ]"))->OnClicked.AddDynamic(this, &UHandTuneWidget::OnCarryLow);
			Choice(CarryCol, TEXT("carry_shoulder"), TEXT("[ SHLDR ]"))->OnClicked.AddDynamic(this, &UHandTuneWidget::OnCarryShoulder);
			Choice(CarryCol, TEXT("carry_ads"), TEXT("[ ADS ]"))->OnClicked.AddDynamic(this, &UHandTuneWidget::OnCarryAds);
			Poses->AddChildToVerticalBox(CarryCol)->SetPadding(FMargin(0, 0, 0, 10));
			// AIM is the row the elbow-across-the-aim columns are read against: picking HIGH here is
			// what makes the "high" column of those rows the live one.
			AimGroup = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
			Stacked(AimGroup, TEXT("AIM"));
			Choice(AimGroup, TEXT("aim_high"), TEXT("[ HIGH ]"))->OnClicked.AddDynamic(this, &UHandTuneWidget::OnAimHigh);
			Choice(AimGroup, TEXT("aim_mid"), TEXT("[ MID ]"))->OnClicked.AddDynamic(this, &UHandTuneWidget::OnAimMid);
			Choice(AimGroup, TEXT("aim_low"), TEXT("[ LOW ]"))->OnClicked.AddDynamic(this, &UHandTuneWidget::OnAimLow);
			Poses->AddChildToVerticalBox(AimGroup);
			if (UOverlaySlot* PS = Stack->AddChildToOverlay(Poses))
			{
				PS->SetHorizontalAlignment(HAlign_Left); PS->SetVerticalAlignment(VAlign_Bottom); PS->SetPadding(FMargin(8, 0, 0, 8));
			}
		}
		Box->AddChild(Stack);
		// A COLUMN AROUND THE PICTURE, so what belongs with it can sit underneath it rather than
		// being pushed into the strip on the right. CARRY and AIM go there: both of them change what
		// the picture is SHOWING, so they belong against the picture and nowhere else.
		LeftPane = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		LeftPane->AddChildToVerticalBox(Box);
		Views->AddChildToHorizontalBox(LeftPane)->SetPadding(FMargin(0, 0, 16, 0));
	}
	// RESET, SAVE and CLOSE stand in the strip beside the second picture, where there is width to
	// spare and no height to spare.
	{
		UVerticalBox* Actions = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());

		// CARRY and AIM live under RESET, in the same strip: they belong with the buttons, and the
		// row they used to occupy was a row of height the table needed. No CLOSE here -- the [ X ] in
		// the header already closes the page, and two ways out is one too many.
		// The paint, so a colourway can be judged against the hold without leaving the page.
		// THE THREE SELECTORS, one above the other: which weapon, which paint, which optic. Each is
		// "< label >" so the shape of the control says what it does without being read. The two things
		// that belong TO a selector now ride on its own row rather than under it: how big the weapon
		// is drawn beside the weapon, and SET DEFAULT beside the paint it would make default. The
		// strip has the width for it since the cells and the name field were widened.
		UWidget* WeapPrev = nullptr; UWidget* WeapNext = nullptr;
		UHorizontalBox* WeaponRow = Selector(TEXT("WEAPON"), WeaponLabel, WeapPrev, WeapNext);
		if (UButton* B = Cast<UButton>(WeapPrev)) { B->OnClicked.AddDynamic(this, &UHandTuneWidget::OnPrevWeapon); }
		if (UButton* B = Cast<UButton>(WeapNext)) { B->OnClicked.AddDynamic(this, &UHandTuneWidget::OnNextWeapon); }
		{
			const FRowSpec SizeOnly[] = { { TEXT(""), TEXT("size"), 17, 1, PERCENT, false, false } };
			WeaponRow->AddChildToHorizontalBox(BuildRows(SizeOnly, UE_ARRAY_COUNT(SizeOnly), StripCellWidth))->SetVerticalAlignment(VAlign_Center);
		}
		Actions->AddChildToVerticalBox(WeaponRow)->SetPadding(FMargin(0, 0, 0, 10));

		// Locals, then stored: Selector writes through raw pointer references and a TObjectPtr member
		// will not bind to one. Every other selector here already did it this way.
		UWidget* SkinPrevW = nullptr; UWidget* SkinNextW = nullptr;
		UHorizontalBox* SkinRow = Selector(TEXT("PAINT"), SkinLabel, SkinPrevW, SkinNextW);
		SkinPrev = SkinPrevW;
		if (UButton* B = Cast<UButton>(SkinPrevW)) { B->OnClicked.AddDynamic(this, &UHandTuneWidget::OnPrevSkin); }
		if (UButton* B = Cast<UButton>(SkinNextW)) { B->OnClicked.AddDynamic(this, &UHandTuneWidget::OnCycleSkin); }
		SkinButton = SkinRow;
		UButton* DefBtn = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
		DefBtn->SetStyle(Crt::ButtonStyle());
		SkinDefaultLabel = Crt::FixedText(WidgetTree, TEXT("[ SET DEFAULT ]"), S.CaptionSize, Crt::DimGreen);
		DefBtn->AddChild(SkinDefaultLabel);
		if (UButtonSlot* DS = Cast<UButtonSlot>(SkinDefaultLabel->Slot)) { DS->SetPadding(FMargin(10.0f, 4.0f)); }
		DefBtn->OnClicked.AddDynamic(this, &UHandTuneWidget::OnSetDefaultSkin);
		SkinDefaultButton = DefBtn;
		SkinRow->AddChildToHorizontalBox(DefBtn)->SetVerticalAlignment(VAlign_Center);
		Actions->AddChildToVerticalBox(SkinRow)->SetPadding(FMargin(0, 0, 0, 12));

		UWidget* OptPrev = nullptr; UWidget* OptNext = nullptr;
		Actions->AddChildToVerticalBox(Selector(TEXT("OPTIC"), OpticLabel, OptPrev, OptNext))->SetPadding(FMargin(0, 0, 0, 14));
		OpticPrev = OptPrev; OpticNext = OptNext;
		if (UButton* B = Cast<UButton>(OptPrev)) { B->OnClicked.AddDynamic(this, &UHandTuneWidget::OnPrevOptic); }
		if (UButton* B = Cast<UButton>(OptNext)) { B->OnClicked.AddDynamic(this, &UHandTuneWidget::OnNextOptic); }
		// PAINT, under OPTIC: the same control as the weapon skin cycler, pointed at the sight. It
		// hides itself when the fitted optic ships in only one colourway, so the row is never a
		// button that does nothing.
		{
			UWidget* SkPrev = nullptr; UWidget* SkNext = nullptr;
			UHorizontalBox* SkRow = Selector(TEXT("PAINT"), OpticSkinLabel, SkPrev, SkNext);
			OpticSkinRow = SkRow;
			Actions->AddChildToVerticalBox(SkRow)->SetPadding(FMargin(0, 0, 0, 14));
			OpticSkinPrev = SkPrev; OpticSkinNext = SkNext;
			if (UButton* B = Cast<UButton>(SkPrev)) { B->OnClicked.AddDynamic(this, &UHandTuneWidget::OnPrevOpticSkin); }
			if (UButton* B = Cast<UButton>(SkNext)) { B->OnClicked.AddDynamic(this, &UHandTuneWidget::OnNextOpticSkin); }
		}
		// WHERE THE SIGHT SITS ON THE RAIL, directly under the sight it belongs to. Placing an optic
		// is one job; having its offset three columns away in the table made it two.
		{
			const FRowSpec OpticOnly[] = { { TEXT(""), TEXT("offset"), 16, 3, XYZ, false, false } };
			Actions->AddChildToVerticalBox(BuildRows(OpticOnly, UE_ARRAY_COUNT(OpticOnly), StripCellWidth))->SetPadding(FMargin(0, 0, 0, 6));
		}
		// WHOSE BODY, directly above the eyeline that belongs to it. The eyeline is the one number on
		// this page that is the character's rather than the weapon's, so the two controls are one
		// idea and sit together. Defaults to the character being played; steps through the real
		// characters only.
		{
			UWidget* ChPrev = nullptr; UWidget* ChNext = nullptr;
			Actions->AddChildToVerticalBox(Selector(TEXT("BODY"), CharacterLabel, ChPrev, ChNext))->SetPadding(FMargin(0, 0, 0, 6));
			CharacterPrev = ChPrev; CharacterNext = ChNext;
			if (UButton* B = Cast<UButton>(ChPrev)) { B->OnClicked.AddDynamic(this, &UHandTuneWidget::OnPrevCharacter); }
			if (UButton* B = Cast<UButton>(ChNext)) { B->OnClicked.AddDynamic(this, &UHandTuneWidget::OnNextCharacter); }
		}
		// THE EYELINE, under the optic offset. Both answer the same question -- where the sight ends
		// up in front of the eye -- and they were being adjusted against each other from opposite
		// ends of the page. This one is the BODY's and is saved on the character, not the weapon.
		{
			const FRowSpec EyeOnly[] = { { TEXT("EYELINE"), TEXT("eye"), 10, 3, EYE3, false, false } };
			Actions->AddChildToVerticalBox(BuildRows(EyeOnly, UE_ARRAY_COUNT(EyeOnly), StripCellWidth))->SetPadding(FMargin(0, 0, 0, 6));
		}
		Views->AddChildToHorizontalBox(Actions)->SetVerticalAlignment(VAlign_Top);
	}
	Outer->AddChildToVerticalBox(Views)->SetPadding(FMargin(0, 0, 0, 10));


	UHorizontalBox* Columns = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	UVerticalBox* Left = BuildRows(LeftRows, UE_ARRAY_COUNT(LeftRows));

	// FINGER PRESETS, under the hands they belong to. A trigger hand is held much the same on every
	// firearm, so the shapes live once in UI/Weapons.json ("finger_presets") and a click drops one
	// into that hand's row. Five slots per hand; a slot with no preset behind it is hidden.
	for (int32 Hand = 0; Hand < 2; ++Hand)
	{
		UHorizontalBox* Line = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		USizeBox* Cap = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass()); Cap->SetWidthOverride(78.0f);
		Cap->AddChild(Crt::FixedText(WidgetTree, Hand == 0 ? TEXT("PRESET R") : TEXT("PRESET L"), S.CaptionSize, Crt::Faint));
		Line->AddChildToHorizontalBox(Cap)->SetVerticalAlignment(VAlign_Center);
		for (int32 k = 0; k < 5; ++k)
		{
			UButton* B = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
			B->SetStyle(Crt::ButtonStyle());
			UTextBlock* L = Crt::FixedText(WidgetTree, TEXT(""), S.CaptionSize, Crt::DimGreen);
			B->AddChild(L);
			if (UButtonSlot* BS = Cast<UButtonSlot>(L->Slot)) { BS->SetPadding(FMargin(6.0f, 3.0f)); }
			if (Hand == 0)
			{
				switch (k) {
				case 0: B->OnClicked.AddDynamic(this, &UHandTuneWidget::OnPresetR0); break;
				case 1: B->OnClicked.AddDynamic(this, &UHandTuneWidget::OnPresetR1); break;
				case 2: B->OnClicked.AddDynamic(this, &UHandTuneWidget::OnPresetR2); break;
				case 3: B->OnClicked.AddDynamic(this, &UHandTuneWidget::OnPresetR3); break;
				default: B->OnClicked.AddDynamic(this, &UHandTuneWidget::OnPresetR4); break; }
			}
			else
			{
				switch (k) {
				case 0: B->OnClicked.AddDynamic(this, &UHandTuneWidget::OnPresetL0); break;
				case 1: B->OnClicked.AddDynamic(this, &UHandTuneWidget::OnPresetL1); break;
				case 2: B->OnClicked.AddDynamic(this, &UHandTuneWidget::OnPresetL2); break;
				case 3: B->OnClicked.AddDynamic(this, &UHandTuneWidget::OnPresetL3); break;
				default: B->OnClicked.AddDynamic(this, &UHandTuneWidget::OnPresetL4); break; }
			}
			Line->AddChildToHorizontalBox(B)->SetPadding(FMargin(0, 0, 5, 0));
			PresetButtons.Add(B);
			PresetLabels.Add(L);
		}
		Left->AddChildToVerticalBox(Line)->SetPadding(FMargin(0, 0, 0, 3));
	}
	// Three columns across the page. The counts are taken from the arrays themselves: the right
	// column used to be built with a hard-coded 7 against an array of 10, so the last three rows --
	// both elbow-across-the-aim rows and the whole EYELINE row -- were constructed and then never
	// shown. A literal that has to agree with an array length is a literal that eventually will not.
	Columns->AddChildToHorizontalBox(Left)->SetPadding(FMargin(0, 0, 14, 0));
	// THREE COLUMNS, SIDE BY SIDE, now that the table has the full width under the page. Stacking
	// two of the groups made one column eleven rows tall, which buys width back by spending height
	// -- and height is the scarcer of the two here, with a 520-pixel picture above. Three columns is
	// about 1420 wide against roughly 1780 available, and eight rows tall at the worst column.
	Columns->AddChildToHorizontalBox(BuildRows(MidRows, UE_ARRAY_COUNT(MidRows)))->SetPadding(FMargin(0, 0, 14, 0));
	Columns->AddChildToHorizontalBox(BuildRows(RightRows, UE_ARRAY_COUNT(RightRows)));
	// THE TABLE GOES UNDER EVERYTHING, ACROSS THE FULL WIDTH. Putting it in the picture row was the
	// wrong call: picture (620) plus the control strip (about 300) plus two columns of rows (about
	// 460 each) is some nineteen hundred pixels of demand on a page that has less than that, so the
	// last column simply left the page. Underneath it has the whole width to itself and needs about
	// half of it.
	Outer->AddChildToVerticalBox(Columns)->SetPadding(FMargin(0, 0, 0, 8));

	// NO NOTE LINE. The cells show the numbers and the pictures show the hold, so a sentence saying
	// what was just saved is a row of height spent on something the page has already said.
	Note = nullptr;
}

void UHandTuneWidget::Open(ABasePlayerController* InController, UTextureRenderTarget2D* Feed)
{
	Controller = InController;
	// JUST THE PAGE'S NAME. The weapon is named once, in the selector to the right, where the
	// arrows that change it are; putting it in the title as well said the same thing twice across
	// the top of the panel.
	if (Title) { Title->SetText(FText::FromString(TEXT("HAND TUNING"))); }
	auto Show = [](UImage* Into, UTextureRenderTarget2D* RT)
	{
		if (!Into || !RT || Into->GetBrush().GetResourceObject() == RT) { return; }
		FSlateBrush Brush;
		Brush.SetResourceObject(RT);
		Brush.ImageSize = FVector2D(RT->SizeX, RT->SizeY);
		Brush.DrawAs = ESlateBrushDrawType::Image;
		Into->SetBrush(Brush);
	};
	Show(ViewImage, Feed);
	// Label the preset slots from the catalogue and hide the spare ones.
	const int32 Have = Controller ? Controller->HandTunePresetCount() : 0;
	for (int32 i = 0; i < PresetButtons.Num(); ++i)
	{
		const int32 PresetIdx = i % 5;
		const bool bShow = PresetIdx < Have;
		if (PresetButtons[i]) { PresetButtons[i]->SetVisibility(bShow ? ESlateVisibility::Visible : ESlateVisibility::Collapsed); }
		if (bShow && PresetLabels[i]) { PresetLabels[i]->SetText(FText::FromString(FString::Printf(TEXT("[ %s ]"), *Controller->HandTunePresetName(PresetIdx).ToUpper()))); }
	}
	if (Note) { Note->SetText(FText::GetEmpty()); }
	Pick(-1);   // nothing is being edited until a cell is clicked
	Refresh();
	SetKeyboardFocus();
}

void UHandTuneWidget::Refresh()
{
	if (!Controller) { return; }
	for (int32 i = 0; i < Cells.Num(); ++i) { PaintCell(i); }
	const int32 C = Controller->HandTuneCarry(); Highlight(TEXT("carry_low"), C == 0); Highlight(TEXT("carry_shoulder"), C == 1); Highlight(TEXT("carry_ads"), C == 2);
	if (Cells.IsValidIndex(Picked) && !CellLive(Cells[Picked])) { Picked = -1; }   // the carry changed out from under the picked cell
	if (WeaponLabel) { WeaponLabel->SetText(FText::FromString(Controller->HandTuneWeaponLabel())); }
	// The weapon's own paint, on the same terms: shown always, live only when there is a choice.
	const FString Skin = Controller->HandTuneSkinLabel();
	const bool bHasSkin = !Skin.IsEmpty();
	if (SkinButton) { SkinButton->SetVisibility(ESlateVisibility::Visible); SkinButton->SetIsEnabled(bHasSkin); SkinButton->SetRenderOpacity(bHasSkin ? 1.0f : 0.35f); }
	if (SkinLabel) { SkinLabel->SetText(FText::FromString(bHasSkin ? Skin : TEXT("[ ONE FINISH ]"))); }
	if (SkinDefaultButton) { SkinDefaultButton->SetVisibility(ESlateVisibility::Visible); SkinDefaultButton->SetIsEnabled(bHasSkin); SkinDefaultButton->SetRenderOpacity(bHasSkin ? 1.0f : 0.35f); }
	if (SkinDefaultLabel) { SkinDefaultLabel->SetColorAndOpacity(FSlateColor(Controller->HandTuneSkinIsDefault() ? Crt::Faint : Crt::Green)); }
	// SAVE says when there is something to save. The same test that guards changing weapon, so the
	// asterisk and the prompt can never disagree about whether the page is dirty.
	if (SaveLabel)
	{
		const bool bDirty = Controller->HandTuneDirty();
		SaveLabel->SetText(FText::FromString(bDirty ? TEXT("[ SAVE * ]") : TEXT("[ SAVE   ]")));
		SaveLabel->SetColorAndOpacity(FSlateColor(bDirty ? Crt::Green : Crt::DimGreen));
	}
	// The optic row: greyed and unclickable on a weapon with nowhere to mount one, rather than
	// hidden, so it is clear the choice exists and this weapon simply cannot take it.
	if (CharacterLabel) { CharacterLabel->SetText(FText::FromString(Controller->HandTuneCharacterLabel())); }

	const bool bOptic = Controller->HandTuneTakesOptic();
	// A BUILT-IN SIGHT CANNOT BE SWAPPED, but it reads and tunes like any other -- the label stays
	// lit and only the stepping is refused. Distinct from bOptic, which means the weapon has nowhere
	// to mount a sight at all and greys the whole row.
	const WeaponCatalog::FWeapon* TunedWeapon = WeaponCatalog::Find(Controller->HandTuneName);
	const bool bOpticFixed = TunedWeapon && TunedWeapon->bOpticFixed;
	const bool bCanStepOptic = bOptic && !bOpticFixed;
	if (OpticLabel)
	{
		// THE STRING IS THE CONTROLLER'S, THE STYLING IS OURS. HandTuneOpticLabel() now works out what
		// is fitted before asking about the mount, so the widget no longer rebuilds that text -- two
		// places composing one string is how a display ends up disagreeing with itself, and while the
		// widget's copy happened to match character for character, it would have silently WON if the
		// two ever diverged. What stays here is the treatment: a fitted sight with nowhere to mount it
		// is real but not adjustable, so it reads dim; faded is kept for a genuine nothing.
		const bool bFittedNoMount = !bOptic && !Controller->HT_Optic.IsEmpty();
		OpticLabel->SetText(FText::FromString(Controller->HandTuneOpticLabel() + (bOpticFixed ? TEXT(" FIXED") : TEXT(""))));
		OpticLabel->SetColorAndOpacity(FSlateColor(bOptic ? Crt::Green : bFittedNoMount ? Crt::DimGreen : Crt::Faint));
	}
	if (OpticPrev) { OpticPrev->SetIsEnabled(bCanStepOptic); OpticPrev->SetRenderOpacity(bCanStepOptic ? 1.0f : 0.35f); }
	{
		// DISABLED, NOT GONE. A control that vanishes when it has nothing to offer leaves you
		// wondering whether the page has it at all -- and it is the same question every time you
		// pick a sight with one colourway. Greyed out it still says "this sight has a paint setting,
		// and there is nothing to choose here".
		const FString Paint = Controller->HandTuneOpticSkinLabel();
		const bool bHasPaint = !Paint.IsEmpty();
		if (OpticSkinRow) { OpticSkinRow->SetVisibility(ESlateVisibility::Visible); }
		if (OpticSkinLabel)
		{
			OpticSkinLabel->SetText(FText::FromString(bHasPaint ? Paint : TEXT("[ ONE FINISH ]")));
			OpticSkinLabel->SetColorAndOpacity(FSlateColor(bHasPaint ? Crt::Green : Crt::Faint));
		}
		if (OpticSkinPrev) { OpticSkinPrev->SetIsEnabled(bHasPaint); OpticSkinPrev->SetRenderOpacity(bHasPaint ? 1.0f : 0.35f); }
		if (OpticSkinNext) { OpticSkinNext->SetIsEnabled(bHasPaint); OpticSkinNext->SetRenderOpacity(bHasPaint ? 1.0f : 0.35f); }
	}
	if (OpticNext) { OpticNext->SetIsEnabled(bCanStepOptic); OpticNext->SetRenderOpacity(bCanStepOptic ? 1.0f : 0.35f); }

	const int32 V = Controller->HandTunePresetView();
	Highlight(TEXT("view_left"), V == 0); Highlight(TEXT("view_right"), V == 1); Highlight(TEXT("view_top"), V == 2);
	Highlight(TEXT("view_front"), V == 3); Highlight(TEXT("view_quarter"), V == 4);
	const int32 A = Controller->HandTuneAim(); Highlight(TEXT("aim_high"), A == 0); Highlight(TEXT("aim_mid"), A == 1); Highlight(TEXT("aim_low"), A == 2);
	if (AimGroup) { AimGroup->SetVisibility(C == 0 ? ESlateVisibility::Collapsed : ESlateVisibility::Visible); }   // low ready has no aim
}

bool UHandTuneWidget::CellLive(const FCell& Cell) const
{
	if (!Controller) { return true; }
	// A MODE CELL IS ALWAYS THE LIVE ONE -- there is only one of it and it shows the selected mode.
	if (Cell.bActiveCol) { return true; }
	const int32 C = Controller->HandTuneCarry();   // 0 low ready, 1 shouldered, 2 sights
	switch (Cell.Row)
	{
	case 6: case 8:  return C == 2;            // hunch and lean are what getting behind the sights does
	// Rows 12-15 (the elbows, per carry and per aim) are not listed: they are mode rows now, caught
	// by the bActiveCol return above. Leaving their old per-column tests here would be dead code
	// that reads like live logic.
	case 11:         return C == 0;            // low ready's own angles off the aim
	// Row 18 (position) is NOT listed: all three of its cells are live, because they are x, y and z of
	// the carry in view rather than one axis across three carries. The old pull and lateral rows were
	// the other shape and had to pick a column; they are gone.
	default:         return true;
	}
}

// WHICH COLUMN A CELL IS SHOWING. An ordinary cell is its own column; a mode cell is whichever
// carry or aim the page has selected, resolved now rather than at build time so the buttons move it.
int32 UHandTuneWidget::CellColumn(const FCell& Cell) const
{
	if (!Cell.bActiveCol || !Controller) { return Cell.Col; }
	return FMath::Clamp(Cell.bAim ? Controller->HandTuneAim() : Controller->HandTuneCarry(), 0, 2);
}

void UHandTuneWidget::PaintCell(int32 Index)
{
	if (!Cells.IsValidIndex(Index) || !Controller) { return; }
	const FCell& Cell = Cells[Index];
	const int32 Col = CellColumn(Cell);
	const TCHAR* Axis = (Cell.bActiveCol && Cell.AxisSet) ? Cell.AxisSet[Col] : Cell.Axis;
	const float V = Controller->HandTuneValue(Cell.Row, Col);
	const FString Text = Cell.bWhole ? FString::Printf(TEXT("%s %.0f"), Axis, V) : FString::Printf(TEXT("%s %.1f"), Axis, V);
	const bool bLive = CellLive(Cell);
	if (Cell.Text) { Cell.Text->SetText(FText::FromString(Text)); Cell.Text->SetColorAndOpacity(FSlateColor(!bLive ? Crt::Faint : Index == Picked ? Crt::Green : Crt::DimGreen)); }
	if (Cell.Box) { Cell.Box->SetBrushColor(!bLive ? CellDead : Index == Picked ? CellPicked : CellGround); }
}

bool UHandTuneWidget::PaneHovered() const { return ViewPane && ViewPane->IsHovered(); }

int32 UHandTuneWidget::HoveredCell() const
{
	for (int32 i = 0; i < Cells.Num(); ++i) { if (Cells[i].Box && Cells[i].Box->IsHovered()) { return i; } }
	return -1;
}

void UHandTuneWidget::Pick(int32 Index)
{
	if (Cells.IsValidIndex(Index) && !CellLive(Cells[Index])) { return; }   // a greyed cell does nothing in this carry
	const int32 Was = Picked;
	Picked = Cells.IsValidIndex(Index) ? Index : -1;
	if (Cells.IsValidIndex(Was)) { PaintCell(Was); }
	if (Cells.IsValidIndex(Picked)) { PaintCell(Picked); }
}

void UHandTuneWidget::Adjust(int32 Index, float Notches, bool bFine, bool bCoarse)
{
	if (!Cells.IsValidIndex(Index) || !Controller || FMath::IsNearlyZero(Notches)) { return; }
	const FCell& Cell = Cells[Index];
	if (!CellLive(Cell)) { return; }
	// A notch is half a centimetre or two degrees (a whole degree on a finger); shift makes it a
	// fifth of that, ctrl five times.
	float Step = Cell.bWhole ? 1.0f : Cell.bDegrees ? 2.0f : 0.5f;
	if (bCoarse) { Step *= 5.0f; } else if (bFine) { Step *= 0.2f; }
	Controller->HandTuneAdjust(Cell.Row, CellColumn(Cell), Step * FMath::Sign(Notches) * FMath::Max(1.0f, FMath::Abs(Notches)));
	if (Note) { Note->SetText(FText::GetEmpty()); }
	PaintCell(Index);
}

FReply UHandTuneWidget::NativeOnMouseWheel(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (PaneHovered()) { return FReply::Handled(); }   // the picture does not zoom on the wheel; that belongs to the clicked cell
	// THE WHEEL FOLLOWS THE CLICK, NOT THE POINTER. Rolling over a cell on the way past must not
	// select it and must not change it: a value is edited only once it has been clicked.
	if (Picked < 0) { return Super::NativeOnMouseWheel(InGeometry, InMouseEvent); }
	Adjust(Picked, InMouseEvent.GetWheelDelta(), InMouseEvent.IsShiftDown(), InMouseEvent.IsControlDown());
	return FReply::Handled();
}

// WHICH AXIS IS PINNED. Held X, Y or Z constrains the drag to that axis of the POSITION -- the
// axes of the numbers being edited, not the world's -- so what the key says matches what the row
// says. Read from the player's key state because these keys go to the game, not to the widget.
// WHAT THE POINTER IS OVER: a hand, or the gun. The capture is orthographic, so projecting a world
// point into the picture is a dot product and a divide -- no matrices, no unprojection. Whichever
// hand is within reach of the pointer wins; otherwise the drag moves the weapon. This is why the
// two drags need no modifier key: you grab the thing you meant.
int32 UHandTuneWidget::DragTargetAt(const FVector2D& ScreenPos) const
{
	if (!Controller || !ViewPane) { return 0; }
	FVector Right, Up, CamLoc; float OrthoWidth = 0.0f;
	if (!Controller->HandTuneDragAxes(Right, Up, OrthoWidth, CamLoc)) { return 0; }
	const FGeometry& Pane = ViewPane->GetCachedGeometry();
	const FVector2D Size = Pane.GetLocalSize();
	if (Size.X < 1.0f || OrthoWidth <= KINDA_SMALL_NUMBER) { return 0; }
	const float CmPerPx = OrthoWidth / (float)Size.X;
	const FVector2D Local = Pane.AbsoluteToLocal(ScreenPos);
	FVector Main, Support; bool bHasSupport = false;
	if (!Controller->HandTuneHandPoints(Main, Support, bHasSupport)) { return 0; }
	auto ToPane = [&](const FVector& P)
	{
		const FVector Rel = P - CamLoc;
		return FVector2D((float)FVector::DotProduct(Rel, Right) / CmPerPx + Size.X * 0.5f,
		                 (float)-FVector::DotProduct(Rel, Up) / CmPerPx + Size.Y * 0.5f);
	};
	const float Reach = 30.0f;   // pixels; a hand is a small thing in a 260 cm frame
	const float DMain = (float)FVector2D::Distance(Local, ToPane(Main));
	const float DSup = bHasSupport ? (float)FVector2D::Distance(Local, ToPane(Support)) : BIG_NUMBER;
	if (DMain <= Reach && DMain <= DSup) { return 1; }
	if (DSup <= Reach) { return 2; }
	return 0;
}

int32 UHandTuneWidget::DragAxisHeld() const
{
	if (const APlayerController* PC = GetOwningPlayer())
	{
		if (PC->IsInputKeyDown(EKeys::X)) { return 0; }
		if (PC->IsInputKeyDown(EKeys::Y)) { return 1; }
		if (PC->IsInputKeyDown(EKeys::Z)) { return 2; }
	}
	return -1;
}

FReply UHandTuneWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	const int32 Target = HoveredCell();
	if (Target >= 0) { Pick(Target); }
	// LEFT DRAGS THE WEAPON, RIGHT LOOKS ROUND IT. Dragging used to orbit, which is the gesture
	// anyone reaches for first to MOVE the thing they are looking at -- so the obvious action did
	// the other job and the wanted one did not exist. The look-round is not lost, it moved to the
	// right button, and it still springs back on release.
	bDragging = PaneHovered() && InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton;
	if (bDragging)
	{
		// WHAT WAS GRABBED is decided once, on the press, and held for the whole drag. Deciding it
		// per move would hand the drag to the other thing the moment the pointer crossed it.
		DragTarget = DragTargetAt(InMouseEvent.GetScreenSpacePosition());
		if (Controller) { Controller->SetDiagNoteTimed(DragTarget == 1 ? TEXT("DRAGGING THE MAIN HAND ALONG THE WEAPON") : DragTarget == 2 ? TEXT("DRAGGING THE SUPPORT HAND ALONG THE WEAPON") : TEXT("DRAGGING THE WEAPON -- grab a hand to move that instead"), 3.0f); }
	}
	bOrbiting = PaneHovered() && InMouseEvent.GetEffectingButton() == EKeys::RightMouseButton;
	DragFrom = InMouseEvent.GetScreenSpacePosition();
	SetKeyboardFocus();
	// Capturing the mouse keeps the drag alive if the pointer runs off the picture.
	return (bDragging || bOrbiting) ? FReply::Handled().CaptureMouse(TakeWidget()) : FReply::Handled();
}

FReply UHandTuneWidget::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (!Controller || (!bDragging && !bOrbiting)) { return Super::NativeOnMouseMove(InGeometry, InMouseEvent); }
	const FVector2D Now = InMouseEvent.GetScreenSpacePosition();
	if (bOrbiting)
	{
		const FVector2D D = Now - DragFrom;
		DragFrom = Now;
		// A quarter of a degree a pixel: the whole forty degrees is a good sweep of the hand, and
		// dragging right swings the camera right round the weapon, the way a turntable reads.
		Controller->HandTuneOrbit((float)D.X * 0.25f, (float)-D.Y * 0.25f);
		return FReply::Handled();
	}
	// THE PIXELS ARE CENTIMETRES, EXACTLY. The capture is orthographic, so cm-per-pixel is the ortho
	// width over the pane's own width -- no unprojection, no guessed sensitivity, and it stays right
	// when the zoom changes. Measured in the pane's LOCAL space so DPI scaling cannot skew it.
	FVector Right, Up; float OrthoWidth = 0.0f;
	const FGeometry& Pane = ViewPane ? ViewPane->GetCachedGeometry() : InGeometry;
	const float PaneW = (float)Pane.GetLocalSize().X;
	FVector CamLoc;
	if (PaneW > 1.0f && Controller->HandTuneDragAxes(Right, Up, OrthoWidth, CamLoc))
	{
		const FVector2D D = Pane.AbsoluteToLocal(Now) - Pane.AbsoluteToLocal(DragFrom);
		const float CmPerPx = OrthoWidth / PaneW;
		const FVector WorldDelta = Right * (float)D.X * CmPerPx + Up * (float)-D.Y * CmPerPx;
		if (DragTarget == 1 || DragTarget == 2) { Controller->HandTuneDragGrip(WorldDelta, DragTarget == 2, DragAxisHeld()); }
		else { Controller->HandTuneDragPosition(WorldDelta, DragAxisHeld()); }
		Refresh();
	}
	DragFrom = Now;
	return FReply::Handled();
}

FReply UHandTuneWidget::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	// Let go of a LOOK and the view is the framed one again: that is a look round, not a camera to
	// park. Let go of a DRAG and nothing springs back -- the weapon stays where it was put.
	if (bOrbiting && Controller) { Controller->HandTuneResetView(); }
	bDragging = false; bOrbiting = false;
	return FReply::Handled().ReleaseMouseCapture();
}

void UHandTuneWidget::NativeOnMouseLeave(const FPointerEvent& InMouseEvent)
{
	if (Controller && !bOrbiting) { Controller->HandTuneResetView(); }   // the angle springs back; the zoom stays where it was put
	Super::NativeOnMouseLeave(InMouseEvent);
}
FReply UHandTuneWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Escape) { OnBack(); return FReply::Handled(); }
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

FReply UHandTuneWidget::NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Escape) { OnBack(); return FReply::Handled(); }
	return Super::NativeOnPreviewKeyDown(InGeometry, InKeyEvent);
}

void UHandTuneWidget::Preset(int32 Index, bool bSupport)
{
	if (Controller) { Controller->HandTuneApplyPreset(bSupport, Index); }
	Refresh();
	if (Note) { Note->SetText(FText::FromString(FString::Printf(TEXT("%s hand: %s"), bSupport ? TEXT("support") : TEXT("main"), *Controller->HandTunePresetName(Index)))); }
	SetKeyboardFocus();
}
void UHandTuneWidget::OnPresetR0() { Preset(0, false); }
void UHandTuneWidget::OnPresetR1() { Preset(1, false); }
void UHandTuneWidget::OnPresetR2() { Preset(2, false); }
void UHandTuneWidget::OnPresetR3() { Preset(3, false); }
void UHandTuneWidget::OnPresetR4() { Preset(4, false); }
void UHandTuneWidget::OnPresetL0() { Preset(0, true); }
void UHandTuneWidget::OnPresetL1() { Preset(1, true); }
void UHandTuneWidget::OnPresetL2() { Preset(2, true); }
void UHandTuneWidget::OnPresetL3() { Preset(3, true); }
void UHandTuneWidget::OnPresetL4() { Preset(4, true); }

void UHandTuneWidget::OnZoomIn() { if (Controller) { Controller->HandTuneZoomStep(1); } SetKeyboardFocus(); }
void UHandTuneWidget::OnZoomOut() { if (Controller) { Controller->HandTuneZoomStep(-1); } SetKeyboardFocus(); }
void UHandTuneWidget::OnViewLeft() { if (Controller) { Controller->HandTuneSetPresetView(0); } Refresh(); SetKeyboardFocus(); }
void UHandTuneWidget::OnViewRight() { if (Controller) { Controller->HandTuneSetPresetView(1); } Refresh(); SetKeyboardFocus(); }
void UHandTuneWidget::OnViewTop() { if (Controller) { Controller->HandTuneSetPresetView(2); } Refresh(); SetKeyboardFocus(); }
void UHandTuneWidget::OnViewFront() { if (Controller) { Controller->HandTuneSetPresetView(3); } Refresh(); SetKeyboardFocus(); }
void UHandTuneWidget::OnViewQuarter() { if (Controller) { Controller->HandTuneSetPresetView(4); } Refresh(); SetKeyboardFocus(); }
void UHandTuneWidget::OnCarryLow() { if (Controller) { Controller->HandTuneSetCarry(0); } Refresh(); SetKeyboardFocus(); }
void UHandTuneWidget::OnCarryShoulder() { if (Controller) { Controller->HandTuneSetCarry(1); } Refresh(); SetKeyboardFocus(); }
void UHandTuneWidget::OnCarryAds() { if (Controller) { Controller->HandTuneSetCarry(2); } Refresh(); SetKeyboardFocus(); }
void UHandTuneWidget::OnAimHigh() { if (Controller) { Controller->HandTuneSetAim(0); } Refresh(); SetKeyboardFocus(); }
void UHandTuneWidget::OnAimMid() { if (Controller) { Controller->HandTuneSetAim(1); } Refresh(); SetKeyboardFocus(); }
void UHandTuneWidget::OnAimLow() { if (Controller) { Controller->HandTuneSetAim(2); } Refresh(); SetKeyboardFocus(); }
UHorizontalBox* UHandTuneWidget::Selector(const TCHAR* Caption, TObjectPtr<UTextBlock>& OutLabel, UWidget*& OutPrev, UWidget*& OutNext)
{
	const FSheetSpec& S = FSheetSpec::Get();
	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	USizeBox* CapBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	CapBox->SetWidthOverride(64.0f);
	CapBox->AddChild(Crt::FixedText(WidgetTree, Caption, S.CaptionSize, Crt::Faint));
	Row->AddChildToHorizontalBox(CapBox)->SetVerticalAlignment(VAlign_Center);
	UButton* Prev = Crt::Button(WidgetTree, TEXT("[ < ]"), S.CaptionSize, Crt::DimGreen);
	Row->AddChildToHorizontalBox(Prev)->SetVerticalAlignment(VAlign_Center);
	// A fixed width, so stepping through names of different lengths does not make the arrows dance.
	USizeBox* LabBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	LabBox->SetWidthOverride(420.0f);   // the longest optic name plus its qualifier, without the arrows dancing
	OutLabel = Crt::FixedText(WidgetTree, TEXT(""), S.CaptionSize, Crt::Green);
	OutLabel->SetJustification(ETextJustify::Center);
	LabBox->AddChild(OutLabel);
	Row->AddChildToHorizontalBox(LabBox)->SetVerticalAlignment(VAlign_Center);
	UButton* Next = Crt::Button(WidgetTree, TEXT("[ > ]"), S.CaptionSize, Crt::DimGreen);
	Row->AddChildToHorizontalBox(Next)->SetVerticalAlignment(VAlign_Center);
	OutPrev = Prev; OutNext = Next;
	return Row;
}

void UHandTuneWidget::StepWeaponGuarded(int32 Dir)
{
	if (!Controller) { return; }
	// UNSAVED WORK IS NOT THROWN AWAY BY A STRAY ARROW PRESS. Same three-way prompt the character
	// pages use when they are left dirty: save it, discard it, or stay where you are.
	if (Controller->HandTuneDirty())
	{
		Controller->ConfirmHandTuneLeave(Dir);
		return;
	}
	Controller->HandTuneStepWeapon(Dir);
	Refresh();
	SetKeyboardFocus();
}

void UHandTuneWidget::OnPrevSkin()
{
	if (Controller) { Controller->HandTuneStepSkin(-1); }
	Refresh();
	SetKeyboardFocus();
}

void UHandTuneWidget::OnPrevOpticSkin() { if (Controller) { Controller->HandTuneStepOpticSkin(-1); } Refresh(); SetKeyboardFocus(); }
void UHandTuneWidget::OnNextOpticSkin() { if (Controller) { Controller->HandTuneStepOpticSkin(1); } Refresh(); SetKeyboardFocus(); }


void UHandTuneWidget::OnPrevCharacter()
{
	if (Controller) { Controller->HandTuneStepCharacter(-1); }
	Refresh();
	SetKeyboardFocus();
}
void UHandTuneWidget::OnNextCharacter()
{
	if (Controller) { Controller->HandTuneStepCharacter(1); }
	Refresh();
	SetKeyboardFocus();
}

void UHandTuneWidget::OnPrevOptic()
{
	if (Controller) { Controller->HandTuneStepOptic(-1); }
	Refresh();
	SetKeyboardFocus();
}

void UHandTuneWidget::OnNextOptic()
{
	if (Controller) { Controller->HandTuneStepOptic(1); }
	Refresh();
	SetKeyboardFocus();
}

void UHandTuneWidget::OnPrevWeapon() { StepWeaponGuarded(-1); }

void UHandTuneWidget::OnNextWeapon() { StepWeaponGuarded(1); }

void UHandTuneWidget::OnCycleSkin()
{
	if (Controller) { Controller->HandTuneCycleSkin(); }
	Refresh();
	SetKeyboardFocus();
}

void UHandTuneWidget::OnSetDefaultSkin()
{
	if (Controller) { Controller->HandTuneSetDefaultSkin(); }
	Refresh();
	SetKeyboardFocus();
}

void UHandTuneWidget::OnReset() { if (Controller) { Controller->HandTuneReset(); } Refresh(); if (Note) { Note->SetText(FText::FromString(TEXT("back to what the catalogue has"))); } SetKeyboardFocus(); }

void UHandTuneWidget::OnSave()
{
	const bool bOk = Controller && Controller->HandTuneSave();
	Refresh();
	if (Note) { Note->SetText(FText::FromString(bOk ? TEXT("SAVED TO UI/WEAPONS.JSON: grip, hand_rot, fingers_r, fore_grip, fore_hand_rot, fingers_l, hunch, pull -- the player holds it so now") : TEXT("SAVE FAILED"))); }
	SetKeyboardFocus();
}

void UHandTuneWidget::OnBack() { OnClose.ExecuteIfBound(); }
