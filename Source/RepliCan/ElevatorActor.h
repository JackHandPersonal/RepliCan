// A rideable lift: a car that moves between floors, with doors, built from the Synty kit's own
// five-piece assembly.
//
// WHAT THE DEMO MAP TAUGHT US (Tools/measure_lift_assembly.py). The pack ships seven lift
// meshes and no instructions, but its demo level has one built, and every piece in it sits at
// the SAME XY WITH ZERO YAW. The whole assembly is authored in place and stacked by Z alone:
//
//     per level   SM_Bld_Lift_Wall_01          the shaft wall, with the opening
//                 SM_Bld_Lift_Wall_Door_01/02  shaft doors, left and right leaves
//                 SM_Bld_Lift_Door_01/02       car doors, same two leaves on the car
//     once        SM_Bld_Lift_01               the car
//
// and the demo stacks two levels 500 cm apart, which is the storey height the pack builds to --
// and happens to be exactly this project's CELL grid, so a shaft lines up with the facility
// without a single fudge factor.
//
// This actor owns everything that MOVES: the car, its doors, and a pair of shaft doors at every
// level. The shaft walls are static structure and are placed by Tools/facility_layout.py like
// any other wall, because they never do anything.
//
// Riding works through Unreal's own based movement: the car's floor is a movable primitive, so
// a character standing on it is carried. Nothing here pushes the player around, which is what
// makes it survive jumping, crouching and walking about inside a moving car.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ElevatorActor.generated.h"

class UStaticMeshComponent;
class UBoxComponent;
class UPointLightComponent;
class ASignActor;

UCLASS()
class REPLICAN_API AElevatorActor : public AActor
{
	GENERATED_BODY()

public:
	AElevatorActor();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	// The shaft doors are built here as well as at BeginPlay, so the lift is complete IN THE
	// EDITOR: at every serviced floor the viewport shows a closed pair of leaves in the shaft
	// wall rather than an open hole with nothing behind it. Components made here are
	// construction-script components and are rebuilt whenever the actor is.
	virtual void OnConstruction(const FTransform& Transform) override;

	// ---- The shaft ----------------------------------------------------------
	// Floors above and below the one this actor is placed at. The actor's own Z is floor zero,
	// so placing it on the bay deck makes the bay the ground floor by definition.
	UPROPERTY(EditAnywhere, Category = "Elevator") int32 FloorsUp = 4;
	UPROPERTY(EditAnywhere, Category = "Elevator") int32 FloorsDown = 10;
	// 500 cm, measured off the demo. Not a guess, and not adjustable without re-measuring:
	// the shaft wall mesh is 499.5 cm tall and will gap or overlap at any other spacing.
	UPROPERTY(EditAnywhere, Category = "Elevator") float FloorHeight = 500.0f;

	UPROPERTY(EditAnywhere, Category = "Elevator") float SpeedCmPerSecond = 260.0f;
	// Eased at both ends. A lift that starts and stops instantly reads as a moving platform;
	// the acceleration is most of what makes it read as a lift.
	UPROPERTY(EditAnywhere, Category = "Elevator") float EaseCm = 140.0f;
	UPROPERTY(EditAnywhere, Category = "Elevator") float DoorSeconds = 1.1f;
	UPROPERTY(EditAnywhere, Category = "Elevator") float DoorSlideCm = 96.0f;
	// How long the doors stand open after arriving before they will close again.
	UPROPERTY(EditAnywhere, Category = "Elevator") float DwellSeconds = 2.5f;

	// ---- The sign over the door ---------------------------------------------
	// The dot-matrix strip in the foyer's lintel is the lift's own status board. The layout
	// script hands the sign actor over; the car rewrites it whenever its state changes -- at a
	// floor, travelling, which way -- and never otherwise, because SetText rebuilds the strip's
	// render target and that is not a per-frame cost.
	UPROPERTY(EditAnywhere, Category = "Elevator") FString LiftName = TEXT("PERSONNEL LIFT B4");
	UPROPERTY(EditAnywhere, Category = "Elevator") TObjectPtr<ASignActor> StatusSign;
	UFUNCTION(BlueprintPure, Category = "Elevator") FString StatusLine() const;

	// ---- Opening on approach ------------------------------------------------
	// The lift's doors work like every other door in the station: they open when somebody is
	// there and close when nobody is. The first version held them open permanently at rest,
	// which read as a broken door rather than an automatic one.
	//
	// ONE test covers both sides. A stopped car and the shaft doors at that floor are in exactly
	// the same place -- that is the whole Synty lift assembly, every piece at the same XY with
	// zero yaw -- so a box straddling the threshold catches the rider inside the car and the
	// person waiting on the landing with the same check. It travels with the car, so standing at
	// Sub 03's doors does nothing while the car is at the bay.
	UPROPERTY(EditAnywhere, Category = "Elevator") bool bAutoDoors = true;
	// The threshold, in the car's own frame. MEASURED off the kit rather than inferred:
	//
	//     SM_Bld_Lift_01         x  77.7..422.3   y -340.1..3.5   z -66.3..300.1   the car
	//     SM_Bld_Lift_Door_01    x 159.5..257.6   y   -3.9..3.5   z   3.9..239.3   left leaf
	//     SM_Bld_Lift_Door_02    x 246.5..344.2   y   -3.9..3.5   z   3.9..239.3   right leaf
	//
	// so the doorway is a 184 cm opening centred on x 252, its plane is y = 0, and the car is
	// entirely on the -Y side of it. The landing is therefore +Y, and a box straddling y = 0
	// covers both. A rider's actor location is their capsule centre, about 88 above the floor.
	UPROPERTY(EditAnywhere, Category = "Elevator") FVector DoorwayLocal = FVector(252.0f, 0.0f, 120.0f);
	// How far out onto the landing, and how far back into the car, someone counts as "there".
	UPROPERTY(EditAnywhere, Category = "Elevator") float ApproachDepthCm = 260.0f;
	UPROPERTY(EditAnywhere, Category = "Elevator") float ApproachHalfWidthCm = 200.0f;
	UPROPERTY(EditAnywhere, Category = "Elevator") float ApproachHalfHeightCm = 220.0f;

