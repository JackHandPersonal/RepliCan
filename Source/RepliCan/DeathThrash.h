// HOW A BODY DIES. Five ways, drawn at random for a person (or forced by RepliCan.DeathStyle):
//   Ragdoll     every bone lets go at once, then the thrash, then stillness.
//   LegFirst    one leg gives way first while the rest still stands on its pose for a beat; then all.
//   UpperFirst  the trunk and arms go slack first, the legs hold for a beat; then all.
//   Clip        a Lyra death clip for the side the shot came from, cut into the ragdoll part-way.
//   Robot       the trunk stays rigid while the limbs jerk in sharp random spasms for a few
//               seconds, then everything drops and stays down (no thrash: a machine goes still).
// The thrash after a drop: random pushes on the limbs, fewer and weaker as the seconds pass,
// then the damped ragdoll settles. All physics, so each stage follows on from the last.
#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DeathThrash.generated.h"

UENUM()
enum class EDeathStyle : uint8 { Ragdoll, LegFirst, UpperFirst, Clip, Robot };

UCLASS()
class REPLICAN_API UDeathPlayComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDeathPlayComponent();
	// Puts one of these on the actor and starts the style. SettleRagdoll (mass, damping, followers) runs here too.
	static void Begin(AActor* Who, class USkeletalMeshComponent* Body, EDeathStyle Style, const FVector& ShotDir, int32 Brawn, FName HitBone);
	// The cvar when it is set, Robot for an actor tagged "robot", else a random organic style.
	static EDeathStyle Pick(const AActor* Who);
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	void FullRagdoll();
	void Kick(bool bRobot, float Fade);
	UPROPERTY() TObjectPtr<class USkeletalMeshComponent> Body;
	EDeathStyle Style = EDeathStyle::Ragdoll;
	FVector ShotDir = FVector::ForwardVector;
	FName HitBone;
	int32 Phase = 0;                 // 0: the first movement; 1: the whole body let go
	float Clock = 0.0f, PhaseAt = 0.0f, NextKick = 0.0f, ThrashFrom = 0.0f, ThrashUntil = 0.0f, Strength = 1.0f;
	bool bThrash = true;
	bool bPooled = false;
};
