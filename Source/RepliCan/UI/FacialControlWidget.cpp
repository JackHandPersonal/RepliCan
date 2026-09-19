#include "UI/FacialControlWidget.h"
#include "Characters/FaceController.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "UI/CrtStyle.h"
#include "Blueprint/WidgetTree.h"
#include "Kismet/GameplayStatics.h"

namespace
{
	constexpr int32 ButtonFontSize = 10;
	constexpr int32 SectionLabelFontSize = 12;

	void SetFontSize(UTextBlock* Text, int32 Size)
	{
		Text->SetFont(Crt::Mono(Size));   // the UI's terminal face
	}

	UButton* AddLabeledButton(UWidgetTree* Tree, UHorizontalBox* Row, const FString& Label)
	{
		UButton* Button = Tree->ConstructWidget<UButton>(UButton::StaticClass());
		UTextBlock* Text = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		Text->SetText(FText::FromString(Label));
		Text->SetColorAndOpacity(FSlateColor(Crt::Green));
		SetFontSize(Text, ButtonFontSize);
		Button->AddChild(Text);

		UHorizontalBoxSlot* Slot = Row->AddChildToHorizontalBox(Button);
		Slot->SetPadding(FMargin(2.f));
		return Button;
	}
}

void UFacialControlWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	BuildFullLayout();
}

