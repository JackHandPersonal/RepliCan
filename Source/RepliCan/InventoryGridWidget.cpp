#include "InventoryGridWidget.h"
#include "CrtStyle.h"
#include "SheetSpec.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Engine/Texture2D.h"
#include "WeaponCatalog.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/GridPanel.h"
#include "Components/GridSlot.h"
#include "Components/Spacer.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"

void UInventorySlotBinding::OnClicked()
{
	if (Grid) { Grid->HandleClick(Index); }
}

void UInventorySlotBinding::OnHovered() { if (Grid) { Grid->HandleHover(Index); } }
void UInventorySlotBinding::OnUnhovered() { if (Grid) { Grid->HandleHover(-1); } }

void UInventoryGridWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Column"));
	WidgetTree->RootWidget = Column;
	TitleText = Crt::Text(WidgetTree, TEXT(""), 15, Crt::DimGreen);
	Column->AddChildToVerticalBox(TitleText)->SetPadding(FMargin(0, 0, 0, 6));
	Grid = WidgetTree->ConstructWidget<UGridPanel>(UGridPanel::StaticClass(), TEXT("Grid"));
	Column->AddChildToVerticalBox(Grid);
}

UInventoryGridWidget* UInventoryGridWidget::MakeBag(APlayerController* Owner, int32 Capacity)
{
	const FSheetSpec& S = FSheetSpec::Get();
	UInventoryGridWidget* G = CreateWidget<UInventoryGridWidget>(Owner, UInventoryGridWidget::StaticClass());
	G->SetGap(S.InventoryGap);
	G->Configure(TEXT(""), Capacity, S.InventoryColumns, S.InventoryCell);
	return G;
}

void UInventoryGridWidget::Configure(const FString& Title, int32 InCapacity, int32 InColumns, float Cell)
{
	TArray<FIntPoint> Layout;
	for (int32 i = 0; i < FMath::Max(0, InCapacity); ++i) { Layout.Add(FIntPoint(i / FMath::Max(1, InColumns), i % FMath::Max(1, InColumns))); }
	Columns = FMath::Max(1, InColumns);
	ConfigureCells(Title, Layout, Cell);
}

void UInventoryGridWidget::ConfigureCells(const FString& Title, const TArray<FIntPoint>& InCells, float Cell)
{
	Capacity = InCells.Num(); CellSize = Cell;
	if (TitleText) { TitleText->SetText(FText::FromString(Title)); TitleText->SetVisibility(Title.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::Visible); }
	if (!Grid) { return; }
	Grid->ClearChildren();
	Cells.Reset(); Labels.Reset(); Icons.Reset(); Bindings.Reset();
	SlotNames.SetNum(Capacity); SlotEnabled.Init(true, Capacity);
	for (int32 i = 0; i < Capacity; ++i)
	{
		USizeBox* Box = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		Box->SetWidthOverride(CellSize); Box->SetHeightOverride(CellSize);
		UButton* B = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
		B->SetStyle(Crt::BoxedButtonStyle());
		// Each cell: the item's icon (when one exists) under a text label (the fallback, or empty).
		UOverlay* Stack = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
		UImage* Icon = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
		Icon->SetVisibility(ESlateVisibility::Collapsed);
		if (UOverlaySlot* IconSlot = Stack->AddChildToOverlay(Icon)) { IconSlot->SetPadding(FMargin(3.0f)); IconSlot->SetHorizontalAlignment(HAlign_Fill); IconSlot->SetVerticalAlignment(VAlign_Fill); }
		UTextBlock* L = Crt::Text(WidgetTree, TEXT(""), 11, Crt::Green, ETextJustify::Center);
		L->SetAutoWrapText(true);
		if (UOverlaySlot* TextSlot = Stack->AddChildToOverlay(L)) { TextSlot->SetPadding(FMargin(4.0f)); TextSlot->SetHorizontalAlignment(HAlign_Center); TextSlot->SetVerticalAlignment(VAlign_Center); }
		B->AddChild(Stack);
		if (UButtonSlot* StackSlot = Cast<UButtonSlot>(Stack->Slot)) { StackSlot->SetPadding(FMargin(0.0f)); StackSlot->SetHorizontalAlignment(HAlign_Fill); StackSlot->SetVerticalAlignment(VAlign_Fill); }
		Box->SetContent(B);
		UInventorySlotBinding* Binding = NewObject<UInventorySlotBinding>(this);
		Binding->Grid = this; Binding->Index = i;
		B->OnClicked.AddDynamic(Binding, &UInventorySlotBinding::OnClicked);
		B->OnHovered.AddDynamic(Binding, &UInventorySlotBinding::OnHovered);
		B->OnUnhovered.AddDynamic(Binding, &UInventorySlotBinding::OnUnhovered);
		Bindings.Add(Binding); Cells.Add(B); Labels.Add(L); Icons.Add(Icon);
		if (UGridSlot* GS = Grid->AddChildToGrid(Box, InCells[i].X, InCells[i].Y)) { GS->SetPadding(FMargin(Gap * 0.5f)); }
	}
	SetItems({});
}

