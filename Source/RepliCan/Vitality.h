// VITALITY: the points a body has before it is done. Endurance plus a quarter of Brawn, so a
// 50/50 body has 62.5. Damage comes off it after armour and the head factor; each region keeps
// count of what it has taken, and a limb past a quarter of the pool is injured, past half of it
// destroyed (a head past half comes off). At zero the body is dead. Lives on anything that can
// be shot: a character from its attributes, an extra on the default the first time a shot lands.
// What injury and death DO to a body is the character's business (ABaseCharacter::NoteInjury,
// Die) and ShotReactions' (the ragdoll, the severed piece); this only keeps the score.
#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Vitality.generated.h"

UCLASS()
class REPLICAN_API UVitalityComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	static float PoolFor(int32 Endurance, int32 Brawn) { return Endurance + 0.25f * Brawn; }
	// The component on an actor, made on the spot if it has none: a character's pool from its
	// attributes, anything else on the 50/50 default.
	static UVitalityComponent* FindOrAdd(AActor* Who);
	static bool IsLimb(const FString& Region) { return Region == TEXT("ArmL") || Region == TEXT("ArmR") || Region == TEXT("LegL") || Region == TEXT("LegR"); }

	struct FOutcome { float Dealt = 0.0f; bool bInjured = false; bool bDestroyed = false; bool bDied = false; };
	// A hit lands: armour comes off the raw amount first, then a head hit counts half again; the
	// region keeps the total and the thresholds are checked against the whole pool.
	FOutcome Apply(const FString& Region, float Raw, float Armour);
	void Reset(float Max);
	FString Summary() const;   // "41/62; ArmL injured"

	UPROPERTY(VisibleAnywhere, Category = "Vitality") float MaxVitality = 62.5f;
	UPROPERTY(VisibleAnywhere, Category = "Vitality") float Vitality = 62.5f;
	UPROPERTY(VisibleAnywhere, Category = "Vitality") TMap<FString, float> RegionDamage;
	UPROPERTY(VisibleAnywhere, Category = "Vitality") TSet<FString> Injured;
	UPROPERTY(VisibleAnywhere, Category = "Vitality") TSet<FString> Destroyed;
	UPROPERTY(VisibleAnywhere, Category = "Vitality") bool bDead = false;
	// Taken since death. Past PulpFactor pools of it there is no body left to speak of: a pile.
	UPROPERTY(VisibleAnywhere, Category = "Vitality") float CorpseDamage = 0.0f;
	UPROPERTY(EditAnywhere, Category = "Vitality") float PulpFactor = 2.5f;
	bool IsPulped() const { return bDead && CorpseDamage >= MaxVitality * PulpFactor; }
	UPROPERTY(EditAnywhere, Category = "Vitality") float HeadFactor = 1.5f;
	UPROPERTY(EditAnywhere, Category = "Vitality") float InjuredFraction = 0.25f;
	UPROPERTY(EditAnywhere, Category = "Vitality") float DestroyedFraction = 0.5f;
	// A CORPSE'S LIMBS come off by force, not by wear: one blow at least as hard as the limb is
	// tough takes it off; weaker blows wear it down to the destroyed threshold but the one that
	// finishes it still has to carry half the toughness, so a needle never takes a leg. Fractions
	// of the pool: the neck is the weakest, a leg the strongest.
	UPROPERTY(EditAnywhere, Category = "Vitality") float ToughHead = 0.20f;
	UPROPERTY(EditAnywhere, Category = "Vitality") float ToughArm = 0.25f;
	UPROPERTY(EditAnywhere, Category = "Vitality") float ToughLeg = 0.35f;
	UPROPERTY(EditAnywhere, Category = "Vitality") float FinishFraction = 0.5f;
	float Toughness(const FString& Region) const;   // in points, 0 for the trunk
};
