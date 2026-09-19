#include "UI/CharacterSheetWidget.h"
#include "UI/PaneShape.h"
#include "Framework/Application/SlateApplication.h"
#include "UI/InventoryGridWidget.h"
#include "UI/CrtRuleWidget.h"
#include "UI/CrtTabsWidget.h"
#include "Items/ItemCatalog.h"
#include "Weapons/WeaponCatalog.h"
#include "Weapons/WeaponSkins.h"
#include "Components/ButtonSlot.h"
#include "UI/SheetSpec.h"
#include "Characters/BaseCharacter.h"
#include "Core/BasePlayerController.h"
#include "UI/CrtStyle.h"
#include "UI/ContextMenuWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Blueprint/SlateBlueprintLibrary.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/ScaleBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/TextureRenderTarget2D.h"

void UCharacterSheetWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	UBorder* Root = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Root"));
	Root->SetBrushColor(Crt::Panel);
	Root->SetPadding(FMargin(34.0f, 26.0f));
	WidgetTree->RootWidget = Root;
	Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Column"));
	// The column under a canvas that carries the drag picture; the canvas never takes the mouse.
	UOverlay* Over = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("SheetOverlay"));
	Root->SetContent(Over);
	if (UOverlaySlot* CS = Over->AddChildToOverlay(Column)) { CS->SetHorizontalAlignment(HAlign_Fill); CS->SetVerticalAlignment(VAlign_Fill); }
	DragLayer = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("DragLayer"));
	DragLayer->SetVisibility(ESlateVisibility::HitTestInvisible);
	if (UOverlaySlot* DS = Over->AddChildToOverlay(DragLayer)) { DS->SetHorizontalAlignment(HAlign_Fill); DS->SetVerticalAlignment(VAlign_Fill); }
	DragImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("DragImage"));
	DragImage->SetVisibility(ESlateVisibility::Collapsed);
	DragImage->SetRenderOpacity(0.85f);
	DragLayer->AddChild(DragImage);
	Rebuild();
}

UVerticalBoxSlot* UCharacterSheetWidget::AddRow(UVerticalBox* Box, const FString& Text, int32 Size, const FLinearColor& Color)
{
	int32 Dummy;
	if (!Text.IsEmpty() && RuleChars.FindChar(Text[Text.Len() - 1], Dummy)) { return Box->AddChildToVerticalBox(UCrtRuleWidget::Make(GetOwningPlayer(), Text, Size, Color)); }
	return Box->AddChildToVerticalBox(Crt::FixedText(WidgetTree, Text, Size, Color));
}

// Which sections are folded, by the name between the caption's brackets; kept for the session.
static TSet<FString> GSheetFolded;
static FString SectionKeyOf(const FString& Caption)
{
	const int32 A = Caption.Find(TEXT("[")), B = Caption.Find(TEXT("]"));
	return (A != INDEX_NONE && B != INDEX_NONE && B > A) ? Caption.Mid(A + 1, B - A - 1).TrimStartAndEnd() : Caption;
}
static FString FoldCaption(const FString& Caption, bool bOpen)
{
	const int32 A = Caption.Find(TEXT("[")), B = Caption.Find(TEXT("]"));
	if (A == INDEX_NONE || B == INDEX_NONE || B < A) { return Caption; }
	const FString Inner = Caption.Mid(A, B - A + 1);
	return bOpen ? TEXT("--") + Inner + TEXT("-") : TEXT("==") + Inner + TEXT("=");
}

void UCharacterSheetSectionBinding::OnClicked() { if (Sheet) { Sheet->ToggleSection(Key); } }

