#include "DemoGameMode.h"
#include "DemoPlayerController.h"

ADemoGameMode::ADemoGameMode()
{
	PlayerControllerClass = ADemoPlayerController::StaticClass();
}
