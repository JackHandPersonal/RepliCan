// GameMode for the main gameplay experience -- kept separate from
// ADemoGameMode (which is wired to the facial-expression demo's
// ADemoPlayerController) so this level's input/cursor behavior doesn't
// touch that unrelated, already-working demo.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "BaseGameMode.generated.h"

UCLASS()
class REPLICAN_API ABaseGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:

	ABaseGameMode();

	// Brings the environment director into the world: the level itself carries no fog, no post
	// process volume and no atmosphere, on purpose, so that adding any of it never means
	// rewriting the facility map.
	virtual void BeginPlay() override;
};
