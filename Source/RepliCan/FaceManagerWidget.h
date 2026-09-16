// The Face Manager page: the character's face systems (nose mesh and
// placement, decal mouth, tint, expression, auto-blink) on their own panel,
// opened from the Character Manager's "Face..." button. It is the same
// panel machinery as UCharacterBuilderWidget building only the face section
// (see bFaceManager there), so every control keeps its handler, reset and
// JSON round-trip; while it is up the controller parks the view on a
// portrait camera in front of the character (ABasePlayerController::
// UpdateFaceCamera).
#pragma once

#include "CoreMinimal.h"
#include "CharacterBuilderWidget.h"
#include "FaceManagerWidget.generated.h"

UCLASS()
class REPLICAN_API UFaceManagerWidget : public UCharacterBuilderWidget
{
	GENERATED_BODY()

public:
	UFaceManagerWidget(const FObjectInitializer& ObjectInitializer);
};
