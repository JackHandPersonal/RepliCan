#include "CharacterSheetWidget.h"
#include "Framework/Application/SlateApplication.h"
#include "InventoryGridWidget.h"
#include "CrtRuleWidget.h"
#include "CrtTabsWidget.h"
#include "ItemCatalog.h"
#include "SheetSpec.h"
#include "BaseCharacter.h"
#include "BasePlayerController.h"
#include "CrtStyle.h"
#include "Blueprint/WidgetTree.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/Border.h"
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
	Root->SetContent(Column);
	Rebuild();
}

UVerticalBoxSlot* UCharacterSheetWidget::AddRow(UVerticalBox* Box, const FString& Text, int32 Size, const FLinearColor& Color)
{
	int32 Dummy;
	if (!Text.IsEmpty() && RuleChars.FindChar(Text[Text.Len() - 1], Dummy)) { return Box->AddChildToVerticalBox(UCrtRuleWidget::Make(GetOwningPlayer(), Text, Size, Color)); }
	return Box->AddChildToVerticalBox(Crt::FixedText(WidgetTree, Text, Size, Color));
}

void UCharacterSheetWidget::Rebuild()
{
	if (!Column) { return; }
	const FSheetSpec& S = FSheetSpec::Get();
	FaceClickFraction = S.FaceClickFraction;
	RuleChars = S.RuleChars.IsEmpty() ? TEXT("-=") : S.RuleChars;
	Column->ClearChildren();
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
		UVerticalBoxSlot* CaptionSlot = AddRow(Stats, Section.Caption, S.CaptionSize, Crt::DimGreen);
		// Space above each caption but the first, so the blocks read as separate without a rule.
		CaptionSlot->SetPadding(FMargin(0, SectionIndex == 0 ? 0.0f : 14.0f, 0, 8));
		for (const FString& Row : Section.Rows)
		{
			UVerticalBoxSlot* RowSlot = AddRow(Stats, Row, S.RowSize, Crt::DimGreen);
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
	if (!S.MirrorCaption.IsEmpty()) { AddRow(Middle, S.MirrorCaption, S.CaptionSize, Crt::DimGreen)->SetPadding(FMargin(0, 0, 0, 8)); }
	// A box held to the portrait's 5:8 aspect: it lays out in one pass, where a scale box needs a
	// frame to find its size and the columns visibly shift while it does.
	USizeBox* Fit = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	MirrorFit = Fit;
	Fit->SetMinAspectRatio(0.625f); Fit->SetMaxAspectRatio(0.625f);
	Feed = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
	Feed->SetColorAndOpacity(FLinearColor::White);
	Fit->AddChild(Feed);
	UVerticalBoxSlot* FitSlot = Middle->AddChildToVerticalBox(Fit);
	// At the TOP of the column, its own height (5:8 of the mirror's width, set with the width in
	// NativeTick): a fill slot stretched the box to the column and centred the picture in it.
	FitSlot->SetSize(ESlateSizeRule::Automatic); FitSlot->SetHorizontalAlignment(HAlign_Fill); FitSlot->SetVerticalAlignment(VAlign_Top);
	Fit->SetHeightOverride(FMath::Floor(MirrorWidth / 0.625f));
	if (!S.MirrorFooter.IsEmpty()) { AddRow(Middle, S.MirrorFooter, S.CaptionSize, Crt::DimGreen)->SetPadding(FMargin(0, 8, 0, 0)); }
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
	AddRow(Right, S.GearCaption, S.CaptionSize, Crt::DimGreen)->SetPadding(FMargin(0, 0, 0, 8));
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
	GearGrid->OnSlotClicked.BindUObject(this, &UCharacterSheetWidget::OnGearSlot);
	GearGrid->OnSlotHovered.BindUObject(this, &UCharacterSheetWidget::OnGearHover);
	Right->AddChildToVerticalBox(GearGrid)->SetHorizontalAlignment(HAlign_Left);
	if (!S.InventoryCaption.IsEmpty())
	{
		// The caption rule fills the width with how full the bag is at the right end of it.
		UHorizontalBox* InvHead = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		UHorizontalBoxSlot* CapSlot = InvHead->AddChildToHorizontalBox(UCrtRuleWidget::Make(GetOwningPlayer(), S.InventoryCaption, S.CaptionSize, Crt::DimGreen));
		CapSlot->SetSize(ESlateSizeRule::Fill); CapSlot->SetVerticalAlignment(VAlign_Center);
		InventoryCount = Crt::FixedText(WidgetTree, TEXT(""), S.CaptionSize, Crt::DimGreen, ETextJustify::Right);
		InvHead->AddChildToHorizontalBox(InventoryCount)->SetPadding(FMargin(10, 0, 0, 0));
		Right->AddChildToVerticalBox(InvHead)->SetPadding(FMargin(0, 18, 0, 8));
	}
	InventoryGrid = UInventoryGridWidget::MakeBag(GetOwningPlayer(), ABasePlayerController::InventoryCapacity);
	InventoryGrid->OnSlotClicked.BindUObject(this, &UCharacterSheetWidget::OnInventorySlot);
	InventoryGrid->OnSlotHovered.BindUObject(this, &UCharacterSheetWidget::OnInventoryHover);
	Right->AddChildToVerticalBox(InventoryGrid)->SetHorizontalAlignment(HAlign_Left);


	// Info for whatever is hovered.
	if (!S.InfoCaption.IsEmpty()) { AddRow(Right, S.InfoCaption, S.CaptionSize, Crt::DimGreen)->SetPadding(FMargin(0, 18, 0, 8)); }
	InfoName = Crt::FixedText(WidgetTree, TEXT(""), S.InfoNameSize, Crt::Green);
	Right->AddChildToVerticalBox(InfoName);
	InfoText = Crt::FixedText(WidgetTree, TEXT(""), S.InfoTextSize, Crt::DimGreen);
	InfoText->SetAutoWrapText(true);
	Right->AddChildToVerticalBox(InfoText)->SetPadding(FMargin(0, 3, 0, 0));
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
				Line = Line.Replace(*FString::Printf(TEXT("{%s}"), *Which.ToString()), *FString::Printf(TEXT("%2d"), Attr.Get(Which)));
			}
			// Derived numbers. They live in FAttributes::Derived rather than in the sheet so the
			// same figures can be used by anything that cares -- damage, saves, a status effect
			// -- instead of only ever being text on a screen.
			for (const TPair<FName, int32>& Pair : FAttributes::Derived(Attr))
			{
				Line = Line.Replace(*FString::Printf(TEXT("{%s}"), *Pair.Key.ToString()), *FString::Printf(TEXT("%2d"), Pair.Value));
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
}

void UCharacterSheetWidget::OnClose() { if (OwnerController) { OwnerController->HideCharacterSheet(); } }
void UCharacterSheetWidget::OnTab(int32 Tab) { if (OwnerController) { OwnerController->ShowConsolePage(Tab); } }

// A click picks an item out and shows it in the info panel; CTRL-click moves it (equip from the
// bag, unequip from the gear), which is what a plain click used to do and kept moving things
// people only meant to look at.
static bool CtrlHeld() { return FSlateApplication::IsInitialized() && FSlateApplication::Get().GetModifierKeys().IsControlDown(); }
void UCharacterSheetWidget::OnInventorySlot(int32 Index)
{
	if (CtrlHeld()) { if (OwnerController && OwnerController->EquipFromInventory(Index)) { SelectedBag = -1; SelectedGear = -1; Refresh(); } return; }
	SelectedBag = Index; SelectedGear = -1;
	if (InventoryGrid) { InventoryGrid->SetSelected(Index); }
	if (GearGrid) { GearGrid->SetSelected(-1); }
	ShowSelectedInfo();
}
void UCharacterSheetWidget::OnGearSlot(int32 Index)
{
	if (CtrlHeld()) { if (OwnerController && OwnerController->UnequipSlot(Index)) { SelectedBag = -1; SelectedGear = -1; Refresh(); } return; }
	SelectedGear = Index; SelectedBag = -1;
	if (GearGrid) { GearGrid->SetSelected(Index); }
	if (InventoryGrid) { InventoryGrid->SetSelected(-1); }
	ShowSelectedInfo();
}
// Hovering shows what is under the pointer; leaving goes back to what was picked.
void UCharacterSheetWidget::OnInventoryHover(int32 Index) { if (Index < 0) { ShowSelectedInfo(); return; } ShowInfo(OwnerController && OwnerController->Inventory.IsValidIndex(Index) ? OwnerController->Inventory[Index] : FString()); }
void UCharacterSheetWidget::OnGearHover(int32 Index) { if (Index < 0) { ShowSelectedInfo(); return; } ShowInfo(OwnerController && OwnerController->Equipped.IsValidIndex(Index) ? OwnerController->Equipped[Index] : FString()); }
void UCharacterSheetWidget::ShowSelectedInfo()
{
	if (!OwnerController) { ShowInfo(FString()); return; }
	if (SelectedBag >= 0 && OwnerController->Inventory.IsValidIndex(SelectedBag)) { ShowInfo(OwnerController->Inventory[SelectedBag]); return; }
	if (SelectedGear >= 0 && OwnerController->Equipped.IsValidIndex(SelectedGear)) { ShowInfo(OwnerController->Equipped[SelectedGear]); return; }
	ShowInfo(FString());
}

void UCharacterSheetWidget::ShowInfo(const FString& Item)
{
	if (InfoName) { InfoName->SetText(FText::FromString(ItemCatalog::InfoTitle(Item))); }
	if (InfoText) { InfoText->SetText(FText::FromString(Item.IsEmpty() ? FString() : ItemCatalog::Describe(Item))); }
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

void UCharacterSheetWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
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
			if (MirrorFit) { MirrorFit->SetHeightOverride(FMath::Floor(MirrorWidth / 0.625f)); }
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
