// A terminal-style rule: a line of text that pads itself with a rule character to the full
// width it is given, e.g. "==[ INVENTORY ]=" becoming "==[ INVENTORY ]=============" out to the
// column's edge, or (vertical) a column of "|" that grows to the height it is given. Shared by
// every panel that draws ASCII rules (the character sheet, the container transfer screen) so
// they line up the same way. Refits each tick against the text's own rendered size, so it
// converges whatever the font measure estimates.
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CrtRuleWidget.generated.h"

class UTextBlock;

UCLASS()
class REPLICAN_API UCrtRuleWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// Base: the text before the padding; the padding character is its last character unless
	// InRuleChar is given. Vertical rules stack Base (a single character) down the height.
	void Configure(const FString& InBase, int32 InSize, const FLinearColor& InColor, TCHAR InRuleChar = 0, bool bInVertical = false);
	static UCrtRuleWidget* Make(APlayerController* Owner, const FString& Base, int32 Size, const FLinearColor& Color, TCHAR RuleChar = 0);
	static UCrtRuleWidget* MakeVertical(APlayerController* Owner, const FString& Glyph, int32 Size, const FLinearColor& Color);

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	void Fit();
	UPROPERTY() TObjectPtr<UTextBlock> Text;
	FString Base;
	int32 Size = 13;
	FLinearColor Color = FLinearColor::White;
	TCHAR RuleChar = TEXT('-');
	bool bVertical = false;
	int32 Count = -1;
};
