// The small translucent name tag that floats above every character's head
// while edit mode is up (drawn by ABaseCharacter's screen-space
// UWidgetComponent). Built in code like the other panels.
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CharacterNameWidget.generated.h"

class UTextBlock;

UCLASS()
class REPLICAN_API UCharacterNameWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetName(const FString& Name);
	// Point size of the label; the owning character drives it from the
	// camera distance (see ABaseCharacter::UpdateNameLabelScale).
	void SetFontSize(int32 Size);

protected:
	virtual void NativeOnInitialized() override;

private:
	UPROPERTY() TObjectPtr<UTextBlock> NameText;
	int32 CurrentFontSize = 14;
};