void UInventoryGridWidget::SetSlotNames(const TArray<FString>& Names)
{
	SlotNames = Names; SlotNames.SetNum(Capacity);
	SetItems({});
}

void UInventoryGridWidget::AddCaption(int32 Row, int32 Col, const FString& Text, int32 Align)
{
	if (!Grid) { return; }
	const ETextJustify::Type J = Align == 1 ? ETextJustify::Right : (Align == 2 ? ETextJustify::Center : ETextJustify::Left);
	UTextBlock* T = Crt::FixedText(WidgetTree, Text.ToUpper(), 11, Crt::DimGreen, J);
	if (UGridSlot* S = Grid->AddChildToGrid(T, Row, Col)) { S->SetHorizontalAlignment(Align == 1 ? HAlign_Right : (Align == 2 ? HAlign_Center : HAlign_Left)); S->SetVerticalAlignment(VAlign_Center); S->SetPadding(FMargin(Gap * 0.5f + (Align == 2 ? 0.0f : 4.0f), Gap * 0.5f)); }
}

void UInventoryGridWidget::AddSpacer(int32 Row, int32 Col, float Width)
{
	if (!Grid) { return; }
	USpacer* Sp = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass());
	Sp->SetSize(FVector2D(Width, 0.0f));
	Grid->AddChildToGrid(Sp, Row, Col);
}

void UInventoryGridWidget::SetGap(float InGap)
{
	Gap = FMath::Max(0.0f, InGap);
}

void UInventoryGridWidget::SetSlotEnabled(int32 Index, bool bEnabled)
{
	if (!SlotEnabled.IsValidIndex(Index)) { return; }
	SlotEnabled[Index] = bEnabled;
	if (Cells.IsValidIndex(Index) && Cells[Index]) { Cells[Index]->SetIsEnabled(bEnabled); Cells[Index]->SetRenderOpacity(bEnabled ? 1.0f : 0.6f); }
}

void UInventoryGridWidget::SetItems(const TArray<FString>& Items)
{
	for (int32 i = 0; i < Cells.Num(); ++i)
	{
		const bool bFilled = Items.IsValidIndex(i) && !Items[i].IsEmpty();
		UTexture2D* Tex = bFilled ? FindIcon(Items[i]) : nullptr;
		if (Icons.IsValidIndex(i) && Icons[i])
		{
			if (Tex) { Icons[i]->SetBrushFromTexture(Tex, false); Icons[i]->SetVisibility(ESlateVisibility::HitTestInvisible); }
			else { Icons[i]->SetVisibility(ESlateVisibility::Collapsed); }
		}
		if (Labels[i])
		{
			const bool bCaption = !bFilled && SlotNames.IsValidIndex(i) && !SlotNames[i].IsEmpty();
			Labels[i]->SetText(FText::FromString(bFilled && !Tex ? Items[i] : (bCaption ? SlotNames[i].ToUpper() : FString())));
			Labels[i]->SetColorAndOpacity(FSlateColor(bCaption ? Crt::Faint : Crt::Green));
		}
		if (Cells[i]) { Cells[i]->SetBackgroundColor(i == SelectedIndex && bFilled ? FLinearColor(0.55f, 1.0f, 0.7f, 1) : (bFilled ? FLinearColor(1, 1, 1, 1) : FLinearColor(0.45f, 0.45f, 0.45f, 1))); }
	}
}