	// Which floors have anything behind the doors. A shaft is dug before the decks are fitted
	// out, and a lift that opens onto a hole is a lift that kills the player for pressing a
	// button. Floors not listed here are offered as SEALED, refuse to be selected, and keep
	// their doors shut even if the car passes them.
	//
	// Empty means every floor is serviced, which is the right default for a lift used somewhere
	// that is actually finished.
	UPROPERTY(EditAnywhere, Category = "Elevator") TArray<int32> ServicedFloors = { 0, -10 };
	UFUNCTION(BlueprintPure, Category = "Elevator") bool IsFloorServiced(int32 Floor) const;

	// ---- Asking it to go somewhere -----------------------------------------
	// Floor indices run from -FloorsDown to +FloorsUp, zero being wherever this was placed.
	UFUNCTION(BlueprintCallable, Category = "Elevator") void GoToFloor(int32 Floor);
	UFUNCTION(BlueprintPure, Category = "Elevator") int32 GetCurrentFloor() const { return CurrentFloor; }
	UFUNCTION(BlueprintPure, Category = "Elevator") bool IsMoving() const { return bMoving; }
	// "Bay 01", "Deck 03", "Sub 07". Built from the index so adding floors needs no table.
	UFUNCTION(BlueprintPure, Category = "Elevator") FString FloorName(int32 Floor) const;
	// Every floor, top to bottom, as the inspect menu wants them.
	UFUNCTION(BlueprintPure, Category = "Elevator") TArray<FString> FloorMenu() const;
	// Turns a menu entry back into a floor index. -1000 when it is not one of ours.
	UFUNCTION(BlueprintPure, Category = "Elevator") int32 FloorFromName(const FString& Name) const;
	// True when the player is standing in the car, which is what decides whether the panel
	// offers floors or only a call button.
	UFUNCTION(BlueprintPure, Category = "Elevator") bool IsSomeoneAboard() const;

	UPROPERTY(VisibleAnywhere, Category = "Elevator") TObjectPtr<UStaticMeshComponent> Car;
	UPROPERTY(VisibleAnywhere, Category = "Elevator") TObjectPtr<UStaticMeshComponent> CarDoorL;
	UPROPERTY(VisibleAnywhere, Category = "Elevator") TObjectPtr<UStaticMeshComponent> CarDoorR;
	// The volume that decides whether anyone is aboard.
	UPROPERTY(VisibleAnywhere, Category = "Elevator") TObjectPtr<UBoxComponent> CarVolume;
	// The control panel, on the inside wall beside the door. THIS is what you inspect from inside
	// the car -- not the car, not the doors -- so the floor list appears when you look at the
	// buttons, which is where a person looks for it. From outside, the doors are the call point.
	UPROPERTY(VisibleAnywhere, Category = "Elevator") TObjectPtr<UStaticMeshComponent> Panel;
	UFUNCTION(BlueprintPure, Category = "Elevator") bool IsInspectPoint(const UPrimitiveComponent* Comp) const;
	// The car's own lamp. A shaft is a hole in the rock with nothing in it, so a ten-storey
	// descent was ten storeys of black; the light that matters is the one riding WITH you.
	UPROPERTY(VisibleAnywhere, Category = "Elevator") TObjectPtr<UPointLightComponent> CarLight;

private:
	float FloorZ(int32 Floor) const { return Floor * FloorHeight; }
	bool AnyPawnNear() const;
	void SpawnShaftDoors();
	void SetDoorOpen(float Alpha);

	// One pair of leaves per level, spawned at BeginPlay. They are components rather than level
	// actors so they cannot be accidentally deleted or moved out from under the lift.
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> ShaftDoorL;
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> ShaftDoorR;

	int32 CurrentFloor = 0;
	int32 TargetFloor = 0;
	bool bMoving = false;
	// 0 shut, 1 fully open. Doors are shut for the whole journey and never otherwise.
	float DoorAlpha = 1.0f;
	float DwellLeft = 0.0f;
	float StartZ = 0.0f;
	// What the doors were last asked to do, so the sound plays once at the start of the travel
	// rather than every frame of it.
	bool bWasOpening = true;
	FString LastSignText;
	void UpdateSign();
};
