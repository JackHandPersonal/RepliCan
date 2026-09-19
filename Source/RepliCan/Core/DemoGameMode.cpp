#include "Core/DemoGameMode.h"
#include "Core/DemoPlayerController.h"

ADemoGameMode::ADemoGameMode()
{
	PlayerControllerClass = ADemoPlayerController::StaticClass();
}
