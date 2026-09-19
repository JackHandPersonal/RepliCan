// A Niagara actor you cannot click on.
//
// Fog cards and steam are the biggest things in any room, in the sense that matters to the
// editor: a ground-fog sprite is a barn-sized billboard, and its icon sits in front of whatever
// you were actually aiming at. Every attempt to select a crate near a vent selected the vent.
//
// The fix is one flag, bSelectable, on the primitive components -- it is what decides whether a
// component gets a hit proxy at all, so with it off a click passes straight through to the thing
// behind. It is not reflected to Python and not editable in Details, which is why this is a
// class rather than a line in Tools/facility_layout.py. Both the particle component and the
// editor icon are switched off, because the icon is usually the part that takes the click.
#pragma once

#include "CoreMinimal.h"
#include "NiagaraActor.h"
#include "AtmosphereFXActor.generated.h"

UCLASS()
class REPLICAN_API AAtmosphereFXActor : public ANiagaraActor
{
	GENERATED_BODY()

public:
	AAtmosphereFXActor(const FObjectInitializer& ObjectInitializer);
};
