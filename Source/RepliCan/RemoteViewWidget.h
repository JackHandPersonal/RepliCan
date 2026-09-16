// The "remote communication" square that sits left of the conversation
// panel: a live camera feed of the character being spoken to, cropped to
// their head, on a small station monitor -- CRT frame, green phosphor
// tint, scanlines, a blinking LIVE mark and a caption. The controller owns
// the scene capture and hands its render target here.
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RemoteViewWidget.generated.h"

class UImage;
class UTextBlock;
class UTextureRenderTarget2D;

UCLASS()
class REPLICAN_API URemoteViewWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetFeed(UTextureRenderTarget2D* Target, const FString& Caption);

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

private:
	UPROPERTY() TObjectPtr<UImage> Feed;
	UPROPERTY() TObjectPtr<UTextBlock> CaptionText;
	UPROPERTY() TObjectPtr<UTextBlock> LiveText;
	float Clock = 0.0f;
};