UVerticalBox* UCharacterSheetWidget::BeginSection(UVerticalBox* Into, const FString& Caption, const FMargin& Pad, UWidget* HeaderRight)
{
	const FSheetSpec& S = FSheetSpec::Get();
	const FString Key = SectionKeyOf(Caption);
	const bool bOpen = !GSheetFolded.Contains(Key);
	UButton* B = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
	B->SetStyle(Crt::ButtonStyle());
	B->SetContent(UCrtRuleWidget::Make(GetOwningPlayer(), FoldCaption(Caption, bOpen), S.CaptionSize, Crt::DimGreen));
	if (UButtonSlot* BS = Cast<UButtonSlot>(B->GetContent()->Slot)) { BS->SetPadding(FMargin(0.0f)); BS->SetHorizontalAlignment(HAlign_Fill); }
	UCharacterSheetSectionBinding* Binding = NewObject<UCharacterSheetSectionBinding>(this);
	Binding->Sheet = this; Binding->Key = Key;
	B->OnClicked.AddDynamic(Binding, &UCharacterSheetSectionBinding::OnClicked);
	SectionBindings.Add(Binding);
	UWidget* Header = B;
	if (HeaderRight)
	{
		UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		UHorizontalBoxSlot* HS = Row->AddChildToHorizontalBox(B); HS->SetSize(ESlateSizeRule::Fill); HS->SetVerticalAlignment(VAlign_Center);
		Row->AddChildToHorizontalBox(HeaderRight)->SetPadding(FMargin(10, 0, 0, 0));
		Header = Row;
	}
	Into->AddChildToVerticalBox(Header)->SetPadding(Pad);
	UVerticalBox* BodyBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	BodyBox->SetVisibility(bOpen ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	Into->AddChildToVerticalBox(BodyBox);
	SectionBodies.Add(Key, BodyBox); SectionButtons.Add(Key, B); SectionCaptions.Add(Key, Caption);
	return BodyBox;
}

void UCharacterSheetWidget::ToggleSection(const FString& Key)
{
	if (GSheetFolded.Contains(Key)) { GSheetFolded.Remove(Key); } else { GSheetFolded.Add(Key); }
	const bool bOpen = !GSheetFolded.Contains(Key);
	if (TObjectPtr<UWidget>* Found = SectionBodies.Find(Key)) { if (*Found) { (*Found)->SetVisibility(bOpen ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed); } }
	if (TObjectPtr<UButton>* B = SectionButtons.Find(Key))
	{
		if (*B)
		{
			(*B)->SetContent(UCrtRuleWidget::Make(GetOwningPlayer(), FoldCaption(SectionCaptions.FindRef(Key), bOpen), FSheetSpec::Get().CaptionSize, Crt::DimGreen));
			if (UButtonSlot* BS = Cast<UButtonSlot>((*B)->GetContent()->Slot)) { BS->SetPadding(FMargin(0.0f)); BS->SetHorizontalAlignment(HAlign_Fill); }
		}
	}
}

void UCharacterSheetWidget::Rebuild()
{
	if (!Column) { return; }
	const FSheetSpec& S = FSheetSpec::Get();
	FaceClickFraction = S.FaceClickFraction;
	RuleChars = S.RuleChars.IsEmpty() ? TEXT("-=") : S.RuleChars;
	Column->ClearChildren();
	SectionBodies.Reset(); SectionButtons.Reset(); SectionCaptions.Reset(); SectionBindings.Reset();
	// Hidden while it settles. Two ticks was not enough: the fixed-width font is built at
	// runtime and its glyphs measure in over the first few frames, which read as the text
	// typing itself in and the columns sliding right to make room. The widths below no longer
	// depend on any measurement, and the settle covers the atlas.
	SettleTicks = 4;
	SetRenderOpacity(0.0f);
	StatRowTexts.Reset(); StatRowTemplates.Reset();
	Feed = nullptr; Tabs = nullptr; InventoryCount = nullptr; InventoryGrid = nullptr; GearGrid = nullptr; InfoName = nullptr; InfoText = nullptr;

	// The top rule with the unit's name set into it, running to the panel's edge.
	UHorizontalBox* Header = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	Header->AddChildToHorizontalBox(Crt::FixedText(WidgetTree, S.HeaderLeft, S.RuleSize, Crt::Faint))->SetVerticalAlignment(VAlign_Center);
	Tabs = UCrtTabsWidget::Make(GetOwningPlayer(), UCrtTabsWidget::StandardTabs(OwnerController ? OwnerController->GetPlayerDisplayName() : FString()), UCrtTabsWidget::TabSheet, S.NameSize);
	Tabs->OnTab.BindUObject(this, &UCharacterSheetWidget::OnTab);
	Header->AddChildToHorizontalBox(Tabs)->SetVerticalAlignment(VAlign_Center);
	UHorizontalBoxSlot* RuleSlot = Header->AddChildToHorizontalBox(UCrtRuleWidget::Make(GetOwningPlayer(), S.HeaderRight, S.RuleSize, Crt::Faint));
	RuleSlot->SetSize(ESlateSizeRule::Fill); RuleSlot->SetVerticalAlignment(VAlign_Center);
	if (!S.HeaderEnd.IsEmpty()) { Header->AddChildToHorizontalBox(Crt::FixedText(WidgetTree, S.HeaderEnd, S.RuleSize, Crt::Faint))->SetVerticalAlignment(VAlign_Center); }
	if (!S.Hint.IsEmpty()) { Header->AddChildToHorizontalBox(Crt::FixedText(WidgetTree, S.Hint, S.RowSize, Crt::DimGreen))->SetVerticalAlignment(VAlign_Center); }
	UButton* Close = Crt::Button(WidgetTree, TEXT("[ X ]"), S.NameSize, Crt::Green);
	Close->OnClicked.AddDynamic(this, &UCharacterSheetWidget::OnClose);
	Header->AddChildToHorizontalBox(Close)->SetPadding(FMargin(12, 0, 0, 0));
	Column->AddChildToVerticalBox(Header)->SetPadding(FMargin(0, 0, 0, 12));

	// Three columns: stats, the mirror, inventory and gear.
	Body = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	Column->AddChildToVerticalBox(Body)->SetSize(ESlateSizeRule::Fill);

	UVerticalBox* Stats = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	for (const FString& Line : S.StatsArt) { Stats->AddChildToVerticalBox(Crt::FixedText(WidgetTree, Line, S.ArtSize, Crt::DimGreen)); }
	if (S.StatsArt.Num() > 0) { Stats->AddChildToVerticalBox(Crt::FixedText(WidgetTree, TEXT(""), S.RowSize, Crt::DimGreen))->SetPadding(FMargin(0, 4)); }
	for (int32 SectionIndex = 0; SectionIndex < S.StatSections.Num(); ++SectionIndex)
	{
		const FSheetStatSection& Section = S.StatSections[SectionIndex];
		// Space above each caption but the first, so the blocks read as separate without a rule.
		UVerticalBox* SectionBody = BeginSection(Stats, Section.Caption, FMargin(0, SectionIndex == 0 ? 0.0f : 14.0f, 0, 8));
		for (const FString& Row : Section.Rows)
		{
			UVerticalBoxSlot* RowSlot = AddRow(SectionBody, Row, S.RowSize, Crt::DimGreen);
			RowSlot->SetPadding(FMargin(0, 1));
			// A row with a token keeps its template so Refresh can fill the number in.
			if (Row.Contains(TEXT("{"))) { if (UTextBlock* T = Cast<UTextBlock>(RowSlot->Content)) { StatRowTexts.Add(T); StatRowTemplates.Add(Row); } }
		}
	}
	UHorizontalBoxSlot* StatsSlot = Body->AddChildToHorizontalBox(Stats);
	// The stats column goes in a size box with NO width yet. Fixing widths from the viewport was
	// wrong twice over: the viewport is wider than the panel, and the DPI scale differs between
	// the editor and a window. The box is filled by weight until the panel has laid out once;
	// then NativeTick reads the panel's real width and pins all three columns from THAT, and
	// nothing measures text after that point.
	StatsBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	StatsBox->AddChild(Stats);
	Body->RemoveChild(Stats);
	StatsSlot = Body->AddChildToHorizontalBox(StatsBox);
	{ FSlateChildSize Sz(ESlateSizeRule::Fill); Sz.Value = S.StatsWeight; StatsSlot->SetSize(Sz); }
	StatsSlot->SetPadding(FMargin(0, 0, S.ColumnGap * 0.5f, 0));
	bColumnsFitted = false;
	// A vertical rule between the columns, a column of the divider glyph down the body's height.
	auto Divide = [&]() { if (S.Divider.IsEmpty()) { return; } UHorizontalBoxSlot* D = Body->AddChildToHorizontalBox(UCrtRuleWidget::MakeVertical(GetOwningPlayer(), S.Divider, S.RuleSize, Crt::Faint)); D->SetVerticalAlignment(VAlign_Fill); D->SetPadding(FMargin(0, 0, S.ColumnGap * 0.5f, 0)); };
	Divide();

	UVerticalBox* Middle = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	UVerticalBox* MirrorBody = S.MirrorCaption.IsEmpty() ? Middle : BeginSection(Middle, S.MirrorCaption, FMargin(0, 0, 0, 8));
	// A box held to the portrait's 5:8 aspect: it lays out in one pass, where a scale box needs a
	// frame to find its size and the columns visibly shift while it does.
	USizeBox* Fit = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	MirrorFit = Fit;
	Fit->SetMinAspectRatio(0.625f); Fit->SetMaxAspectRatio(0.625f);
	Feed = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
	Feed->SetColorAndOpacity(FLinearColor::White);
	Fit->AddChild(Feed);
	UVerticalBoxSlot* FitSlot = MirrorBody->AddChildToVerticalBox(Fit);
	// At the TOP of the column, its own height (5:8 of the mirror's width, set with the width in
	// NativeTick): a fill slot stretched the box to the column and centred the picture in it.
	FitSlot->SetSize(ESlateSizeRule::Automatic); FitSlot->SetHorizontalAlignment(HAlign_Fill); FitSlot->SetVerticalAlignment(VAlign_Top);
	Fit->SetHeightOverride(FMath::Floor(PaneShape::HeightFor(PaneShape::Mirror, MirrorWidth)));
	if (!S.MirrorFooter.IsEmpty()) { AddRow(MirrorBody, S.MirrorFooter, S.CaptionSize, Crt::DimGreen)->SetPadding(FMargin(0, 8, 0, 0)); }
	// A fixed width, so nothing about this column depends on the rest of the page settling. The
	// first guess is from the viewport; NativeTick replaces it with the body's real width.
	MirrorBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	const float Scale = FMath::Max(0.01f, UWidgetLayoutLibrary::GetViewportScale(this));
	const float Inner = UWidgetLayoutLibrary::GetViewportSize(this).X / Scale - S.Panel.Left - S.Panel.Right - 68.0f;
	MirrorWidth = S.MirrorWidthFor(Inner);
	MirrorBox->SetWidthOverride(MirrorWidth);
	MirrorBox->AddChild(Middle);
	UHorizontalBoxSlot* MiddleSlot = Body->AddChildToHorizontalBox(MirrorBox);
	MiddleSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic)); MiddleSlot->SetPadding(FMargin(0, 0, S.ColumnGap * 0.5f, 0)); MiddleSlot->SetVerticalAlignment(VAlign_Fill);
	Divide();

	UVerticalBox* Right = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	// Equipped gear from the spec's slot table: equipment squares unlabelled, body slots captioned beside/above/below.
	// The quickbar first, at the top of the column; the gear below it.
	// The QUICKBAR: ten squares captioned 1 to 0, the keys. Squares 1 and 2 show the weapons
	// the keys 1 and 2 draw; the others are empty until the binding UI arrives.
	UVerticalBox* QuickBody = BeginSection(Right, S.QuickbarCaption, FMargin(0, 0, 0, 8));
	QuickGrid = CreateWidget<UInventoryGridWidget>(GetOwningPlayer(), UInventoryGridWidget::StaticClass());
	QuickGrid->SetGap(S.InventoryGap);
	{
		TArray<FIntPoint> Cells;
		for (int32 k = 0; k < 10; ++k) { Cells.Add(FIntPoint(1, k)); }
		QuickGrid->ConfigureCells(TEXT(""), Cells, S.GearCell);
		for (int32 k = 0; k < 10; ++k) { QuickGrid->AddCaption(0, k, FString::Printf(TEXT("%d"), (k + 1) % 10), 2); }
	}
	QuickGrid->OnDragBegan.BindUObject(this, &UCharacterSheetWidget::OnDragBegan); QuickGrid->OnDragEnded.BindUObject(this, &UCharacterSheetWidget::OnDragEnded); QuickGrid->OnDropRefused.BindUObject(this, &UCharacterSheetWidget::OnDropRefused);
	QuickGrid->SetGridId(2); QuickGrid->OnCanDrop.BindUObject(this, &UCharacterSheetWidget::CanDrop); QuickGrid->OnDropped.BindUObject(this, &UCharacterSheetWidget::OnDropped);
	QuickGrid->OnSlotClicked.BindUObject(this, &UCharacterSheetWidget::OnQuickSlot);
	QuickGrid->OnSlotHovered.BindUObject(this, &UCharacterSheetWidget::OnQuickHover);
	QuickBody->AddChildToVerticalBox(QuickGrid)->SetHorizontalAlignment(HAlign_Left);

	UVerticalBox* GearBody = S.GearCaption.IsEmpty() ? Right : BeginSection(Right, S.GearCaption, FMargin(0, 18, 0, 8));
	GearGrid = CreateWidget<UInventoryGridWidget>(GetOwningPlayer(), UInventoryGridWidget::StaticClass());
	GearGrid->SetGap(S.InventoryGap);
	{
		TArray<FIntPoint> Cells;
		for (const FSheetGearSlot& G : S.Slots) { Cells.Add(FIntPoint(G.Row, G.Col)); }
		GearGrid->ConfigureCells(TEXT(""), Cells, S.GearCell);
		for (const FIntVector& Sp : S.Spacers) { GearGrid->AddSpacer(Sp.X, Sp.Y, Sp.Z); }
		for (int32 i = 0; i < S.Slots.Num(); ++i)
		{
			const FSheetGearSlot& G = S.Slots[i];
			if (!G.bEnabled) { GearGrid->SetSlotEnabled(i, false); }
			if (G.Label == TEXT("left")) { GearGrid->AddCaption(G.Row, G.Col - 1, G.Name, 1); }
			else if (G.Label == TEXT("right")) { GearGrid->AddCaption(G.Row, G.Col + 1, G.Name, 0); }
			else if (G.Label == TEXT("above")) { GearGrid->AddCaption(G.Row - 1, G.Col, G.Name, 2); }
			else if (G.Label == TEXT("below")) { GearGrid->AddCaption(G.Row + 1, G.Col, G.Name, 2); }
		}
	}
	GearGrid->OnDragBegan.BindUObject(this, &UCharacterSheetWidget::OnDragBegan); GearGrid->OnDragEnded.BindUObject(this, &UCharacterSheetWidget::OnDragEnded); GearGrid->OnDropRefused.BindUObject(this, &UCharacterSheetWidget::OnDropRefused);
	GearGrid->SetGridId(1); GearGrid->OnCanDrop.BindUObject(this, &UCharacterSheetWidget::CanDrop); GearGrid->OnDropped.BindUObject(this, &UCharacterSheetWidget::OnDropped);
	GearGrid->OnSlotClicked.BindUObject(this, &UCharacterSheetWidget::OnGearSlot);
	GearGrid->OnSlotHovered.BindUObject(this, &UCharacterSheetWidget::OnGearHover);
	GearGrid->OnSlotRightClicked.BindUObject(this, &UCharacterSheetWidget::OnGearRightClick);
	GearBody->AddChildToVerticalBox(GearGrid)->SetHorizontalAlignment(HAlign_Left);
	UVerticalBox* InvBody = Right;
	if (!S.InventoryCaption.IsEmpty())
	{
		// The caption rule is the fold button, with how full the bag is at the right end of it.
		InventoryCount = Crt::FixedText(WidgetTree, TEXT(""), S.CaptionSize, Crt::DimGreen, ETextJustify::Right);
		InvBody = BeginSection(Right, S.InventoryCaption, FMargin(0, 18, 0, 8), InventoryCount);
	}
	InventoryGrid = UInventoryGridWidget::MakeBag(GetOwningPlayer(), ABasePlayerController::InventoryCapacity);
	InventoryGrid->OnDragBegan.BindUObject(this, &UCharacterSheetWidget::OnDragBegan); InventoryGrid->OnDragEnded.BindUObject(this, &UCharacterSheetWidget::OnDragEnded); InventoryGrid->OnDropRefused.BindUObject(this, &UCharacterSheetWidget::OnDropRefused);
	InventoryGrid->SetGridId(0); InventoryGrid->OnCanDrop.BindUObject(this, &UCharacterSheetWidget::CanDrop); InventoryGrid->OnDropped.BindUObject(this, &UCharacterSheetWidget::OnDropped);
	InventoryGrid->OnSlotClicked.BindUObject(this, &UCharacterSheetWidget::OnInventorySlot);
	InventoryGrid->OnSlotHovered.BindUObject(this, &UCharacterSheetWidget::OnInventoryHover);
	InventoryGrid->OnSlotRightClicked.BindUObject(this, &UCharacterSheetWidget::OnInventoryRightClick);
	InvBody->AddChildToVerticalBox(InventoryGrid)->SetHorizontalAlignment(HAlign_Left);


	// Info for whatever is hovered.
	// At the right end of the INFO header: a link to the shown item's card in the Reference.
	InfoLink = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
	InfoLink->SetStyle(Crt::ButtonStyle());
	{
		UTextBlock* LinkText = Crt::FixedText(WidgetTree, TEXT("[ REFERENCE > ]"), S.CaptionSize, Crt::Green);
		InfoLink->AddChild(LinkText);
		if (UButtonSlot* LS = Cast<UButtonSlot>(LinkText->Slot)) { LS->SetPadding(FMargin(6.0f, 1.0f)); }
	}
	InfoLink->OnClicked.AddDynamic(this, &UCharacterSheetWidget::OnInfoLink);
	InfoLink->SetVisibility(ESlateVisibility::Collapsed);
	UVerticalBox* InfoBody = S.InfoCaption.IsEmpty() ? Right : BeginSection(Right, S.InfoCaption, FMargin(0, 18, 0, 8), InfoLink);
	InfoName = Crt::FixedText(WidgetTree, TEXT(""), S.InfoNameSize, Crt::Green);
	{
		// The title row: the name on the left and, for a weapon its pack paints more than one
		// way, a SKIN button at the right end that steps through the variants. Hidden otherwise.
		UHorizontalBox* TitleRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		UHorizontalBoxSlot* NS = TitleRow->AddChildToHorizontalBox(InfoName); NS->SetSize(FSlateChildSize(ESlateSizeRule::Fill)); NS->SetVerticalAlignment(VAlign_Center);
		SkinButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
		SkinButton->SetStyle(Crt::ButtonStyle());
		SkinLabel = Crt::FixedText(WidgetTree, TEXT("[ SKIN ]"), S.CaptionSize, Crt::Green);
		SkinButton->AddChild(SkinLabel);
		if (UButtonSlot* BS = Cast<UButtonSlot>(SkinLabel->Slot)) { BS->SetPadding(FMargin(6.0f, 2.0f)); }
		SkinButton->OnClicked.AddDynamic(this, &UCharacterSheetWidget::OnSkin);
		SkinButton->SetVisibility(ESlateVisibility::Collapsed);
		UHorizontalBoxSlot* SS = TitleRow->AddChildToHorizontalBox(SkinButton); SS->SetHorizontalAlignment(HAlign_Right); SS->SetVerticalAlignment(VAlign_Center); SS->SetPadding(FMargin(8, 0, 0, 0));
		InfoBody->AddChildToVerticalBox(TitleRow);
	}
	InfoText = Crt::FixedText(WidgetTree, TEXT(""), S.InfoTextSize, Crt::DimGreen);
	InfoText->SetAutoWrapText(true);
	InfoBody->AddChildToVerticalBox(InfoText)->SetPadding(FMargin(0, 3, 0, 0));
	// Weight 0 in the spec means the right column HUGS its grids: automatic, never squeezed and
	// never stretched. The grids are size boxes, so that width is known before any text has
	// measured, and the left columns take everything else.
	RightBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	RightBox->AddChild(Right);
	UHorizontalBoxSlot* RightSlot = Body->AddChildToHorizontalBox(RightBox);
	if (S.RightWeight > 0.0f) { FSlateChildSize Sz(ESlateSizeRule::Fill); Sz.Value = S.RightWeight; RightSlot->SetSize(Sz); }
	else { RightSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic)); }

	// The closing rule across the bottom.
	if (!S.FooterLeft.IsEmpty() || !S.FooterEnd.IsEmpty())
	{
		UHorizontalBox* Footer = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		UHorizontalBoxSlot* FootRule = Footer->AddChildToHorizontalBox(UCrtRuleWidget::Make(GetOwningPlayer(), S.FooterLeft, S.RuleSize, Crt::Faint));
		FootRule->SetSize(ESlateSizeRule::Fill); FootRule->SetVerticalAlignment(VAlign_Center);
		if (!S.FooterEnd.IsEmpty()) { Footer->AddChildToHorizontalBox(Crt::FixedText(WidgetTree, S.FooterEnd, S.RuleSize, Crt::Faint))->SetVerticalAlignment(VAlign_Center); }
		Column->AddChildToVerticalBox(Footer)->SetPadding(FMargin(0, 12, 0, 0));
	}
}