void UFacialControlWidget::BuildFullLayout()
{
	// Root is a full-screen canvas so the collapse button (anchored to the
	// screen's bottom-left corner) can be positioned independently of the
	// control panel itself (anchored top-left) -- a plain vertical box can
	// only stack children relative to each other, not pin one to a
	// different screen corner than the rest.
	UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
	WidgetTree->RootWidget = Canvas;

	UBorder* Background = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Background->SetBrushColor(Crt::Panel);
	ContentPanel = Background;

	UCanvasPanelSlot* ContentSlot = Canvas->AddChildToCanvas(Background);
	ContentSlot->SetAnchors(FAnchors(0.f, 0.f, 0.f, 0.f));
	ContentSlot->SetAlignment(FVector2D(0.f, 0.f));
	ContentSlot->SetPosition(FVector2D(20.f, 20.f));
	ContentSlot->SetAutoSize(true);

	UVerticalBox* Root = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Background->SetContent(Root);

	UTextBlock* MouthLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	MouthLabel->SetText(FText::FromString(TEXT("Mouth")));
	MouthLabel->SetColorAndOpacity(FSlateColor(Crt::Green));
	SetFontSize(MouthLabel, SectionLabelFontSize);
	Root->AddChildToVerticalBox(MouthLabel);

	UHorizontalBox* MouthRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	Root->AddChildToVerticalBox(MouthRow);

	AddLabeledButton(WidgetTree, MouthRow, TEXT("Neutral"))->OnClicked.AddDynamic(this, &UFacialControlWidget::OnClickNeutral);
	AddLabeledButton(WidgetTree, MouthRow, TEXT("Sad"))->OnClicked.AddDynamic(this, &UFacialControlWidget::OnClickSad);
	AddLabeledButton(WidgetTree, MouthRow, TEXT("Happy"))->OnClicked.AddDynamic(this, &UFacialControlWidget::OnClickHappy);
	AddLabeledButton(WidgetTree, MouthRow, TEXT("Surprised"))->OnClicked.AddDynamic(this, &UFacialControlWidget::OnClickSurprised);
	AddLabeledButton(WidgetTree, MouthRow, TEXT("Smirk"))->OnClicked.AddDynamic(this, &UFacialControlWidget::OnClickSmirk);
	AddLabeledButton(WidgetTree, MouthRow, TEXT("Frown"))->OnClicked.AddDynamic(this, &UFacialControlWidget::OnClickFrown);
	AddLabeledButton(WidgetTree, MouthRow, TEXT("Angry"))->OnClicked.AddDynamic(this, &UFacialControlWidget::OnClickAngry);
	AddLabeledButton(WidgetTree, MouthRow, TEXT("None"))->OnClicked.AddDynamic(this, &UFacialControlWidget::OnClickMouthNone);
	AddLabeledButton(WidgetTree, MouthRow, TEXT("Talking"))->OnClicked.AddDynamic(this, &UFacialControlWidget::OnClickTalking);

	UHorizontalBox* MouthRow2 = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	Root->AddChildToVerticalBox(MouthRow2);

	AddLabeledButton(WidgetTree, MouthRow2, TEXT("Open1"))->OnClicked.AddDynamic(this, &UFacialControlWidget::OnClickOpen1);
	AddLabeledButton(WidgetTree, MouthRow2, TEXT("Open2"))->OnClicked.AddDynamic(this, &UFacialControlWidget::OnClickOpen2);
	AddLabeledButton(WidgetTree, MouthRow2, TEXT("Open3"))->OnClicked.AddDynamic(this, &UFacialControlWidget::OnClickOpen3);
	AddLabeledButton(WidgetTree, MouthRow2, TEXT("Talking1"))->OnClicked.AddDynamic(this, &UFacialControlWidget::OnClickTalking1);
	AddLabeledButton(WidgetTree, MouthRow2, TEXT("Talking2"))->OnClicked.AddDynamic(this, &UFacialControlWidget::OnClickTalking2);
	AddLabeledButton(WidgetTree, MouthRow2, TEXT("Talking3"))->OnClicked.AddDynamic(this, &UFacialControlWidget::OnClickTalking3);

	UTextBlock* EyeLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	EyeLabel->SetText(FText::FromString(TEXT("Eyes")));
	EyeLabel->SetColorAndOpacity(FSlateColor(Crt::Green));
	SetFontSize(EyeLabel, SectionLabelFontSize);
	Root->AddChildToVerticalBox(EyeLabel);

	UHorizontalBox* EyeRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	Root->AddChildToVerticalBox(EyeRow);

	AddLabeledButton(WidgetTree, EyeRow, TEXT("Wide"))->OnClicked.AddDynamic(this, &UFacialControlWidget::OnClickWide);
	AddLabeledButton(WidgetTree, EyeRow, TEXT("Normal"))->OnClicked.AddDynamic(this, &UFacialControlWidget::OnClickNormal);
	AddLabeledButton(WidgetTree, EyeRow, TEXT("Narrow"))->OnClicked.AddDynamic(this, &UFacialControlWidget::OnClickNarrow);
	AddLabeledButton(WidgetTree, EyeRow, TEXT("Squint"))->OnClicked.AddDynamic(this, &UFacialControlWidget::OnClickSquint);

	UTextBlock* BrowHeightLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	BrowHeightLabel->SetText(FText::FromString(TEXT("Brow Height")));
	BrowHeightLabel->SetColorAndOpacity(FSlateColor(Crt::Green));
	SetFontSize(BrowHeightLabel, SectionLabelFontSize);
	Root->AddChildToVerticalBox(BrowHeightLabel);

	UHorizontalBox* BrowHeightRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	Root->AddChildToVerticalBox(BrowHeightRow);

	AddLabeledButton(WidgetTree, BrowHeightRow, TEXT("Low"))->OnClicked.AddDynamic(this, &UFacialControlWidget::OnClickBrowLow);
	AddLabeledButton(WidgetTree, BrowHeightRow, TEXT("Normal"))->OnClicked.AddDynamic(this, &UFacialControlWidget::OnClickBrowHeightNormal);
	AddLabeledButton(WidgetTree, BrowHeightRow, TEXT("High"))->OnClicked.AddDynamic(this, &UFacialControlWidget::OnClickBrowHigh);
	AddLabeledButton(WidgetTree, BrowHeightRow, TEXT("Highest"))->OnClicked.AddDynamic(this, &UFacialControlWidget::OnClickBrowHighest);

	UTextBlock* BrowAngleLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	BrowAngleLabel->SetText(FText::FromString(TEXT("Brow Angle")));
	BrowAngleLabel->SetColorAndOpacity(FSlateColor(Crt::Green));
	SetFontSize(BrowAngleLabel, SectionLabelFontSize);
	Root->AddChildToVerticalBox(BrowAngleLabel);

	UHorizontalBox* BrowAngleRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	Root->AddChildToVerticalBox(BrowAngleRow);

	AddLabeledButton(WidgetTree, BrowAngleRow, TEXT("High Left"))->OnClicked.AddDynamic(this, &UFacialControlWidget::OnClickBrowHighLeft);
	AddLabeledButton(WidgetTree, BrowAngleRow, TEXT("Left"))->OnClicked.AddDynamic(this, &UFacialControlWidget::OnClickBrowLeft);
	AddLabeledButton(WidgetTree, BrowAngleRow, TEXT("Normal"))->OnClicked.AddDynamic(this, &UFacialControlWidget::OnClickBrowAngleNormal);
	AddLabeledButton(WidgetTree, BrowAngleRow, TEXT("Right"))->OnClicked.AddDynamic(this, &UFacialControlWidget::OnClickBrowRight);
	AddLabeledButton(WidgetTree, BrowAngleRow, TEXT("High Right"))->OnClicked.AddDynamic(this, &UFacialControlWidget::OnClickBrowHighRight);

	UTextBlock* HeadGearLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	HeadGearLabel->SetText(FText::FromString(TEXT("Head Gear (each cycles through its own options, incl. None)")));
	HeadGearLabel->SetColorAndOpacity(FSlateColor(Crt::Green));
	SetFontSize(HeadGearLabel, SectionLabelFontSize);
	Root->AddChildToVerticalBox(HeadGearLabel);

	UHorizontalBox* HeadGearRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	Root->AddChildToVerticalBox(HeadGearRow);

	AddLabeledButton(WidgetTree, HeadGearRow, TEXT("Cycle Hair"))->OnClicked.AddDynamic(this, &UFacialControlWidget::OnClickCycleHair);
	AddLabeledButton(WidgetTree, HeadGearRow, TEXT("Cycle Facial Hair"))->OnClicked.AddDynamic(this, &UFacialControlWidget::OnClickCycleFacialHair);
	AddLabeledButton(WidgetTree, HeadGearRow, TEXT("Cycle Head Gear"))->OnClicked.AddDynamic(this, &UFacialControlWidget::OnClickCycleHeadGear);
	AddLabeledButton(WidgetTree, HeadGearRow, TEXT("Cycle Skin"))->OnClicked.AddDynamic(this, &UFacialControlWidget::OnClickCycleMaterial);

	UTextBlock* OtherLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	OtherLabel->SetText(FText::FromString(TEXT("Other")));
	OtherLabel->SetColorAndOpacity(FSlateColor(Crt::Green));
	SetFontSize(OtherLabel, SectionLabelFontSize);
	Root->AddChildToVerticalBox(OtherLabel);

	UHorizontalBox* OtherRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	Root->AddChildToVerticalBox(OtherRow);

	AddLabeledButton(WidgetTree, OtherRow, TEXT("Toggle Highlight"))->OnClicked.AddDynamic(this, &UFacialControlWidget::OnClickToggleHighlight);

	// Collapse toggle: its own canvas slot anchored to the screen's
	// bottom-left corner, independent of the panel's top-left slot above,
	// so it stays visible in a fixed spot whether the panel is shown or
	// collapsed.
	UButton* CollapseButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
	CollapseButtonText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	CollapseButtonText->SetColorAndOpacity(FSlateColor(Crt::Green));
	SetFontSize(CollapseButtonText, ButtonFontSize);
	CollapseButton->AddChild(CollapseButtonText);
	CollapseButton->OnClicked.AddDynamic(this, &UFacialControlWidget::OnClickToggleCollapse);

	UCanvasPanelSlot* CollapseSlot = Canvas->AddChildToCanvas(CollapseButton);
	CollapseSlot->SetAnchors(FAnchors(0.f, 1.f, 0.f, 1.f));
	CollapseSlot->SetAlignment(FVector2D(0.f, 1.f));
	CollapseSlot->SetPosition(FVector2D(20.f, -20.f));
	CollapseSlot->SetAutoSize(true);

	// Apply the default collapsed state (bPanelCollapsed's initializer) to
	// both the panel's visibility and the button's label, rather than
	// hardcoding "Hide UI"/Visible here and letting it drift out of sync
	// with whatever bPanelCollapsed actually defaults to.
	ContentPanel->SetVisibility(bPanelCollapsed ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	CollapseButtonText->SetText(FText::FromString(bPanelCollapsed ? TEXT("Show UI") : TEXT("Hide UI")));

	UE_LOG(LogTemp, Warning, TEXT("FacialControlWidget: BuildFullLayout complete"));
}

void UFacialControlWidget::OnClickToggleCollapse()
{
	bPanelCollapsed = !bPanelCollapsed;
	ContentPanel->SetVisibility(bPanelCollapsed ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	CollapseButtonText->SetText(FText::FromString(bPanelCollapsed ? TEXT("Show UI") : TEXT("Hide UI")));
}

void UFacialControlWidget::ApplyExpressionToAll(FName ExpressionName)
{
	// Mouth-only: SetExpression would also re-apply that expression's
	// eyes/brow pose, which fights with the Eyes row's own buttons since
	// both would be writing to the same FaceAnimInstance fields.
	TArray<AActor*> Controllers;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AFaceController::StaticClass(), Controllers);
	for (AActor* Actor : Controllers)
	{
		if (AFaceController* Controller = Cast<AFaceController>(Actor))
		{
			Controller->SetMouthTexture(ExpressionName);
		}
	}
}

void UFacialControlWidget::ApplyEyeOpennessToAll(EEyeOpenness Openness)
{
	TArray<AActor*> Controllers;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AFaceController::StaticClass(), Controllers);
	for (AActor* Actor : Controllers)
	{
		if (AFaceController* Controller = Cast<AFaceController>(Actor))
		{
			Controller->SetEyeOpenness(Openness);
		}
	}
}

