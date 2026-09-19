// THE DECOMMISSIONED WORK BOT: the first thing in the game that fights back. A CyberCity service
// robot on the mannequin rig (Characters/WorkBot.json: slow, sturdy, no face) that patrols a box
// of the service deck's floor, mutters status reports through a bad speaker, and comes for whoever
// it becomes aware of (UAlertnessComponent). Damage throws sparks (ShotReactions); under half its
// vitality it arcs on its own; it dies with a scream and the robot death style. Its controller is
// the whole of its mind: no behaviour tree, no navmesh -- a goal, a sweep ahead for what is in the
// way, and a way round it.
#pragma once
#include "CoreMinimal.h"
#include "Characters/BaseCharacter.h"
#include "AIController.h"
#include "Characters/Alertness.h"
#include "WorkBot.generated.h"

UCLASS()
class REPLICAN_API AWorkBot : public ABaseCharacter
{
	GENERATED_BODY()
public:
	AWorkBot();
	virtual void PossessedBy(AController* NewController) override;
	// The floor it keeps to, in world cm: set by whoever places it (Tools/facility_layout.py).
	UPROPERTY(EditAnywhere, Category = "Work Bot") FVector PatrolMin = FVector(300.0f, 300.0f, -5000.0f);
	UPROPERTY(EditAnywhere, Category = "Work Bot") FVector PatrolMax = FVector(830.0f, 1340.0f, -5000.0f);
	UPROPERTY(EditAnywhere, Category = "Work Bot") float PatrolSpeed = 0.6f;    // of the WALK speed: it never jogs (SetWalkOnly)
	UPROPERTY(EditAnywhere, Category = "Work Bot") float HuntSpeed = 1.0f;      // the full walk when it has someone
	UPROPERTY(EditAnywhere, Category = "Work Bot") float StrikeDamage = 14.0f;
	UPROPERTY(EditAnywhere, Category = "Work Bot") float StrikeReach = 150.0f;
	UPROPERTY(EditAnywhere, Category = "Work Bot") float StrikeEvery = 1.8f;
	// Conversations/Voice/<BarkFolder>/status_*.wav, notice_*, alert_*, resume_* (Tools/bake_voices.py).
	UPROPERTY(EditAnywhere, Category = "Work Bot") FString BarkFolder = TEXT("WorkBotBarks");
	UPROPERTY(VisibleAnywhere, Category = "Work Bot") TObjectPtr<UAlertnessComponent> Alertness;
};

UCLASS()
class REPLICAN_API AWorkBotController : public AAIController
{
	GENERATED_BODY()
public:
	AWorkBotController();
	virtual void OnPossess(APawn* InPawn) override;
	virtual void Tick(float DeltaSeconds) override;
	void Land(AActor* Target);   // the blow arriving, a third of a second after the swing starts
private:
	enum class EMode : uint8 { Patrol, Look, Investigate, Hunt, Dead };
	AWorkBot* Bot() const;
	void PickPatrolGoal();
	void FindClearSpot();   // placed inside a crate or a cage: the nearest free floor of the box
	void Steer(float DeltaSeconds, const FVector& Goal, float Speed);
	void Look(float DeltaSeconds);
	void FaceToward(const FVector& Point, float DeltaSeconds);
	void Say(const TArray<FString>& Lines, float Volume);
	void PlayAt(const FString& File, float Volume, const FVector* Where = nullptr);
	void Strike(AActor* Target);
	void OnAlert(EAlertState Old, EAlertState New);
	EMode Mode = EMode::Patrol;
	FVector Goal = FVector::ZeroVector;
	float GoalTimeout = 20.0f, ModeClock = 0.0f, StuckClock = 0.0f, LookSeconds = 3.0f, LookYawTarget = 0.0f;
	float NextBark = 8.0f, NextStrike = 0.0f, NextSpark = 0.0f, CurrentSpeed = -1.0f;
	float TwitchClock = 1.0f, StutterUntil = 0.0f;   // jerky: a yaw kick now and then, a step that half stops
	float StepClock = 0.3f;                          // the clank of each footfall
	// The way round an obstacle, held for a while: choosing afresh every tick made it dither left,
	// right, left against a crate -- the vibration. Also how long it has been going nowhere.
	int32 DetourSide = 0; float DetourHold = 0.0f; float NoProgress = 0.0f; FVector ProgressFrom = FVector::ZeroVector;
	// The heading actually walked, turned toward the wanted one at a rate, and how long the way
	// straight ahead has been clear. Both exist to stop the bot buzzing: see Steer.
	FVector SteerDir = FVector::ZeroVector; float DirectClearFor = 0.0f;
	// The death: sparks over the body for a couple of seconds, not one flash.
	// How many arcs the wreck throws after it goes down. It is a count rather than a duration
	// because the gaps stretch as it runs out: see the taper in Tick, where the same fraction
	// shrinks the bursts and quietens the crackle. Thirty-four of them come to roughly fourteen seconds.
	static constexpr int32 DeathSparkTotal = 34;
	int32 DeathSparksLeft = 0; float DeathSparkClock = 0.0f;
	// THE ALARM: while it hunts, every point light tagged "alarm" (the deck's red lamps) pulses; the
	// moment it gives up they settle back to what they were. The room tells you before the bot does.
	void SetAlarm(bool bOn);
	void TickAlarm(float DeltaSeconds);
	bool bAlarm = false; float AlarmClock = 0.0f;
	TArray<TWeakObjectPtr<class UPointLightComponent>> AlarmLights; TArray<float> AlarmBase;
	bool bDead = false;
	TArray<FString> StatusLines, NoticeLines, AlertLines, ResumeLines;
	UPROPERTY() TObjectPtr<class USoundAttenuation> Attenuation;
	UPROPERTY() TObjectPtr<class UAudioComponent> Voice;
};