void UCharacterSheetWidget::Refresh()
{
	if (!InventoryGrid || !GearGrid) { return; }
	// The name the player chose (their pawn's display name, falling back to the saved Player.json).
	FString Name = OwnerController ? OwnerController->GetPlayerDisplayName() : FString();
	if (Name.IsEmpty()) { Name = TEXT("Repli Can"); }
	if (Tabs) { Tabs->SetLabel(UCrtTabsWidget::TabSheet, Name.ToUpper()); }
	if (Feed)
	{
		if (UTextureRenderTarget2D* Target = OwnerController ? OwnerController->GetSheetFeed() : nullptr)
		{
			FSlateBrush Brush;
			Brush.SetResourceObject(Target);
			Brush.ImageSize = FVector2D(Target->SizeX, Target->SizeY);
			Brush.DrawAs = ESlateBrushDrawType::Image;
			Feed->SetBrush(Brush);
			Feed->SetVisibility(ESlateVisibility::Visible);
		}
		else { Feed->SetVisibility(ESlateVisibility::Hidden); }
	}
	// {Brawn} and the rest become this character's scores.
	if (StatRowTexts.Num() > 0)
	{
		const ABaseCharacter* Me = OwnerController ? Cast<ABaseCharacter>(OwnerController->GetPawn()) : nullptr;
		const FAttributes Attr = Me ? Me->GetAttributes() : FAttributes();
		for (int32 i = 0; i < StatRowTexts.Num(); ++i)
		{
			if (!StatRowTexts[i]) { continue; }
			FString Line = StatRowTemplates[i];
			for (const FName& Which : FAttributes::Names())
			{
				Line = Line.Replace(*FString::Printf(TEXT("{%s}"), *Which.ToString()), *FString::Printf(TEXT("%3d"), Attr.Get(Which)));
			}
			// Derived numbers. They live in FAttributes::Derived rather than in the sheet so the
			// same figures can be used by anything that cares -- damage, saves, a status effect
			// -- instead of only ever being text on a screen.
			for (const TPair<FName, int32>& Pair : FAttributes::Derived(Attr))
			{
				Line = Line.Replace(*FString::Printf(TEXT("{%s}"), *Pair.Key.ToString()), *FString::Printf(TEXT("%3d"), Pair.Value));
			}
			StatRowTexts[i]->SetText(FText::FromString(Line));
		}
	}
	InventoryGrid->SetItems(OwnerController ? OwnerController->Inventory : TArray<FString>());
	if (InventoryCount)
	{
		int32 Used = 0;
		if (OwnerController) { for (const FString& It : OwnerController->Inventory) { if (!It.IsEmpty()) { ++Used; } } }
		InventoryCount->SetText(FText::FromString(FString::Printf(TEXT("%d / %d"), Used, ABasePlayerController::InventoryCapacity)));
	}
	TArray<FString> Gear = OwnerController ? OwnerController->Equipped : TArray<FString>();
	Gear.SetNum(FSheetSpec::Get().Slots.Num());
	GearGrid->SetItems(Gear);
	if (QuickGrid) { QuickGrid->SetItems(QuickbarItems()); }
}

