#include "InventoryTransferWidget.h"
#include "BasePlayerController.h"
#include "CrtStyle.h"
#include "CrtRuleWidget.h"
#include "InventoryGridWidget.h"
#include "LootBoxActor.h"
#include "ItemCatalog.h"
#include "SheetSpec.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"

// The container screen is dressed exactly like the character sheet: the same panel, header
// rule with the name set into it, the same bag grid (UInventoryGridWidget::MakeBag), rule
// captions, info block and footer, all from the sheet spec.
void UInventoryTransferWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	UBorder* Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Panel"));
	Panel->SetBrushColor(Crt::Panel);
	Panel->SetPadding(FMargin(34.0f, 26.0f));
	WidgetTree->RootWidget = Panel;
	Outer = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Outer"));
	Panel->SetContent(Outer);
}

void UInventoryTransferWidget::Rebuild()
{
	if (!Outer) { return; }
	const FSheetSpec& S = FSheetSpec::Get();
	Outer->ClearChildren();
	BoxGrid = nullptr; PlayerGrid = nullptr; InfoName = nullptr; InfoText = nullptr; Note = nullptr;
	const FString BoxName = Box ? Box->DisplayName.ToUpper() : TEXT("CONTAINER");
	// The container's caption borrows the inventory caption's dressing: "==[ NAME ]=" for "==[ INVENTORY ]=".
	FString CapLeft = TEXT("==[ "), CapRight = TEXT(" ]=");
	{
		int32 Open = INDEX_NONE, Close = INDEX_NONE;
		if (S.InventoryCaption.FindChar(TEXT('['), Open) && S.InventoryCaption.FindLastChar(TEXT(']'), Close) && Close > Open)
		{
			CapLeft = S.InventoryCaption.Left(Open + 1) + TEXT(" ");
			CapRight = TEXT(" ") + S.InventoryCaption.Mid(Close);
		}
	}

	// Header: the container's name in the rule, an X to close at the right.
	UHorizontalBox* Header = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	Header->AddChildToHorizontalBox(Crt::FixedText(WidgetTree, S.HeaderLeft, S.RuleSize, Crt::Faint))->SetVerticalAlignment(VAlign_Center);
	Header->AddChildToHorizontalBox(Crt::FixedText(WidgetTree, BoxName, S.NameSize, Crt::Green))->SetVerticalAlignment(VAlign_Center);
	UHorizontalBoxSlot* RuleSlot = Header->AddChildToHorizontalBox(UCrtRuleWidget::Make(GetOwningPlayer(), S.HeaderRight, S.RuleSize, Crt::Faint));
	RuleSlot->SetSize(ESlateSizeRule::Fill); RuleSlot->SetVerticalAlignment(VAlign_Center);
	if (!S.HeaderEnd.IsEmpty()) { Header->AddChildToHorizontalBox(Crt::FixedText(WidgetTree, S.HeaderEnd, S.RuleSize, Crt::Faint))->SetVerticalAlignment(VAlign_Center); }
	UButton* Close = Crt::Button(WidgetTree, TEXT(" X "), S.NameSize, Crt::Green);
	Close->OnClicked.AddDynamic(this, &UInventoryTransferWidget::OnClose);
	Header->AddChildToHorizontalBox(Close)->SetPadding(FMargin(12, 0, 0, 0));
	Outer->AddChildToVerticalBox(Header)->SetPadding(FMargin(0, 0, 0, 12));

	// The container's bag, then the player's: the same grid, one above the other.
	Outer->AddChildToVerticalBox(UCrtRuleWidget::Make(GetOwningPlayer(), CapLeft + BoxName + CapRight, S.CaptionSize, Crt::DimGreen))->SetPadding(FMargin(0, 0, 0, 8));
	BoxGrid = UInventoryGridWidget::MakeBag(GetOwningPlayer(), Box ? Box->Capacity : 10);
	BoxGrid->OnSlotClicked.BindUObject(this, &UInventoryTransferWidget::OnBoxSlot);
	BoxGrid->OnSlotHovered.BindUObject(this, &UInventoryTransferWidget::OnBoxHover);
	Outer->AddChildToVerticalBox(BoxGrid)->SetHorizontalAlignment(HAlign_Left);
	Outer->AddChildToVerticalBox(UCrtRuleWidget::Make(GetOwningPlayer(), S.InventoryCaption.IsEmpty() ? CapLeft + TEXT("INVENTORY") + CapRight : S.InventoryCaption, S.CaptionSize, Crt::DimGreen))->SetPadding(FMargin(0, 18, 0, 8));
	PlayerGrid = UInventoryGridWidget::MakeBag(GetOwningPlayer(), ABasePlayerController::InventoryCapacity);
	PlayerGrid->OnSlotClicked.BindUObject(this, &UInventoryTransferWidget::OnPlayerSlot);
	PlayerGrid->OnSlotHovered.BindUObject(this, &UInventoryTransferWidget::OnPlayerHover);
	Outer->AddChildToVerticalBox(PlayerGrid)->SetHorizontalAlignment(HAlign_Left);

	// Info for whatever is hovered, and a quiet status line (full bag, full box).
	if (!S.InfoCaption.IsEmpty()) { Outer->AddChildToVerticalBox(UCrtRuleWidget::Make(GetOwningPlayer(), S.InfoCaption, S.CaptionSize, Crt::DimGreen))->SetPadding(FMargin(0, 18, 0, 8)); }
	InfoName = Crt::FixedText(WidgetTree, TEXT(""), S.InfoNameSize, Crt::Green);
	Outer->AddChildToVerticalBox(InfoName);
	InfoText = Crt::FixedText(WidgetTree, TEXT(""), S.InfoTextSize, Crt::DimGreen);
	InfoText->SetAutoWrapText(true);
	Outer->AddChildToVerticalBox(InfoText)->SetPadding(FMargin(0, 3, 0, 0));
	Note = Crt::FixedText(WidgetTree, TEXT(""), S.RowSize, Crt::DimGreen);
	Outer->AddChildToVerticalBox(Note)->SetPadding(FMargin(0, 8, 0, 0));
	Outer->AddChildToVerticalBox(WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass()))->SetSize(ESlateSizeRule::Fill);

	if (!S.FooterLeft.IsEmpty() || !S.FooterEnd.IsEmpty())
	{
		UHorizontalBox* Footer = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		UHorizontalBoxSlot* FootRule = Footer->AddChildToHorizontalBox(UCrtRuleWidget::Make(GetOwningPlayer(), S.FooterLeft, S.RuleSize, Crt::Faint));
		FootRule->SetSize(ESlateSizeRule::Fill); FootRule->SetVerticalAlignment(VAlign_Center);
		if (!S.FooterEnd.IsEmpty()) { Footer->AddChildToHorizontalBox(Crt::FixedText(WidgetTree, S.FooterEnd, S.RuleSize, Crt::Faint))->SetVerticalAlignment(VAlign_Center); }
		Outer->AddChildToVerticalBox(Footer)->SetPadding(FMargin(0, 12, 0, 0));
	}
}

