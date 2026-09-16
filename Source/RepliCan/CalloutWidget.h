// A short line of text floating over a world actor for a few seconds --
// what a character "says" for the inspect menu's Talk, or the note an
// Inspect / Take / custom action leaves. Owned and positioned by
// ABasePlayerController::ShowCallout (projected each tick), fades out at
// the end of its life.
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CalloutWidget.generated.h"

class UTextBlock;

UCLASS()
class REPLICAN_API UCalloutWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetCalloutText(const FString& Text, bool bSpeech);

protected:
	virtual void NativeOnInitialized() override;

private:
	UPROPERTY() TObjectPtr<UTextBlock> Text;
};