TArray<FString> UCharacterSheetWidget::QuickbarItems() const
{
	TArray<FString> Out; Out.SetNum(10);
	if (!OwnerController) { return Out; }
	// The Nth weapon slot of the spec ("Slot 1", "Slot 2" ...) sits in square N, the way EquipWeaponSlot counts them.
	const FSheetSpec& S = FSheetSpec::Get();
	for (int32 i = 0; i < S.Slots.Num() && i < OwnerController->Equipped.Num(); ++i)
	{
		for (int32 N = 1; N <= 2; ++N) { if (S.Slots[i].Name == FString::Printf(TEXT("Slot %d"), N)) { Out[N - 1] = OwnerController->Equipped[i]; } }   // by name: square N is Slot N
	}
	return Out;
}

void UCharacterSheetWidget::OnQuickSlot(int32 Index)
{
	if (!OwnerController || !QuickGrid) { return; }
	// A quickbar square is PICKED, exactly as a bag or gear square is: the pick outlives the
	// pointer, so INFO keeps showing it once the mouse moves away. Without a remembered pick the
	// info flicked back to whatever the bag had selected the moment you left the square.
	SelectedQuick = Index; SelectedBag = -1; SelectedGear = -1;
	QuickGrid->SetSelected(Index);
	if (InventoryGrid) { InventoryGrid->SetSelected(-1); }
	if (GearGrid) { GearGrid->SetSelected(-1); }
	if (Index < 2 && !QuickbarItems()[Index].IsEmpty()) { OwnerController->EquipWeaponSlot(Index + 1); }
	ShowSelectedInfo();
}