void UFacialControlWidget::OnClickNeutral() { ApplyExpressionToAll(TEXT("Neutral")); }
void UFacialControlWidget::OnClickSad() { ApplyExpressionToAll(TEXT("Sad")); }
void UFacialControlWidget::OnClickHappy() { ApplyExpressionToAll(TEXT("Happy")); }
void UFacialControlWidget::OnClickSurprised() { ApplyExpressionToAll(TEXT("Surprised")); }
void UFacialControlWidget::OnClickSmirk() { ApplyExpressionToAll(TEXT("Smirk")); }
void UFacialControlWidget::OnClickFrown() { ApplyExpressionToAll(TEXT("Frown")); }
void UFacialControlWidget::OnClickAngry() { ApplyExpressionToAll(TEXT("Angry")); }
void UFacialControlWidget::OnClickMouthNone() { ApplyExpressionToAll(TEXT("None")); }

void UFacialControlWidget::StartTalkingOnAll()
{
	TArray<AActor*> Controllers;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AFaceController::StaticClass(), Controllers);
	for (AActor* Actor : Controllers)
	{
		if (AFaceController* Controller = Cast<AFaceController>(Actor))
		{
			Controller->SetTalkingMode(true);
		}
	}
}

void UFacialControlWidget::OnClickTalking() { StartTalkingOnAll(); }

