#include "Weapons/TimedFx.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "NiagaraFunctionLibrary.h"
#include "Engine/World.h"
#include "TimerManager.h"

UNiagaraComponent* TimedFx::Spawn(UWorld* World, UNiagaraSystem* System, const FVector& At,
	const FRotator& Rot, const FVector& Scale, float FadeAfterSeconds)
{
	if (!World || !System) { return nullptr; }
	UNiagaraComponent* C = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
		World, System, At, Rot, Scale, /*bAutoDestroy=*/true, /*bAutoActivate=*/true);
	if (!C) { return nullptr; }
	C->SetCastShadow(false);
	if (FadeAfterSeconds > 0.0f)
	{
		// The weak lambda is what makes this safe: if the component has already gone -- the level
		// changed, the pool reclaimed it -- the timer simply does not run.
		FTimerHandle H;
		World->GetTimerManager().SetTimer(H,
			FTimerDelegate::CreateWeakLambda(C, [C]() { C->Deactivate(); }), FadeAfterSeconds, false);
	}
	return C;
}