void UCharacterSheetWidget::OnQuickHover(int32 Index) { if (Index < 0) { ShowSelectedInfo(); return; } const TArray<FString> Q = QuickbarItems(); HoverInfo(Q.IsValidIndex(Index) ? Q[Index] : FString()); }

// A drop is valid when the item fits where it lands and whatever it displaces fits where it came
// from: bag to bag always, bag to a gear slot of the right kind, gear back to the bag, gear to
// gear when both fit. The quickbar takes nothing until its binding UI exists.
bool UCharacterSheetWidget::CanDrop(int32 SrcGrid, int32 Src, int32 DstGrid, int32 Dst)
{
	if (!OwnerController || (SrcGrid == DstGrid && Src == Dst)) { return false; }
	const TArray<FString>& Bag = OwnerController->Inventory;
	const TArray<FString>& Gear = OwnerController->Equipped;
	const FString SrcItem = SrcGrid == 0 ? (Bag.IsValidIndex(Src) ? Bag[Src] : FString()) : SrcGrid == 1 ? (Gear.IsValidIndex(Src) ? Gear[Src] : FString()) : FString();
	if (SrcItem.IsEmpty()) { return false; }
	if (DstGrid == 0)
	{
		if (Dst < 0 || Dst >= ABasePlayerController::InventoryCapacity) { return false; }
		const FString There = Bag.IsValidIndex(Dst) ? Bag[Dst] : FString();
		return SrcGrid == 0 || There.IsEmpty() || OwnerController->KindFitsSlot(There, Src);
	}
	if (DstGrid == 1)
	{
		if (!OwnerController->KindFitsSlot(SrcItem, Dst)) { return false; }
		const FString There = Gear.IsValidIndex(Dst) ? Gear[Dst] : FString();
		return SrcGrid == 0 || There.IsEmpty() || OwnerController->KindFitsSlot(There, Src);
	}
	return false;
}

