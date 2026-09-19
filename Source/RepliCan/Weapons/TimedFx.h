#pragma once
#include "CoreMinimal.h"

class UNiagaraComponent;
class UNiagaraSystem;
class UWorld;

namespace TimedFx
{
	// A NIAGARA SYSTEM THAT IS GUARANTEED TO END.
	//
	// bAutoDestroy only fires when the system reports itself COMPLETE, and an ambient loop never
	// does -- so every "one-off" spawned from a looping asset stays alive, at full strength, for the
	// rest of the session. It is the same trap as USoundWaveProcedural on the audio side, and it
	// hides well: each puff is cheap and correct on its own, and only the total is wrong.
	//
	// Measured in the running game before this existed: 179 NS_Dust_Spots_Small_01 and 58
	// NS_Electricity_Surge_01 alive at once, none of them ever coming back. That is what "the dark
	// floaties gather more and more, and never fade" looks like from the inside.
	//
	// Deactivate rather than destroy is the point: emission stops, the particles already in flight
	// live out their own lives, and the component goes when the last one is gone. That IS the fade.
	// Destroying outright would make the puff blink out, which reads worse than never fading.
	UNiagaraComponent* Spawn(UWorld* World, UNiagaraSystem* System, const FVector& At,
		const FRotator& Rot, const FVector& Scale, float FadeAfterSeconds);
}
