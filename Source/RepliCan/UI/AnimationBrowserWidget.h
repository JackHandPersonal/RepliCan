// The Animation Browser page: pick a category (Idles pack, sword combat,
// each locomotion set, dash/roll, falling, poses) and a clip, then Play
// once / Loop / Stop it on the selected character -- a quick way to see
// what an imported animation looks like on a rig. Same panel machinery as
// UCharacterBuilderWidget building only that section (see bAnimBrowser
// there); opened from the Character Manager's "Animations..." button.
#pragma once

#include "CoreMinimal.h"
#include "UI/CharacterBuilderWidget.h"
#include "AnimationBrowserWidget.generated.h"

UCLASS()
class REPLICAN_API UAnimationBrowserWidget : public UCharacterBuilderWidget
{
	GENERATED_BODY()

public:
	UAnimationBrowserWidget(const FObjectInitializer& ObjectInitializer);
};
