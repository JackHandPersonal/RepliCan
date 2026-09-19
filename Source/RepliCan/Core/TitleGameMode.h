// Game mode of the title map (Lvl_Title): no playable pawn, just the
// ATitlePlayerController driving the title / intro / main menu screens.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "TitleGameMode.generated.h"

UCLASS()
class REPLICAN_API ATitleGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ATitleGameMode();
};
