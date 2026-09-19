// Sleepy eyes: two soft, rounded eyelids that drift open and closed over
// the whole screen, plus a darkness veil that lifts over time. Used by the
// waking-up sequence (SequenceDirector "blink" step). Everything is drawn
// in NativePaint from boxes, no assets. Closed more often than not at the
// start; the parameters set how open the eyes get and how dark it stays.
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BlinkOverlayWidget.generated.h"

USTRUCT()
struct FBlinkParams
{
	GENERATED_BODY()
	UPROPERTY() float OpenMin = 0.25f;       // how far the eyes open (0 closed .. 1 fully)
	UPROPERTY() float OpenMax = 0.65f;
	UPROPERTY() float ClosedSeconds = 3.0f;  // average time spent shut per cycle
	UPROPERTY() float OpenSeconds = 1.4f;    // average time spent open per cycle
	UPROPERTY() float DarkStart = 0.8f;      // veil alpha at the start ...
	UPROPERTY() float DarkEnd = 0.3f;        // ... and after DarkSeconds
	UPROPERTY() float DarkSeconds = 25.0f;
	// Seconds for the next lift of the lids; < 0 = the slow random heave. A short value snaps them open.
	UPROPERTY() float LiftSeconds = -1.0f;
};

UCLASS()
class REPLICAN_API UBlinkOverlayWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void Configure(const FBlinkParams& InParams);
	float GetOpenness() const { return Openness; }
	float GetDarkness() const;
	// Completed blinks (open then shut again) since the overlay went up.
	int32 GetBlinkCount() const { return Blinks; }
	// The activation jolt: lids snap wide and flutter, a green-white flash
	// and torn scanlines that die away over Seconds.
	void Jolt(float Seconds);

protected:
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

private:
	FBlinkParams Params;
	float Clock = 0.0f;
	// Eyelid state machine: Closed -> Opening -> Open -> Closing -> Closed.
	enum class ELid : uint8 { Closed, Opening, Open, Closing };
	ELid Lid = ELid::Closed;
	float PhaseLeft = 2.0f;      // seconds left in the current phase
	float PhaseLength = 2.0f;
	float OpenTarget = 0.4f;     // this cycle's open level
	float Openness = 0.0f;       // 0 shut .. 1 wide
	float OpenFrom = 0.0f;
	int32 Blinks = 0;
	float DarkFrom = 1.0f;       // veil alpha when the current parameters arrived
	float JoltLeft = 0.0f;
	float JoltTotal = 0.0f;
	float JoltClock = 0.0f;
};
