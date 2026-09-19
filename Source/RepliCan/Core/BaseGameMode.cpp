#include "Core/BaseGameMode.h"
#include "Core/BasePlayerController.h"
#include "Characters/BaseCharacter.h"
#include "UI/BaseHUD.h"
#include "World/EnvironmentDirector.h"

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
