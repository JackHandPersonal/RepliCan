// A Synty sci-fi door that works. Two kinds:
//  * Wide  - Doorframe_05 with its split leaves (Door_L_05 / Door_R_05), which
//            slide apart into the flanking walls.
//  * Hatch - Doorframe_01 with the single hinged Doorframe_Door_01 leaf (183 cm
//            opening, red wheel), which swings on its hinge to the side given
//            by SwingSign (crew-cabin doors swing into the cabin).
// Either opens when a pawn steps into the approach volume and closes again
// when it leaves. The frame pivot is the actor origin (left end of the 500 cm
// segment on the grid line, yaw along the wall), so it drops in exactly where
// a wall would. Placed as a level actor by Tools/facility_layout.py.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SlidingDoorActor.generated.h"

class UBoxComponent;
class UStaticMeshComponent;
class UStaticMesh;

UENUM(BlueprintType)
enum class EDoorKind : uint8
{
	Wide,    // Doorframe_05 + split leaves: opens itself for anyone near, with a pneumatic whoosh
	Hatch,   // Doorframe_01 + the hinged wheel leaf: opened from the inspect menu
	Lift,    // Lift_Wall_01 + its two sliding leaves (180 x 240 opening): the cabin door, opened from the inspect menu
	Cabin,   // Doorframe_06 + its one sliding leaf (100 x 198 opening): a crew-cabin door at a person's scale, opened from the inspect menu
};

UCLASS()
class REPLICAN_API ASlidingDoorActor : public AActor
{
	GENERATED_BODY()

public:
	ASlidingDoorActor();
	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door") EDoorKind Kind = EDoorKind::Wide;
	// Hatch only: +1 swings the leaf towards the frame's local +Y, -1 towards -Y.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door") float SwingSign = 1.0f;
	// Wide: how far each leaf travels (cm); the leaves are 187 wide, so 187 clears the opening.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door") float SlideDistance = 187.0f;
	// Hatch: degrees of swing.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door") float SwingDegrees = 105.0f;
	// Seconds for a full open or close.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door") float SlideSeconds = 0.9f;
	// A locked door ignores the approach volume (and closes if open).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door") bool bLocked = false;
	// Hold open regardless of who is near.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door") bool bHoldOpen = false;

	UFUNCTION(BlueprintCallable, Category = "Door") void SetLocked(bool bInLocked) { bLocked = bInLocked; }
	UFUNCTION(BlueprintCallable, Category = "Door") void SetHoldOpen(bool bInOpen) { bHoldOpen = bInOpen; }
	UFUNCTION(BlueprintPure, Category = "Door") bool IsLocked() const { return bLocked; }
	UFUNCTION(BlueprintPure, Category = "Door") bool IsHeldOpen() const { return bHoldOpen; }
	// Manual doors (Hatch, Lift) ignore the approach volume: only the inspect menu opens and closes them.
	UFUNCTION(BlueprintPure, Category = "Door") bool IsManual() const { return Kind != EDoorKind::Wide; }
	UFUNCTION(BlueprintPure, Category = "Door") FString GetDoorName() const { return Kind == EDoorKind::Wide ? TEXT("Door") : TEXT("Cabin door"); }
	UFUNCTION(BlueprintCallable, Category = "Door") void Configure(EDoorKind InKind, float InSwingSign, bool bInLocked);
	UFUNCTION(BlueprintPure, Category = "Door") float GetOpenAmount() const { return OpenAmount; }

	UPROPERTY(VisibleAnywhere, Category = "Door") TObjectPtr<USceneComponent> Root;
	UPROPERTY(VisibleAnywhere, Category = "Door") TObjectPtr<UStaticMeshComponent> Frame;
	UPROPERTY(VisibleAnywhere, Category = "Door") TObjectPtr<UStaticMeshComponent> LeafL;
	UPROPERTY(VisibleAnywhere, Category = "Door") TObjectPtr<UStaticMeshComponent> LeafR;
	// The leaves are the door; the frame is wall. The inspect reticle and outline use only the leaves.
	bool IsLeaf(const UPrimitiveComponent* C) const { return C && (C == LeafL || C == LeafR); }
	UPROPERTY(VisibleAnywhere, Category = "Door") TObjectPtr<UBoxComponent> Approach;

protected:
	virtual void BeginPlay() override;
	virtual void OnConstruction(const FTransform& Transform) override;   // editor preview of Hold Open
	// Pawns are polled against the approach box each tick rather than
	// through overlap events: the player's capsule has its collision turned
	// off during cinematics (bed scenes), and the door must still answer.
	bool AnyPawnNear() const;
	void ApplyKind();
	void PlaceLeaves();

private:
	UPROPERTY() TObjectPtr<UStaticMesh> Frame05Mesh;
	UPROPERTY() TObjectPtr<UStaticMesh> LeafL05Mesh;
	UPROPERTY() TObjectPtr<UStaticMesh> LeafR05Mesh;
	UPROPERTY() TObjectPtr<UStaticMesh> Frame01Mesh;
	UPROPERTY() TObjectPtr<UStaticMesh> Door01Mesh;
	UPROPERTY() TObjectPtr<UStaticMesh> LiftWallMesh;
	UPROPERTY() TObjectPtr<UStaticMesh> LiftDoorLMesh;
	UPROPERTY() TObjectPtr<UStaticMesh> LiftDoorRMesh;
	UPROPERTY() TObjectPtr<UStaticMesh> Frame06Mesh;   // Cabin: a 500 wall segment, 45 thick, with a 100 x 198 doorway at x 200..300
	UPROPERTY() TObjectPtr<UStaticMesh> Door06Mesh;    // Cabin: its leaf, authored in the frame's space at x 199..301, 10 thick
	static constexpr float CabinSlide = 104.0f;   // the leaf is 102 wide: this clears the opening into the wall on its left
	static constexpr float LiftSlide = 96.0f;   // each lift leaf is 98 wide; the opening is x 160..340
	bool bWasOpening = false;   // for the travel sound
	// Closed rest positions, measured from the frame meshes.
	static constexpr float LeafLClosedX = 156.4f;   // Wide: the leaves meet at x 250
	static constexpr float LeafRClosedX = 342.1f;
	// Hatch: the frame's visible portal (vertex-measured) runs x 148..352 at the mid-plane (the
	// outer lips sit at 169) and up to z 291; the 169 x 256 leaf is hung at 148 and stretched to fill it.
	static constexpr float HatchHingeX = 148.0f;
	static constexpr float HatchLeafScaleX = 204.0f / 169.0f;
	static constexpr float HatchLeafScaleZ = 291.0f / 256.0f;
	static constexpr float HatchLeafZ = 128.0f * HatchLeafScaleZ;   // the leaf mesh is centred on its pivot vertically
	float OpenAmount = 0.0f;   // 0 closed .. 1 open
};
