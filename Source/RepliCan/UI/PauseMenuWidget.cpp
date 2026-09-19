#include "PauseMenuWidget.h"
#include "BasePlayerController.h"
#include "CrtStyle.h"
#include "Blueprint/WidgetTree.h"
#include "Components/HorizontalBox.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Styling/CoreStyle.h"

void UPauseMenuWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	// Full-screen dim behind a centred box.
	UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("Root"));
	WidgetTree->RootWidget = Root;

	UBorder* Dim = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Dim"));
	Dim->SetBrushColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.5f));
	UOverlaySlot* DimSlot = Root->AddChildToOverlay(Dim);
	DimSlot->SetHorizontalAlignment(HAlign_Fill);
	DimSlot->SetVerticalAlignment(VAlign_Fill);

	Box = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Box"));
	Box->SetBrushColor(Crt::Panel);
	Box->SetPadding(FMargin(44.0f, 30.0f));
	UOverlaySlot* BoxSlot = Root->AddChildToOverlay(Box);
	BoxSlot->SetHorizontalAlignment(HAlign_Center);
	BoxSlot->SetVerticalAlignment(VAlign_Center);

	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Column"));
	Box->SetContent(Column);

	UHorizontalBox* Title = Crt::SmallCaps(WidgetTree, TEXT("Menu"), 24, Crt::Green);
	UVerticalBoxSlot* TitleSlot = Column->AddChildToVerticalBox(Title);
	TitleSlot->SetHorizontalAlignment(HAlign_Center);
	TitleSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 18.0f));

	UButton* Back = MakeButton(TEXT("BACK"));
	Back->OnClicked.AddDynamic(this, &UPauseMenuWidget::OnBack);
	UVerticalBoxSlot* BackSlot = Column->AddChildToVerticalBox(Back);
	BackSlot->SetHorizontalAlignment(HAlign_Fill);
	BackSlot->SetPadding(FMargin(0.0f, 4.0f));

	// Out of the geometry and back to the last solid ground; the menu closes so the move is seen.
	UButton* Unstuck = MakeButton(TEXT("UNSTUCK"));
	Unstuck->OnClicked.AddDynamic(this, &UPauseMenuWidget::OnUnstuck);
	UVerticalBoxSlot* UnstuckSlot = Column->AddChildToVerticalBox(Unstuck);
	UnstuckSlot->SetHorizontalAlignment(HAlign_Fill);
	UnstuckSlot->SetPadding(FMargin(0.0f, 4.0f));

	UButton* Save = MakeButton(TEXT("SAVE GAME"));
	Save->OnClicked.AddDynamic(this, &UPauseMenuWidget::OnSave);
	UVerticalBoxSlot* SaveSlot = Column->AddChildToVerticalBox(Save);
	SaveSlot->SetHorizontalAlignment(HAlign_Fill);
	SaveSlot->SetPadding(FMargin(0.0f, 4.0f));

	UButton* Load = MakeButton(TEXT("LOAD GAME"));
	Load->OnClicked.AddDynamic(this, &UPauseMenuWidget::OnLoad);
	UVerticalBoxSlot* LoadSlot = Column->AddChildToVerticalBox(Load);
	LoadSlot->SetHorizontalAlignment(HAlign_Fill);
	LoadSlot->SetPadding(FMargin(0.0f, 4.0f));

	UButton* Scenes = MakeButton(TEXT("SEQUENCES"));
	Scenes->OnClicked.AddDynamic(this, &UPauseMenuWidget::OnScenes);
	UVerticalBoxSlot* ScenesSlot = Column->AddChildToVerticalBox(Scenes);
	ScenesSlot->SetHorizontalAlignment(HAlign_Fill);
	ScenesSlot->SetPadding(FMargin(0.0f, 4.0f));

	UButton* Settings = MakeButton(TEXT("SETTINGS"));
	Settings->OnClicked.AddDynamic(this, &UPauseMenuWidget::OnSettings);
	UVerticalBoxSlot* SettingsSlot = Column->AddChildToVerticalBox(Settings);
	SettingsSlot->SetHorizontalAlignment(HAlign_Fill);
	SettingsSlot->SetPadding(FMargin(0.0f, 4.0f));

	UButton* Quit = MakeButton(TEXT("QUIT"));
	Quit->OnClicked.AddDynamic(this, &UPauseMenuWidget::OnQuit);
	UVerticalBoxSlot* QuitSlot = Column->AddChildToVerticalBox(Quit);
	QuitSlot->SetHorizontalAlignment(HAlign_Fill);
	QuitSlot->SetPadding(FMargin(0.0f, 4.0f));
}

void UPauseMenuWidget::OnScenes()
{
	if (OwnerController) { OwnerController->ShowScenesPanel(); }
}

void UPauseMenuWidget::OnSettings()
{
	if (OwnerController) { OwnerController->ShowSettingsPanel(); }
}

void UPauseMenuWidget::OnSave()
{
	if (OwnerController) { OwnerController->QuickSave(); }
}

void UPauseMenuWidget::OnLoad()
{
	if (OwnerController) { OwnerController->QuickLoad(); }
}

UButton* UPauseMenuWidget::MakeButton(const FString& Label)
{
	UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
	Button->SetStyle(Crt::ButtonStyle());
	UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	Text->SetText(FText::FromString(Label));
	Text->SetFont(Crt::Mono(18));
	Text->SetColorAndOpacity(FSlateColor(Crt::Green));
	Text->SetJustification(ETextJustify::Center);
	Button->AddChild(Text);
	if (UButtonSlot* TextSlot = Cast<UButtonSlot>(Text->Slot))
	{
		TextSlot->SetPadding(FMargin(28.0f, 8.0f));
		TextSlot->SetHorizontalAlignment(HAlign_Center);
	}
	return Button;
}

void UPauseMenuWidget::OnBack()
{
	if (OwnerController) { OwnerController->HidePauseMenu(); }
}

void UPauseMenuWidget::OnUnstuck()
{
	if (OwnerController) { OwnerController->Unstuck(); OwnerController->HidePauseMenu(); }
}

void UPauseMenuWidget::OnQuit()
{
	if (OwnerController) { OwnerController->QuitGame(); }
}

void UPauseMenuWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	Clock += InDeltaTime;
}

int32 UPauseMenuWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	LayerId = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
	Crt::PaintFrameAround(OutDrawElements, AllottedGeometry, Box, LayerId + 1, Clock);
	return LayerId + 2;
}
