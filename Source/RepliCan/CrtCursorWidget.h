// The in-game mouse cursor: a phosphor-green arrow with a dark rim and a
// faint trailing glow, drawn in NativePaint so it needs no asset. Wired as
// the software cursor for Default / Crosshairs / EyeDropper / Hand in
// DefaultEngine.ini ([/Script/Engine.UserInterfaceSettings]).
//
// The hotspot is the widget's CENTRE, not its top-left: FSlateUser::DrawCursor
// offsets a software cursor by -0.5 * its desired size before painting it, so
// whatever is drawn at the middle of the widget is what sits on the point Slate
// measures clicks at. The box is therefore deliberately larger than the art and
// the art hangs off its centre; see NativePaint.
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CrtCursorWidget.generated.h"

UCLASS()
class REPLICAN_API UCrtCursorWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

private:
	float Clock = 0.0f;
};