UTexture2D* UInventoryGridWidget::FindIcon(const FString& ItemName)
{
	// A weapon knows its own icon. Everything else falls through to the name-derived key.
	if (const WeaponCatalog::FWeapon* W = WeaponCatalog::Find(ItemName))
	{
		if (!W->Icon.IsEmpty())
		{
			const FString IconPath = FString::Printf(TEXT("/Game/RepliCan/Icons/T_Icon_%s.T_Icon_%s"), *W->Icon, *W->Icon);
			if (UTexture2D* Tex = LoadObject<UTexture2D>(nullptr, *IconPath, nullptr, LOAD_NoWarn | LOAD_Quiet)) { return Tex; }
		}
	}
	FString Key;
	bool bGap = false;
	for (TCHAR C : ItemName)
	{
		if (FChar::IsAlnum(C)) { if (bGap && !Key.IsEmpty()) { Key.AppendChar(TEXT('_')); } Key.AppendChar(C); bGap = false; }
		else { bGap = true; }
	}
	if (Key.IsEmpty()) { return nullptr; }
	const FString Path = FString::Printf(TEXT("/Game/RepliCan/Icons/T_Icon_%s.T_Icon_%s"), *Key, *Key);
	return LoadObject<UTexture2D>(nullptr, *Path);
}

void UInventoryGridWidget::SetSelected(int32 Index)
{
	SelectedIndex = Index;
	for (int32 i = 0; i < Cells.Num(); ++i)
	{
		if (!Cells[i]) { continue; }
		const bool bFilled = Icons.IsValidIndex(i) && Icons[i] && Icons[i]->GetVisibility() != ESlateVisibility::Collapsed;
		const bool bLabelled = Labels.IsValidIndex(i) && Labels[i] && !Labels[i]->GetText().IsEmpty() && SlotNames.IsValidIndex(i) && Labels[i]->GetText().ToString() != SlotNames[i].ToUpper();
		const bool bHas = bFilled || bLabelled;
		Cells[i]->SetBackgroundColor(i == SelectedIndex && bHas ? FLinearColor(0.55f, 1.0f, 0.7f, 1) : (bHas ? FLinearColor(1, 1, 1, 1) : FLinearColor(0.45f, 0.45f, 0.45f, 1)));
	}
}

void UInventoryGridWidget::HandleClick(int32 Index)
{
	OnSlotClicked.ExecuteIfBound(Index);
}

void UInventoryGridWidget::HandleHover(int32 Index)
{
	OnSlotHovered.ExecuteIfBound(Index);
}

int32 UInventoryGridWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	LayerId = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
	// Cross-hatch over every disabled cell: diagonals every 9 px, clipped to the square.
	const FLinearColor Hatch(Crt::DimGreen.R, Crt::DimGreen.G, Crt::DimGreen.B, 0.55f);
	for (int32 i = 0; i < Cells.Num(); ++i)
	{
		if (!SlotEnabled.IsValidIndex(i) || SlotEnabled[i] || !Cells[i]) { continue; }
		const FGeometry& CG = Cells[i]->GetCachedGeometry();
		const FVector2D TL = AllottedGeometry.AbsoluteToLocal(CG.GetAbsolutePosition());
		const FVector2D Sz = CG.GetLocalSize();
		if (Sz.X <= 1.0 || Sz.Y <= 1.0) { continue; }
		const float Step = 9.0f;
		for (float D = -Sz.Y; D < Sz.X; D += Step)
		{
			// the line from (D, 0) to (D + H, H), clipped to the square
			double X0 = D, Y0 = 0.0, X1 = D + Sz.Y, Y1 = Sz.Y;
			if (X0 < 0.0) { Y0 = -X0; X0 = 0.0; }
			if (X1 > Sz.X) { Y1 = Sz.Y - (X1 - Sz.X); X1 = Sz.X; }
			if (X0 >= X1) { continue; }
			TArray<FVector2f> Pts; Pts.Add(FVector2f(TL.X + X0, TL.Y + Y0)); Pts.Add(FVector2f(TL.X + X1, TL.Y + Y1));
			FSlateDrawElement::MakeLines(OutDrawElements, LayerId + 1, AllottedGeometry.ToPaintGeometry(), Pts, ESlateDrawEffect::None, Hatch, true, 1.0f);
		}
	}
	return LayerId + 2;
}
