#include "Core/TitleGameMode.h"
#include "Core/TitlePlayerController.h"
#include "GameFramework/SpectatorPawn.h"

ATitleGameMode::ATitleGameMode()
{
	PlayerControllerClass = ATitlePlayerController::StaticClass();
	DefaultPawnClass = ASpectatorPawn::StaticClass();
	HUDClass = nullptr;
}
