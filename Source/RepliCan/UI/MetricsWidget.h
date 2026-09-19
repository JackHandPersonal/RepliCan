// A CRT-green performance readout in the top-left: frame rate and the three
// frame times, draw calls, memory, and a line per live subsystem (director,
// conversation, voice, remote view, booth, chooser, characters, doors, lamps).
// Amber warnings appear when something is out of budget. F11 toggles it.
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MetricsWidget.generated.h"

class ABasePlayerController;

UCLASS()
class REPLICAN_API UMetricsWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetOwnerController(ABasePlayerController* In) { OwnerController = In; }

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

private:
	struct FLine { FString Text; bool bWarning = false; bool bDim = false; };
	void Sample();

	UPROPERTY() TObjectPtr<ABasePlayerController> OwnerController;
	TArray<FLine> Lines;
	float Refresh = 0.0f;       // seconds until the next sample
	float SmoothedDelta = 1.0f / 60.0f;
	float WorstDelta = 0.0f;    // worst frame since the last sample
	float Clock = 0.0f;
};
