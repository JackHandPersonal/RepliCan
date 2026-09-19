#include "Core/DemoPlayerController.h"
#include "Characters/FacialExpressionInterface.h"
#include "UI/FacialControlWidget.h"
#include "Components/InputComponent.h"
#include "Blueprint/UserWidget.h"
#include "EngineUtils.h"

void ADemoPlayerController::BeginPlay()
{
	Super::BeginPlay();

	// Show the mouse cursor and allow UI interaction, otherwise the default
	// input mode captures the mouse for camera look and the widget's
	// buttons never receive clicks.
	bShowMouseCursor = true;
	FInputModeGameAndUI InputMode;
	InputMode.SetHideCursorDuringCapture(false);
	SetInputMode(InputMode);

	UFacialControlWidget* Widget = CreateWidget<UFacialControlWidget>(this, UFacialControlWidget::StaticClass());
	if (Widget)
	{
		// Filling the whole screen (no SetPositionInViewport/SetDesiredSizeInViewport
		// constraining it to a small box) -- the widget's own root CanvasPanel
		// positions the control panel top-left and the collapse button
		// bottom-left independently, which needs the true screen bounds to
		// anchor against, not a small fixed box.
		Widget->AddToViewport(100);
	}
}

void ADemoPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	InputComponent->BindKey(EKeys::One, IE_Pressed, this, &ADemoPlayerController::OnKey1);
	InputComponent->BindKey(EKeys::Two, IE_Pressed, this, &ADemoPlayerController::OnKey2);
	InputComponent->BindKey(EKeys::Three, IE_Pressed, this, &ADemoPlayerController::OnKey3);
	InputComponent->BindKey(EKeys::Four, IE_Pressed, this, &ADemoPlayerController::OnKey4);
	InputComponent->BindKey(EKeys::Five, IE_Pressed, this, &ADemoPlayerController::OnKey5);
	InputComponent->BindKey(EKeys::Six, IE_Pressed, this, &ADemoPlayerController::OnKey6);
	InputComponent->BindKey(EKeys::Seven, IE_Pressed, this, &ADemoPlayerController::OnKey7);
	InputComponent->BindKey(EKeys::Eight, IE_Pressed, this, &ADemoPlayerController::OnKey8);
	InputComponent->BindKey(EKeys::Nine, IE_Pressed, this, &ADemoPlayerController::OnKey9);
	InputComponent->BindKey(EKeys::Q, IE_Pressed, this, &ADemoPlayerController::CycleToNextExpression);
}

void ADemoPlayerController::SelectExpression(int32 KeyIndex)
{
	if (!ExpressionKeyMap.IsValidIndex(KeyIndex))
	{
		return;
	}
	ApplyExpression(ExpressionKeyMap[KeyIndex]);
}

void ADemoPlayerController::CycleToNextExpression()
{
	if (CycleExpressions.Num() == 0)
	{
		return;
	}
	CycleIndex = (CycleIndex + 1) % CycleExpressions.Num();
	ApplyExpression(CycleExpressions[CycleIndex]);
}

void ADemoPlayerController::ApplyExpression(FName ExpressionName)
{
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		if (It->Implements<UFacialExpressionInterface>())
		{
			IFacialExpressionInterface::Execute_SetMouthExpression(*It, ExpressionName);
		}
	}
}
