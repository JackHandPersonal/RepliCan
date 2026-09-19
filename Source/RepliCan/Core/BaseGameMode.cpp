#include "BaseGameMode.h"
#include "BasePlayerController.h"
#include "BaseCharacter.h"
#include "BaseHUD.h"
#include "EnvironmentDirector.h"

ABaseGameMode::ABaseGameMode()
{
	PlayerControllerClass = ABasePlayerController::StaticClass();
	DefaultPawnClass = ABaseCharacter::StaticClass();
	HUDClass = ABaseHUD::StaticClass();
}

void ABaseGameMode::BeginPlay()
{
	Super::BeginPlay();
	AEnvironmentDirector::Ensure(GetWorld());
}
