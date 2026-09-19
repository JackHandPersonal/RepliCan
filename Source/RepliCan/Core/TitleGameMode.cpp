#include "TitleGameMode.h"
#include "TitlePlayerController.h"
#include "GameFramework/SpectatorPawn.h"

ATitleGameMode::ATitleGameMode()
{
	PlayerControllerClass = ATitlePlayerController::StaticClass();
	DefaultPawnClass = ASpectatorPawn::StaticClass();
	HUDClass = nullptr;
}