void UFacialControlWidget::OnClickOpen1() { ApplyExpressionToAll(TEXT("Open1")); }
void UFacialControlWidget::OnClickOpen2() { ApplyExpressionToAll(TEXT("Open2")); }
void UFacialControlWidget::OnClickOpen3() { ApplyExpressionToAll(TEXT("Open3")); }
void UFacialControlWidget::OnClickTalking1() { ApplyExpressionToAll(TEXT("Talking1")); }
void UFacialControlWidget::OnClickTalking2() { ApplyExpressionToAll(TEXT("Talking2")); }
void UFacialControlWidget::OnClickTalking3() { ApplyExpressionToAll(TEXT("Talking3")); }

void UFacialControlWidget::OnClickWide() { ApplyEyeOpennessToAll(EEyeOpenness::Wide); }
void UFacialControlWidget::OnClickNormal() { ApplyEyeOpennessToAll(EEyeOpenness::Normal); }
void UFacialControlWidget::OnClickNarrow() { ApplyEyeOpennessToAll(EEyeOpenness::Narrow); }
void UFacialControlWidget::OnClickSquint() { ApplyEyeOpennessToAll(EEyeOpenness::Squint); }

void UFacialControlWidget::ApplyBrowHeightToAll(EBrowHeight Height)
{
	TArray<AActor*> Controllers;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AFaceController::StaticClass(), Controllers);
	for (AActor* Actor : Controllers)
	{
		if (AFaceController* Controller = Cast<AFaceController>(Actor))
		{
			Controller->SetBrowHeight(Height);
		}
	}
}

