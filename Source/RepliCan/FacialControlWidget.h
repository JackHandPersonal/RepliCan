// Simple on-screen control panel: one button per mouth expression, one
// button per eye-openness state. Clicking a button applies that state to
// every AFaceController in the level. Built entirely in C++ (NativeConstruct)
// rather than as a UMG Widget Blueprint, since Widget Designer/graph editing
// isn't something that can be driven reliably through this project's Python
// remote-exec workflow -- same reasoning as building ABP_KnightFace's logic
// in a native AnimInstance base class instead of pure Blueprint.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "FacialControlWidget.generated.h"

enum class EEyeOpenness : uint8;
enum class EBrowHeight : uint8;
enum class EBrowAngle : uint8;
class UWidget;
class UTextBlock;

UCLASS()
class REPLICAN_API UFacialControlWidget : public UUserWidget
{
	GENERATED_BODY()

protected:

	// Building the WidgetTree here rather than in NativeConstruct: Slate's
	// widget hierarchy gets built from WidgetTree before NativeConstruct
	// runs, so changes made there are too late to be picked up.
	// NativeOnInitialized runs earlier, before that hierarchy is built.
	virtual void NativeOnInitialized() override;

private:

	void BuildFullLayout();

	void ApplyExpressionToAll(FName ExpressionName);
	void ApplyEyeOpennessToAll(EEyeOpenness Openness);
	void ApplyBrowHeightToAll(EBrowHeight Height);
	void ApplyBrowAngleToAll(EBrowAngle Angle);
	void StartTalkingOnAll();

	UFUNCTION() void OnClickNeutral();
	UFUNCTION() void OnClickSad();
	UFUNCTION() void OnClickHappy();
	UFUNCTION() void OnClickSurprised();
	UFUNCTION() void OnClickSmirk();
	UFUNCTION() void OnClickFrown();
	UFUNCTION() void OnClickAngry();
	UFUNCTION() void OnClickMouthNone();
	UFUNCTION() void OnClickTalking();

	UFUNCTION() void OnClickOpen1();
	UFUNCTION() void OnClickOpen2();
	UFUNCTION() void OnClickOpen3();
	UFUNCTION() void OnClickTalking1();
	UFUNCTION() void OnClickTalking2();
	UFUNCTION() void OnClickTalking3();

	UFUNCTION() void OnClickWide();
	UFUNCTION() void OnClickNormal();
	UFUNCTION() void OnClickNarrow();
	UFUNCTION() void OnClickSquint();

	UFUNCTION() void OnClickBrowLow();
	UFUNCTION() void OnClickBrowHeightNormal();
	UFUNCTION() void OnClickBrowHigh();
	UFUNCTION() void OnClickBrowHighest();

	UFUNCTION() void OnClickBrowHighLeft();
	UFUNCTION() void OnClickBrowLeft();
	UFUNCTION() void OnClickBrowAngleNormal();
	UFUNCTION() void OnClickBrowRight();
	UFUNCTION() void OnClickBrowHighRight();

	void CycleHairOnAll();
	void CycleFacialHairOnAll();
	void CycleHeadGearOnAll();
	void CycleMaterialOnAll();

	UFUNCTION() void OnClickCycleHair();
	UFUNCTION() void OnClickCycleFacialHair();
	UFUNCTION() void OnClickCycleHeadGear();
	UFUNCTION() void OnClickCycleMaterial();

	void ToggleHighlightOnAll();
	UFUNCTION() void OnClickToggleHighlight();

	// The whole control panel (all sections/rows), collapsible independently
	// of the toggle button itself -- the button lives in a separate
	// screen-anchored canvas slot (bottom-left) so it stays put and visible
	// regardless of whether the panel above it is currently shown.
	UPROPERTY()
	TObjectPtr<UWidget> ContentPanel;

	UPROPERTY()
	TObjectPtr<UTextBlock> CollapseButtonText;

	bool bPanelCollapsed = true;

	UFUNCTION() void OnClickToggleCollapse();
};
