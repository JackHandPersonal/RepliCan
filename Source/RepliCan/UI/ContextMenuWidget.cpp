#include "ContextMenuWidget.h"
#include "CrtStyle.h"
#include "Blueprint/WidgetTree.h"
#include "Blueprint/SlateBlueprintLibrary.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"

static TWeakObjectPtr<UContextMenuWidget> GOpenContextMenu;

void UContextMenuRowBinding::OnClicked() { if (Menu) { Menu->Pick(Option); } }

UContextMenuWidget* UContextMenuWidget::Show(APlayerController* Owner, const FVector2D& ScreenPos, const FString& Title, const TArray<FString>& Options, TFunction<void(const FString&)> OnPick)
{
	if (GOpenContextMenu.IsValid()) { GOpenContextMenu->Close(); }
	if (!Owner) { return nullptr; }
	UContextMenuWidget* W = CreateWidget<UContextMenuWidget>(Owner, UContextMenuWidget::StaticClass());
	if (!W) { return nullptr; }
	W->OnPickFn = MoveTemp(OnPick);
	W->Build(Title, Options);
	W->AddToViewport(300);
	// Slate's absolute position to the viewport's own, so the panel lands under the pointer.
	FVector2D Pixel, Viewport;
	USlateBlueprintLibrary::AbsoluteToViewport(Owner, ScreenPos, Pixel, Viewport);
	if (UCanvasPanelSlot* PS = Cast<UCanvasPanelSlot>(W->Panel->Slot)) { PS->SetAutoSize(true); PS->SetPosition(Viewport + FVector2D(6.0f, 4.0f)); }
	W->SetKeyboardFocus();
	GOpenContextMenu = W;
	return W;
}

void UContextMenuWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	SetIsFocusable(true);
	// The canvas fills the screen and is hit-testable, so a click anywhere off the panel reaches
	// NativeOnMouseButtonDown and closes the menu.
	Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("MenuCanvas"));
	Canvas->SetVisibility(ESlateVisibility::Visible);
	WidgetTree->RootWidget = Canvas;
	Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("MenuPanel"));
	Panel->SetBrushColor(Crt::Panel);
	Panel->SetPadding(FMargin(10.0f, 6.0f));
	Panel->SetVisibility(ESlateVisibility::Visible);
	Canvas->AddChild(Panel);
	Rows = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("MenuRows"));
	Panel->SetContent(Rows);
}

void UContextMenuWidget::Build(const FString& Title, const TArray<FString>& Options)
{
	if (!Rows) { return; }
	Rows->ClearChildren(); Bindings.Reset();
	if (!Title.IsEmpty()) { Rows->AddChildToVerticalBox(Crt::FixedText(WidgetTree, Title.ToUpper(), 12, Crt::DimGreen))->SetPadding(FMargin(4.0f, 0.0f, 4.0f, 4.0f)); }
	for (const FString& Option : Options)
	{
		UButton* B = Crt::Button(WidgetTree, FString::Printf(TEXT("[ %s ]"), *Option.ToUpper()), 15, Crt::Green);
		UContextMenuRowBinding* Binding = NewObject<UContextMenuRowBinding>(this);
		Binding->Menu = this; Binding->Option = Option;
		B->OnClicked.AddDynamic(Binding, &UContextMenuRowBinding::OnClicked);
		Bindings.Add(Binding);
		UVerticalBoxSlot* S = Rows->AddChildToVerticalBox(B);
		S->SetHorizontalAlignment(HAlign_Fill); S->SetPadding(FMargin(0.0f, 1.0f));
	}
}

void UContextMenuWidget::Close()
{
	if (GOpenContextMenu.Get() == this) { GOpenContextMenu = nullptr; }
	RemoveFromParent();
}

void UContextMenuWidget::Pick(const FString& Option)
{
	TFunction<void(const FString&)> Fn = MoveTemp(OnPickFn);
	Close();
	if (Fn) { Fn(Option); }
}

FReply UContextMenuWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	// On the panel itself (its padding, the title): nothing. Anywhere else: the menu goes away.
	if (Panel && Panel->GetCachedGeometry().IsUnderLocation(InMouseEvent.GetScreenSpacePosition())) { return FReply::Handled(); }
	Close();
	return FReply::Handled();
}

FReply UContextMenuWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Escape) { Close(); return FReply::Handled(); }
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}