void UCharacterSheetWidget::OnDropped(int32 SrcGrid, int32 Src, int32 DstGrid, int32 Dst)
{
	if (!OwnerController || !CanDrop(SrcGrid, Src, DstGrid, Dst)) { return; }
	bool bDone = false;
	if (SrcGrid == 0 && DstGrid == 0) { bDone = OwnerController->MoveInventory(Src, Dst); }
	else if (SrcGrid == 0 && DstGrid == 1) { bDone = OwnerController->EquipFromInventoryToSlot(Src, Dst); }
	else if (SrcGrid == 1 && DstGrid == 0) { bDone = OwnerController->UnequipToInventory(Src, Dst); }
	else if (SrcGrid == 1 && DstGrid == 1) { bDone = OwnerController->SwapGear(Src, Dst); }
	if (!bDone) { return; }
	SelectedBag = -1; SelectedGear = -1;
	Refresh();
}

void UCharacterSheetWidget::OnClose() { if (OwnerController) { OwnerController->HideCharacterSheet(); } }
void UCharacterSheetWidget::OnTab(int32 Tab) { if (OwnerController) { OwnerController->ShowConsolePage(Tab); } }

// A click picks an item out and shows it in the info panel; CTRL-click moves it (equip from the
// bag, unequip from the gear), which is what a plain click used to do and kept moving things
// people only meant to look at.
static bool CtrlHeld() { return FSlateApplication::IsInitialized() && FSlateApplication::Get().GetModifierKeys().IsControlDown(); }
void UCharacterSheetWidget::OnInventoryRightClick(int32 Index, FVector2D ScreenPos)
{
	if (!OwnerController || !OwnerController->Inventory.IsValidIndex(Index) || OwnerController->Inventory[Index].IsEmpty()) { return; }
	OnInventorySlot(Index);   // shown in INFO, as a click would
	TWeakObjectPtr<UCharacterSheetWidget> Self(this);
	UContextMenuWidget::Show(OwnerController, ScreenPos, WeaponCatalog::DisplayName(OwnerController->Inventory[Index]), { TEXT("Drop"), TEXT("Delete") }, [Self, Index](const FString& Pick)
	{
		if (!Self.IsValid() || !Self->OwnerController) { return; }
		// Drop leaves it on the floor; Delete does not. A thing with no mesh to drop can only go the
		// second way, which is why both are offered rather than one being the other's fallback.
		const bool bGone = (Pick == TEXT("Drop")) ? Self->OwnerController->DropInventory(Index)
			: (Pick == TEXT("Delete")) ? Self->OwnerController->DeleteInventory(Index) : false;
		if (bGone) { Self->SelectedBag = -1; Self->Refresh(); Self->ShowSelectedInfo(); }
	});
}

void UCharacterSheetWidget::OnGearRightClick(int32 Index, FVector2D ScreenPos)
{
	if (!OwnerController || !OwnerController->Equipped.IsValidIndex(Index) || OwnerController->Equipped[Index].IsEmpty()) { return; }
	OnGearSlot(Index);
	TWeakObjectPtr<UCharacterSheetWidget> Self(this);
	UContextMenuWidget::Show(OwnerController, ScreenPos, WeaponCatalog::DisplayName(OwnerController->Equipped[Index]), { TEXT("Drop"), TEXT("Delete") }, [Self, Index](const FString& Pick)
	{
		if (!Self.IsValid() || !Self->OwnerController) { return; }
		const bool bGone = (Pick == TEXT("Drop")) ? Self->OwnerController->DropGear(Index)
			: (Pick == TEXT("Delete")) ? Self->OwnerController->DeleteGear(Index) : false;
		if (bGone) { Self->SelectedGear = -1; Self->Refresh(); Self->ShowSelectedInfo(); }
	});
}

void UCharacterSheetWidget::OnInventorySlot(int32 Index)
{
	if (CtrlHeld()) { if (OwnerController && OwnerController->EquipFromInventory(Index)) { SelectedBag = -1; SelectedGear = -1; SelectedQuick = -1; Refresh(); } return; }
	SelectedBag = Index; SelectedGear = -1; SelectedQuick = -1;
	if (InventoryGrid) { InventoryGrid->SetSelected(Index); }
	if (GearGrid) { GearGrid->SetSelected(-1); }
	if (QuickGrid) { QuickGrid->SetSelected(-1); }
	ShowSelectedInfo();
}
void UCharacterSheetWidget::OnGearSlot(int32 Index)
{
	if (CtrlHeld()) { if (OwnerController && OwnerController->UnequipSlot(Index)) { SelectedBag = -1; SelectedGear = -1; SelectedQuick = -1; Refresh(); } return; }
	SelectedGear = Index; SelectedBag = -1; SelectedQuick = -1;
	if (GearGrid) { GearGrid->SetSelected(Index); }
	if (InventoryGrid) { InventoryGrid->SetSelected(-1); }
	if (QuickGrid) { QuickGrid->SetSelected(-1); }
	ShowSelectedInfo();
}
// Hovering shows what is under the pointer; leaving goes back to what was picked.
// Passing over an EMPTY square leaves INFO alone. Blanking it there meant a pick was wiped out by
// the pointer merely crossing a gap in the grid on its way somewhere else.
void UCharacterSheetWidget::HoverInfo(const FString& Item) { if (Item.IsEmpty()) { ShowSelectedInfo(); } else { ShowInfo(Item); } }
void UCharacterSheetWidget::OnInventoryHover(int32 Index) { if (Index < 0) { ShowSelectedInfo(); return; } HoverInfo(OwnerController && OwnerController->Inventory.IsValidIndex(Index) ? OwnerController->Inventory[Index] : FString()); }
void UCharacterSheetWidget::OnGearHover(int32 Index) { if (Index < 0) { ShowSelectedInfo(); return; } HoverInfo(OwnerController && OwnerController->Equipped.IsValidIndex(Index) ? OwnerController->Equipped[Index] : FString()); }
void UCharacterSheetWidget::ShowSelectedInfo()
{
	if (!OwnerController) { ShowInfo(FString()); return; }
	if (SelectedBag >= 0 && OwnerController->Inventory.IsValidIndex(SelectedBag)) { ShowInfo(OwnerController->Inventory[SelectedBag]); return; }
	if (SelectedGear >= 0 && OwnerController->Equipped.IsValidIndex(SelectedGear)) { ShowInfo(OwnerController->Equipped[SelectedGear]); return; }
	if (SelectedQuick >= 0) { const TArray<FString> Q = QuickbarItems(); if (Q.IsValidIndex(SelectedQuick)) { ShowInfo(Q[SelectedQuick]); return; } }
	ShowInfo(FString());
}