void UInventoryTransferWidget::Open(ABasePlayerController* InController, ALootBoxActor* InBox)
{
	OwnerController = InController; Box = InBox;
	Rebuild();
	Refresh();
}

void UInventoryTransferWidget::Refresh()
{
	if (BoxGrid && Box) { BoxGrid->SetItems(Box->Items); }
	if (PlayerGrid && OwnerController) { PlayerGrid->SetItems(OwnerController->Inventory); }
}

void UInventoryTransferWidget::OnBoxSlot(int32 Index)
{
	if (!Box || !OwnerController || !Box->Items.IsValidIndex(Index)) { return; }
	if (!OwnerController->AddToInventory(Box->Items[Index])) { if (Note) { Note->SetText(FText::FromString(TEXT("NO ROOM"))); } return; }
	Box->Items.RemoveAt(Index);
	Refresh();
}

void UInventoryTransferWidget::OnPlayerSlot(int32 Index)
{
	if (!Box || !OwnerController || !OwnerController->Inventory.IsValidIndex(Index) || OwnerController->Inventory[Index].IsEmpty()) { return; }
	if (Box->Items.Num() >= Box->Capacity) { if (Note) { Note->SetText(FText::FromString(TEXT("THE BOX IS FULL"))); } return; }
	Box->Items.Add(OwnerController->Inventory[Index]);
	OwnerController->Inventory[Index].Reset();   // the square stays, empty
	Refresh();
}

void UInventoryTransferWidget::OnBoxHover(int32 Index) { ShowInfo(Box && Box->Items.IsValidIndex(Index) ? Box->Items[Index] : FString()); }
void UInventoryTransferWidget::OnPlayerHover(int32 Index) { ShowInfo(OwnerController && OwnerController->Inventory.IsValidIndex(Index) ? OwnerController->Inventory[Index] : FString()); }

void UInventoryTransferWidget::ShowInfo(const FString& Item)
{
	if (InfoName) { InfoName->SetText(FText::FromString(ItemCatalog::InfoTitle(Item))); }
	if (InfoText) { InfoText->SetText(FText::FromString(Item.IsEmpty() ? FString() : ItemCatalog::Describe(Item))); }
}

void UInventoryTransferWidget::OnClose()
{
	if (OwnerController) { OwnerController->CloseTransfer(); }
}

void UInventoryTransferWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	Clock += InDeltaTime;
}

int32 UInventoryTransferWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	const int32 Layer = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
	Crt::PaintFrame(OutDrawElements, AllottedGeometry, FVector2f(0, 0), FVector2f(AllottedGeometry.GetLocalSize()), Layer + 1, Clock);
	return Layer + 2;
}
