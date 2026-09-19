#include "Core/RepliCanUserSettings.h"
#include "Engine/Engine.h"

URepliCanUserSettings* URepliCanUserSettings::Get()
{
	return GEngine ? Cast<URepliCanUserSettings>(GEngine->GetGameUserSettings()) : nullptr;
}
