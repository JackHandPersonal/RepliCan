// THE AIR OF THE WHOLE STATION, rather than clouds parked in particular rooms.
//
// The first attempt at this placed a handful of Niagara dust systems per room, which had two faults
// the moment you walked around: each one was a DENSE patch in one spot, and everywhere between them
// the air was dead. Motes are not a thing that happens in seven places -- they are a property of the
// building, and the only place a property of the building can be evaluated cheaply is around the
// person looking at it.
//
// So this rides the player. Every second or two it puts ONE small, short-lived puff of specks at a
// random point in the air ahead of them: far enough out not to be a smear on the lens, near enough
// to be lit by whatever they are lit by, and only where a trace says there is actually open air, so
// nothing blooms inside a wall. The puffs expire on their own. What you see is a few specks turning
// over wherever you happen to be standing, which is the whole level, all the time, and the cost is
// one small system alive at a time rather than a permanent cloud in every room.
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AmbientMotes.generated.h"

class UNiagaraSystem;

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class REPLICAN_API UAmbientMotesComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UAmbientMotesComponent();
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaSeconds, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// Seconds between puffs, jittered between the two. Slow on purpose: the point is that the air is
	// never quite still, not that there is weather indoors.
	UPROPERTY(EditAnywhere, Category = "Ambient Motes") FVector2D Interval = FVector2D(1.4f, 3.4f);
	// How far ahead of the eye a puff may appear. Inside Near it reads as dirt on the camera.
	UPROPERTY(EditAnywhere, Category = "Ambient Motes") float NearCm = 220.0f;
	UPROPERTY(EditAnywhere, Category = "Ambient Motes") float FarCm = 750.0f;
	// Sideways and vertical spread about the view direction.
	UPROPERTY(EditAnywhere, Category = "Ambient Motes") float SpreadCm = 260.0f;
	UPROPERTY(EditAnywhere, Category = "Ambient Motes") float RiseCm = 140.0f;
	UPROPERTY(EditAnywhere, Category = "Ambient Motes") float Scale = 0.42f;
	// EVERY PUFF ENDS, AND ONLY SO MANY HANG AT ONCE. The dust asset loops, so bAutoDestroy never
	// fired and these accumulated for the whole session -- 179 were counted alive in one run, which
	// is why they seemed to gather and never fade. With a fade of this length and the interval
	// below, the steady state is about three or four in the air: a level-wide hint, not weather.
	UPROPERTY(EditAnywhere, Category = "Ambient Motes") float FadeAfterSeconds = 7.0f;
	UPROPERTY(EditAnywhere, Category = "Ambient Motes") int32 MaxAlive = 10;
	// A puff needs this much clear air around it, or the spot is rejected and the turn is skipped.
	UPROPERTY(EditAnywhere, Category = "Ambient Motes") float ClearanceCm = 45.0f;
	// Below this Z the station is the service deck, where the air is meant to be thicker.
	UPROPERTY(EditAnywhere, Category = "Ambient Motes") float DeckBelowZ = -4000.0f;
	UPROPERTY(EditAnywhere, Category = "Ambient Motes") float DeckIntervalScale = 0.55f;

	UPROPERTY(EditAnywhere, Category = "Ambient Motes") TObjectPtr<UNiagaraSystem> System;
	UPROPERTY(EditAnywhere, Category = "Ambient Motes") FString SystemPath = TEXT("/Game/Synty/PolygonSciFiHorror/FX/NS_Dust_Spots_Small_01.NS_Dust_Spots_Small_01");

private:
	bool EyeOf(FVector& OutLoc, FRotator& OutRot) const;
	float Clock = 0.0f;
	TArray<TWeakObjectPtr<class UNiagaraComponent>> Live;   // what is still in the air, so the cap can be honest
};
