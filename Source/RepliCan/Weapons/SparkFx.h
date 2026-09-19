// ELECTRICAL SPARKS. A burst of small hot motes, blue-white and yellow, thrown from a point and
// falling under gravity, gone inside half a second, with a flash of blue light on the way. What a
// damaged machine gives off (ShotReactions on anything tagged "robot"; the work bot arcing on its
// own under half its vitality), and what a tool arcing on metal does. Small emissive spheres moved
// by hand rather than a particle asset: nothing to author, and the colours are exactly the two
// that were asked for.
//
// EVERY MOTE USED TO BE ITS OWN SCENE COMPONENT, created and registered at runtime and moved with
// SetWorldLocation each tick. That put a hard ceiling on how big a burst could be -- a few dozen --
// and the ceiling showed: a robot coming apart threw fewer sparks than a scratched wire. Now a
// burst is TWO instanced-mesh components, one per colour, and a mote is an instance in one of them.
// The whole burst moves in two batched writes a frame instead of hundreds of component updates, so
// hundreds of motes cost about what a dozen used to and the call sites can ask for what the moment
// deserves.
#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SparkFx.generated.h"

UCLASS()
class REPLICAN_API ASparkBurstActor : public AActor
{
	GENERATED_BODY()
public:
	ASparkBurstActor();
	virtual void Tick(float DeltaSeconds) override;
	void Init(const FVector& Where, const FVector& Normal, int32 Count, float Scale, const FLinearColor& ColourA, const FLinearColor& ColourB);
private:
	// bWarm picks the colour, and so which of the two instanced components holds this mote; Slot
	// is its index within that one.
	struct FMote { FVector Pos = FVector::ZeroVector, Vel = FVector::ZeroVector; float Life = 0.4f, Age = 0.0f, Size = 1.0f; bool bWarm = false; int32 Slot = 0; };
	TArray<FMote> Motes;
	UPROPERTY() TObjectPtr<class UInstancedStaticMeshComponent> Cool;
	UPROPERTY() TObjectPtr<class UInstancedStaticMeshComponent> Warm;
	UPROPERTY() TObjectPtr<class UPointLightComponent> Flash;
	float Clock = 0.0f;
	float FlashSeconds = 0.14f, FlashPeak = 0.0f;
};

namespace SparkFx
{
	// Count motes from Where, thrown mostly along Normal; Scale sizes the motes and the flash. Half
	// the motes wear ColourA, half ColourB: an arc's blue-white and hot yellow unless told otherwise.
	// Count is cheap now -- see the note on the class -- so ask for what the moment is worth.
	REPLICAN_API void Burst(UWorld* World, const FVector& Where, const FVector& Normal, int32 Count = 14, float Scale = 1.0f,
		// BRIGHTER THAN WHITE. These were inside 0..1, which is the range of a PAINTED surface -- a
		// spark is a burning speck, and at a colour no stronger than a wall it reads as flecks of
		// dark fluid coming off the machine rather than fire. Values above one are what put it
		// through the bloom and give it the glare a spark has.
		const FLinearColor& ColourA = FLinearColor(1.6f, 3.4f, 7.5f, 1.0f),
		const FLinearColor& ColourB = FLinearColor(9.0f, 4.6f, 1.1f, 1.0f));
}