void UCharacterSheetWidget::ShowInfo(const FString& Item)
{
	InfoItem = Item;
	if (InfoLink) { InfoLink->SetVisibility(Item.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::Visible); }
	if (InfoName) { InfoName->SetText(FText::FromString(ItemCatalog::InfoTitle(Item))); }
	if (InfoText)
	{
		// The description, and under it what THIS copy is carrying. Empty for anything that cannot
		// take an accessory, so the line simply does not appear rather than reading "none".
		FString Blurb = Item.IsEmpty() ? FString() : ItemCatalog::Describe(Item);
		const FString Fitted = (Item.IsEmpty() || !OwnerController) ? FString() : OwnerController->AccessorySummary(Item);
		if (!Fitted.IsEmpty()) { Blurb += LINE_TERMINATOR; Blurb += Fitted; }
		InfoText->SetText(FText::FromString(Blurb));
	}
	if (SkinButton)
	{
		const WeaponCatalog::FWeapon* W = Item.IsEmpty() ? nullptr : WeaponCatalog::Find(Item);
		const TArray<FString> V = W ? WeaponSkins::Variants(*W) : TArray<FString>();
		SkinButton->SetVisibility(V.Num() >= 2 ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		if (SkinLabel && W && V.Num() >= 2) { SkinLabel->SetText(FText::FromString(FString::Printf(TEXT("[ SKIN %d/%d ]"), V.IndexOfByKey(WeaponSkins::Current(*W)) + 1, V.Num()))); }
	}
}

void UCharacterSheetWidget::OnInfoLink() { if (OwnerController && !InfoItem.IsEmpty()) { OwnerController->ShowReferenceFor(InfoItem); } }

void UCharacterSheetWidget::OnSkin()
{
	if (!OwnerController || InfoItem.IsEmpty()) { return; }
	OwnerController->CycleWeaponSkin(InfoItem);
	ShowInfo(InfoItem);
}

FReply UCharacterSheetWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && Feed && Feed->GetCachedGeometry().IsUnderLocation(InMouseEvent.GetScreenSpacePosition()))
	{
		bDragging = true;
		DragLast = InMouseEvent.GetScreenSpacePosition();
		DragStart = DragLast; DragTravel = 0.0f;
		return FReply::Handled().CaptureMouse(TakeWidget());
	}
	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

FReply UCharacterSheetWidget::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (bDragging && InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		bDragging = false;
		// A click (no real drag): on the face it zooms to the head; anywhere while zoomed goes back.
		if (DragTravel < 6.0f && OwnerController && Feed)
		{
			const FGeometry& G = Feed->GetCachedGeometry();
			const FVector2D Local = G.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition());
			const float Frac = G.GetLocalSize().Y > 0.0f ? Local.Y / G.GetLocalSize().Y : 1.0f;
			if (OwnerController->IsSheetMirrorZoomed()) { OwnerController->SetSheetMirrorZoom(false); }
			else if (Frac < FaceClickFraction) { OwnerController->SetSheetMirrorZoom(true); }
		}
		return FReply::Handled().ReleaseMouseCapture();
	}
	return Super::NativeOnMouseButtonUp(InGeometry, InMouseEvent);
}

FReply UCharacterSheetWidget::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (bDragging)
	{
		const FVector2D Now = InMouseEvent.GetScreenSpacePosition();
		const FVector2D Delta = Now - DragLast;
		DragLast = Now; DragTravel += Delta.Size();
		// Dragging right turns the figure to the right (the camera swings the other way); up tilts down on them.
		if (OwnerController) { OwnerController->OrbitSheetMirror(-Delta.X * 0.4f, Delta.Y * 0.25f); }
		return FReply::Handled();
	}
	return Super::NativeOnMouseMove(InGeometry, InMouseEvent);
}

void UCharacterSheetWidget::OnDragBegan(const FSlateBrush& Brush)
{
	if (!DragImage) { return; }
	DragImage->SetBrush(Brush);
	DragImage->SetVisibility(ESlateVisibility::HitTestInvisible);
	bDragPicture = true;
}

void UCharacterSheetWidget::OnDragEnded()
{
	bDragPicture = false;
	if (DragImage) { DragImage->SetVisibility(ESlateVisibility::Collapsed); }
}

void UCharacterSheetWidget::OnDropRefused(int32 SrcGrid, int32 Src, int32 DstGrid, int32 Dst)
{
	// Say why, in the INFO text: the kinds the slot takes, or that the square is out of reach.
	if (!OwnerController || !InfoText) { return; }
	const FSheetSpec& Spec = FSheetSpec::Get();
	FString Why = TEXT("It does not go there.");
	if (DstGrid == 1 && Spec.Slots.IsValidIndex(Dst))
	{
		const FSheetGearSlot& S = Spec.Slots[Dst];
		Why = S.bEnabled ? FString::Printf(TEXT("%s takes: %s"), *S.Name, *FString::Join(S.Kinds, TEXT(", "))) : FString::Printf(TEXT("%s is not available yet."), *S.Name);
	}
	else if (DstGrid == 2) { Why = TEXT("The quickbar mirrors the weapon slots; bind it from a slot."); }
	InfoText->SetText(FText::FromString(Why));
}

void UCharacterSheetWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	if (bDragPicture && DragImage && DragLayer)
	{
		// The picture follows the pointer, centred on it, in the canvas's own space.
		const FVector2D Local = USlateBlueprintLibrary::AbsoluteToLocal(DragLayer->GetCachedGeometry(), FSlateApplication::Get().GetCursorPos());
		const FVector2D Size = DragImage->GetBrush().ImageSize.IsNearlyZero() ? FVector2D(64.0f, 64.0f) : DragImage->GetBrush().ImageSize;
		if (UCanvasPanelSlot* PS = Cast<UCanvasPanelSlot>(DragImage->Slot)) { PS->SetAutoSize(false); PS->SetSize(Size); PS->SetPosition(Local - Size * 0.5f); }
	}
	Super::NativeTick(MyGeometry, InDeltaTime);
	Clock += InDeltaTime;
	// Once the panel has laid out, pin the two text columns to widths taken from ITS width, in
	// the ratio the fill weights expressed. From here on no column depends on what its text
	// measures, so the runtime font filling in cannot move anything.
	// A resize changes the body's width after the columns were pinned: unpin them, so the next
	// tick measures and fits again from the new width. (The pins stop TEXT from moving the
	// columns; a resize is not text moving.)
	if (bColumnsFitted && Body && StatsBox && RightBox && FMath::Abs(Body->GetCachedGeometry().GetLocalSize().X - FittedBodyW) > 1.0f)
	{
		const FSheetSpec& S = FSheetSpec::Get();
		StatsBox->ClearWidthOverride();
		if (UHorizontalBoxSlot* SS = Cast<UHorizontalBoxSlot>(StatsBox->Slot)) { FSlateChildSize Sz(ESlateSizeRule::Fill); Sz.Value = S.StatsWeight; SS->SetSize(Sz); }
		if (S.RightWeight > 0.0f)
		{
			RightBox->ClearWidthOverride();
			if (UHorizontalBoxSlot* RS = Cast<UHorizontalBoxSlot>(RightBox->Slot)) { FSlateChildSize Sz(ESlateSizeRule::Fill); Sz.Value = S.RightWeight; RS->SetSize(Sz); }
		}
		bColumnsFitted = false;
	}
	if (!bColumnsFitted && Body && StatsBox && MirrorBox && RightBox)
	{
		const float BodyW = Body->GetCachedGeometry().GetLocalSize().X;
		if (BodyW > 50.0f)
		{
			const FSheetSpec& S = FSheetSpec::Get();
			// What the dividers and the gaps take, MEASURED: the body less the three columns as
			// Slate laid them this frame. The mirror comes from the spec's rule (shared with the
			// Appearance page), the right column hugs its grids or takes its weight, and the stats
			// column is exactly what remains -- so the three always sum to the body and nothing
			// can reach past the panel.
			const float Overhead = FMath::Max(0.0f, BodyW - StatsBox->GetCachedGeometry().GetLocalSize().X
				- MirrorBox->GetCachedGeometry().GetLocalSize().X - RightBox->GetCachedGeometry().GetLocalSize().X);
			const float Usable = FMath::Max(300.0f, BodyW - Overhead);
			const float RightW = S.RightWeight > 0.0f
				? FMath::Floor(Usable * S.RightWeight / FMath::Max(0.01f, S.StatsWeight + S.MirrorWeight + S.RightWeight))
				: S.RightColumnWidth();
			MirrorWidth = S.MirrorWidthFor(BodyW);
			const float StatsW = FMath::Max(120.0f, FMath::Floor(Usable - RightW - MirrorWidth));
			StatsBox->SetWidthOverride(StatsW);
			MirrorBox->SetWidthOverride(MirrorWidth);
			if (MirrorFit) { MirrorFit->SetHeightOverride(FMath::Floor(PaneShape::HeightFor(PaneShape::Mirror, MirrorWidth))); }
			RightBox->SetWidthOverride(RightW);
			if (UHorizontalBoxSlot* SS = Cast<UHorizontalBoxSlot>(StatsBox->Slot)) { SS->SetSize(FSlateChildSize(ESlateSizeRule::Automatic)); }
			if (UHorizontalBoxSlot* MS = Cast<UHorizontalBoxSlot>(MirrorBox->Slot)) { MS->SetSize(FSlateChildSize(ESlateSizeRule::Automatic)); }
			if (UHorizontalBoxSlot* RS = Cast<UHorizontalBoxSlot>(RightBox->Slot)) { RS->SetSize(FSlateChildSize(ESlateSizeRule::Automatic)); }
			UE_LOG(LogTemp, Log, TEXT("Sheet: body %.0f = stats %.0f + mirror %.0f + right %.0f + overhead %.0f"), BodyW, StatsW, MirrorWidth, RightW, Overhead);
			bColumnsFitted = true;
			FittedBodyW = BodyW;
			SettleTicks = FMath::Max(SettleTicks, 2);
		}
	}
	// The first ticks after a rebuild are layout settling (rules padding, the portrait sizing): shown once still.
	if (SettleTicks > 0) { if (--SettleTicks == 0) { SetRenderOpacity(1.0f); if (OwnerController) { OwnerController->PageSettled(this); } } }
}

int32 UCharacterSheetWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	LayerId = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
	Crt::PaintFrame(OutDrawElements, AllottedGeometry, FVector2f::ZeroVector, FVector2f(AllottedGeometry.GetLocalSize()), LayerId + 1, Clock);
	// The portrait is drawn again above the frame's scanlines (the panel is a CRT, the mirror a
	// window), then a much fainter set of lines so it still belongs to the screen.
	if (Feed && Feed->GetBrush().GetResourceObject())
	{
		const FGeometry& FeedGeo = Feed->GetCachedGeometry();
		const FVector2f Sz(FeedGeo.GetLocalSize());
		if (Sz.X > 1.0f)
		{
			FSlateDrawElement::MakeBox(OutDrawElements, LayerId + 2, FeedGeo.ToPaintGeometry(), &Feed->GetBrush(), ESlateDrawEffect::None, FLinearColor::White);
			const FSlateBrush* White = FCoreStyle::Get().GetBrush("WhiteBrush");
			for (float Ly = 1.0f; Ly < Sz.Y; Ly += 3.0f) { FSlateDrawElement::MakeBox(OutDrawElements, LayerId + 3, FeedGeo.ToPaintGeometry(FVector2f(Sz.X, 1.0f), FSlateLayoutTransform(FVector2f(0.0f, Ly))), White, ESlateDrawEffect::None, FLinearColor(0, 0, 0, 0.04f)); }
			const TArray<FVector2f> Rim = { {0.0f, 0.0f}, {Sz.X, 0.0f}, Sz, {0.0f, Sz.Y}, {0.0f, 0.0f} };
			FSlateDrawElement::MakeLines(OutDrawElements, LayerId + 4, FeedGeo.ToPaintGeometry(), Rim, ESlateDrawEffect::None, Crt::Faint, true, 1.0f);
		}
	}
	return LayerId + 5;
}