void UFacialControlWidget::ApplyBrowAngleToAll(EBrowAngle Angle)
{
	TArray<AActor*> Controllers;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AFaceController::StaticClass(), Controllers);
	for (AActor* Actor : Controllers)
	{
		if (AFaceController* Controller = Cast<AFaceController>(Actor))
		{
			Controller->SetBrowAngle(Angle);
		}
	}
}

void UFacialControlWidget::OnClickBrowLow() { ApplyBrowHeightToAll(EBrowHeight::Low); }
void UFacialControlWidget::OnClickBrowHeightNormal() { ApplyBrowHeightToAll(EBrowHeight::Normal); }
void UFacialControlWidget::OnClickBrowHigh() { ApplyBrowHeightToAll(EBrowHeight::High); }
void UFacialControlWidget::OnClickBrowHighest() { ApplyBrowHeightToAll(EBrowHeight::Highest); }

void UFacialControlWidget::OnClickBrowHighLeft() { ApplyBrowAngleToAll(EBrowAngle::HighLeft); }
void UFacialControlWidget::OnClickBrowLeft() { ApplyBrowAngleToAll(EBrowAngle::Left); }
void UFacialControlWidget::OnClickBrowAngleNormal() { ApplyBrowAngleToAll(EBrowAngle::Normal); }
void UFacialControlWidget::OnClickBrowRight() { ApplyBrowAngleToAll(EBrowAngle::Right); }
void UFacialControlWidget::OnClickBrowHighRight() { ApplyBrowAngleToAll(EBrowAngle::HighRight); }

void UFacialControlWidget::CycleHairOnAll()
{
	TArray<AActor*> Controllers;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AFaceController::StaticClass(), Controllers);
	for (AActor* Actor : Controllers)
	{
		if (AFaceController* Controller = Cast<AFaceController>(Actor))
		{
			Controller->CycleHair();
		}
	}
}

void UFacialControlWidget::CycleFacialHairOnAll()
{
	TArray<AActor*> Controllers;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AFaceController::StaticClass(), Controllers);
	for (AActor* Actor : Controllers)
	{
		if (AFaceController* Controller = Cast<AFaceController>(Actor))
		{
			Controller->CycleFacialHair();
		}
	}
}

void UFacialControlWidget::CycleHeadGearOnAll()
{
	TArray<AActor*> Controllers;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AFaceController::StaticClass(), Controllers);
	for (AActor* Actor : Controllers)
	{
		if (AFaceController* Controller = Cast<AFaceController>(Actor))
		{
			Controller->CycleHeadGear();
		}
	}
}

void UFacialControlWidget::OnClickCycleHair() { CycleHairOnAll(); }
void UFacialControlWidget::OnClickCycleFacialHair() { CycleFacialHairOnAll(); }
void UFacialControlWidget::OnClickCycleHeadGear() { CycleHeadGearOnAll(); }

void UFacialControlWidget::CycleMaterialOnAll()
{
	TArray<AActor*> Controllers;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AFaceController::StaticClass(), Controllers);
	for (AActor* Actor : Controllers)
	{
		if (AFaceController* Controller = Cast<AFaceController>(Actor))
		{
			Controller->CycleMaterial();
		}
	}
}

void UFacialControlWidget::OnClickCycleMaterial() { CycleMaterialOnAll(); }

void UFacialControlWidget::ToggleHighlightOnAll()
{
	TArray<AActor*> Controllers;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AFaceController::StaticClass(), Controllers);

	// Flip based on the first controller's current state, so a mixed-state
	// level still converges to all-on or all-off in one click rather than
	// each controller toggling independently out of sync with the others.
	bool bNewVisible = true;
	if (Controllers.Num() > 0)
	{
		if (AFaceController* First = Cast<AFaceController>(Controllers[0]))
		{
			bNewVisible = !First->IsHighlightVisible();
		}
	}

	for (AActor* Actor : Controllers)
	{
		if (AFaceController* Controller = Cast<AFaceController>(Actor))
		{
			Controller->SetHighlightVisible(bNewVisible);
		}
	}
}

void UFacialControlWidget::OnClickToggleHighlight() { ToggleHighlightOnAll(); }
