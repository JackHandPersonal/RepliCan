// NPC ALERTNESS: how aware a character is of the player, the foundation of sneaking and of
// being hunted. An observer looks along its facing with eyes at its head: a target inside its
// cone, in range and in its line of sight raises awareness at a rate that falls with distance,
// halves for a crouched target and drops again for one standing still; a target it cannot see
// lets it decay. Noise (a shot, a beam lighting) reaches every observer inside its radius and
// raises awareness by how close it was. Damage is the loudest thing of all: whoever dealt it is
// known at once and carries threat.
//   Calm       -> Suspicious  awareness past SuspiciousAt: the observer has somewhere to look
//   Suspicious -> Alert       past AlertAt, or any damage: the target is known and carries threat
//   Alert      -> Searching   the target out of sight for a few seconds; the last known point kept
//   Searching  -> Calm        nothing for SearchSeconds
// The component keeps the score and says what it knows. What the body DOES about it is its
// controller's business (AWorkBotController patrols, closes and strikes).
#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Alertness.generated.h"

UENUM()
enum class EAlertState : uint8 { Calm, Suspicious, Alert, Searching };

UCLASS()
class REPLICAN_API UAlertnessComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UAlertnessComponent();
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// A sound at Where, heard Radius cm out, made by Source: every observer in range hears it.
	static void ReportNoise(UWorld* World, const FVector& Where, float Radius, AActor* Source);
	static const TCHAR* StateName(EAlertState S);
	// Damage taken from Who: known at once, and threat added.
	void NoteDamage(AActor* Who, float Amount);

	EAlertState GetState() const { return State; }
	float GetAwareness() const { return Awareness; }
	AActor* GetTarget() const { return Target.Get(); }
	const FVector& GetLastKnown() const { return LastKnown; }
	bool HasLastKnown() const { return bHasLastKnown; }
	float SecondsInState() const;
	bool CanSee(const AActor* Who) const;   // this instant: in the cone, in range, unobstructed
	void ForgetTarget();

	UPROPERTY(EditAnywhere, Category = "Alertness") float SightRange = 1400.0f;
	UPROPERTY(EditAnywhere, Category = "Alertness") float SightFovDeg = 120.0f;
	UPROPERTY(EditAnywhere, Category = "Alertness") float GainPerSecond = 0.9f;    // at point-blank, standing, moving
	UPROPERTY(EditAnywhere, Category = "Alertness") float DecayPerSecond = 0.22f;
	UPROPERTY(EditAnywhere, Category = "Alertness") float SuspiciousAt = 0.3f;
	UPROPERTY(EditAnywhere, Category = "Alertness") float AlertAt = 1.0f;
	UPROPERTY(EditAnywhere, Category = "Alertness") float SearchSeconds = 12.0f;  // out of sight this long: back to calm
	UPROPERTY(EditAnywhere, Category = "Alertness") float CrouchFactor = 0.45f;
	UPROPERTY(EditAnywhere, Category = "Alertness") float StillFactor = 0.6f;

	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnAlertStateChanged, EAlertState /*Old*/, EAlertState /*New*/);
	FOnAlertStateChanged OnStateChanged;

private:
	void SetState(EAlertState New);
	FVector Eyes() const;
	void Hear(const FVector& Where, float Radius, AActor* Source);
	EAlertState State = EAlertState::Calm;
	float Awareness = 0.0f;
	float StateSince = 0.0f;
	float LastSeenAt = -1000.0f;
	bool bHasLastKnown = false;
	FVector LastKnown = FVector::ZeroVector;
	TWeakObjectPtr<AActor> Target;
	TMap<TWeakObjectPtr<AActor>, float> Threat;
};
