#include "UI/EditToolWidget.h"
#include "Core/BasePlayerController.h"
#include "UI/CrtStyle.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"

UTextBlock* UEditToolWidget::MakeText(const FText& Text, float FontSize, const FLinearColor& Color)
{
	UTextBlock* Block = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	Block->SetText(Text);
	Block->SetColorAndOpacity(FSlateColor(Color));
	Block->SetFont(Crt::Mono(FMath::RoundToInt(FontSize)));
	return Block;
}

UButton* UEditToolWidget::MakeButton(const FString& Label)
{
	UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
	Button->SetStyle(Crt::BoxedButtonStyle());
	UTextBlock* Text = MakeText(FText::FromString(Label), 12.0f, Crt::Green);
	Button->AddChild(Text);
	if (UButtonSlot* TextSlot = Cast<UButtonSlot>(Text->Slot)) { TextSlot->SetPadding(FMargin(10.0f, 3.0f)); }
	return Button;
}

void UEditToolWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	UBorder* Background = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Background"));
	Background->SetBrushColor(Crt::Panel);
	Background->SetPadding(FMargin(18.0f, 14.0f));
	WidgetTree->RootWidget = Background;

	UVerticalBox* Box = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Background->SetContent(Box);

	UHorizontalBox* Title = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	Title->AddChildToHorizontalBox(Crt::SmallCaps(WidgetTree, TEXT("Edit Mode"), 16, Crt::Green))->SetSize(ESlateSizeRule::Fill);
	Title->AddChildToHorizontalBox(MakeText(FText::FromString(TEXT("[ F7 ] return")), 11.0f, Crt::DimGreen))->SetVerticalAlignment(VAlign_Bottom);
	Box->AddChildToVerticalBox(Title)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 6.0f));

	StatusText = MakeText(FText::FromString(TEXT("fly: WASD, RMB look, space/ctrl up/down. click a character to edit it.")), 10.0f, Crt::DimGreen);
	StatusText->SetAutoWrapText(true);
	Box->AddChildToVerticalBox(StatusText)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 8.0f));

	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	Box->AddChildToVerticalBox(Row);
	UButton* NewButton = MakeButton(TEXT("CHARACTER MANAGER"));
	NewButton->OnClicked.AddDynamic(this, &UEditToolWidget::OnNewCharacter);
	Row->AddChildToHorizontalBox(NewButton)->SetPadding(FMargin(0.0f, 0.0f, 8.0f, 0.0f));
	UButton* AppearanceButton = MakeButton(TEXT("APPEARANCE"));
	AppearanceButton->OnClicked.AddDynamic(this, &UEditToolWidget::OnAppearance);
	Row->AddChildToHorizontalBox(AppearanceButton)->SetPadding(FMargin(0.0f, 0.0f, 8.0f, 0.0f));
	UButton* ReturnButton = MakeButton(TEXT("RETURN TO GAME"));
	ReturnButton->OnClicked.AddDynamic(this, &UEditToolWidget::OnReturnToGame);
	Row->AddChildToHorizontalBox(ReturnButton);
	// Claude Assist: arm it and every click writes what was hit to Saved/ClaudeAssist for Claude to act on.
	UHorizontalBox* Row2 = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	Box->AddChildToVerticalBox(Row2)->SetPadding(FMargin(0.0f, 8.0f, 0.0f, 0.0f));
	UButton* AssistButton = MakeButton(TEXT("CLAUDE ASSIST: CLICK TO MARK"));
	AssistButton->OnClicked.AddDynamic(this, &UEditToolWidget::OnClaudeAssist);
	Row2->AddChildToHorizontalBox(AssistButton)->SetPadding(FMargin(0.0f, 0.0f, 8.0f, 0.0f));
	AssistLabel = MakeText(FText::FromString(TEXT("off")), 11.0f, Crt::DimGreen);
	Row2->AddChildToHorizontalBox(AssistLabel)->SetVerticalAlignment(VAlign_Center);
}

void UEditToolWidget::OnClaudeAssist()
{
	if (!OwnerController) { return; }
	if (OwnerController->IsClaudeAssistActive()) { OwnerController->CancelClaudeAssist(); SetStatus(TEXT("fly: WASD, RMB look, space/ctrl up/down. click a character to edit it.")); }
	else { OwnerController->BeginClaudeAssist(); }
	if (AssistLabel) { AssistLabel->SetText(FText::FromString(OwnerController->IsClaudeAssistActive() ? TEXT("armed: click anything, Claude gets it") : TEXT("off"))); }
}

void UEditToolWidget::NativeConstruct()
{
	Super::NativeConstruct();
}

void UEditToolWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	Clock += InDeltaTime;
}

int32 UEditToolWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	LayerId = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
	Crt::PaintFrame(OutDrawElements, AllottedGeometry, FVector2f::ZeroVector, FVector2f(AllottedGeometry.GetLocalSize()), LayerId + 1, Clock, 0.85f);
	return LayerId + 2;
}

void UEditToolWidget::RefreshFromController()
{
}

void UEditToolWidget::SetStatus(const FString& Text)
{
	if (StatusText) { StatusText->SetText(FText::FromString(Text)); }
}

void UEditToolWidget::OnNewCharacter()
{
	// Opens the Character Manager (placing a new character is its "New"
	// button); the edit tool itself stays the root page.
	if (OwnerController) { OwnerController->ShowCharacterManager(); }
}

void UEditToolWidget::OnReturnToGame()
{
	if (OwnerController) { OwnerController->ExitEditMode(); }
}

void UEditToolWidget::OnAppearance()
{
	// Leave edit mode first: the Appearance screen takes the camera and input for itself.
	if (!OwnerController) { return; }
	OwnerController->ExitEditMode();
	OwnerController->BeginAppearance();
}
