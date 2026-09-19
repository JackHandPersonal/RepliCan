// Plays an FSequence (see SequenceData.h): spawns or finds its actors,
// then walks the steps -- fades, cinematic camera shots, spoken lines in
// the conversation panel, player choices -- and hands control back to the
// pawn at "end". Owned and ticked by ABasePlayerController, which also
// routes the panel's clicks (ChooseConversationOption) here while active.
#pragma once

#include "CoreMinimal.h"
#include "Narrative/SequenceData.h"
#include "UObject/Object.h"
#include "SequenceDirector.generated.h"

class ABaseCharacter;
class ABasePlayerController;

UCLASS()
class REPLICAN_API USequenceDirector : public UObject
{
	GENERATED_BODY()

public:
	bool Start(ABasePlayerController* InController, const FSequence& InSequence);
	void Tick(float DeltaSeconds);
	bool IsActive() const { return bActive; }
	bool IsUnskippable() const { return bActive && Sequence.bUnskippable; }
	// The player clicked / pressed: a Say line continues, a Choice picks.
	void OnChoice(int32 Index);
	void Advance();              // next step now (debug)
	void Skip();                 // straight to the release
	// Space: to the next "skips" label when the current step is not a line or a reply.
	bool CanSkipAhead() const;
	void SkipAhead();
	FString Describe() const;    // "<Name> step <i>/<n> <type>" for tests

private:
	void BeginStep(int32 Index);
	bool ConditionHolds(const FString& Condition) const;   // a Goto step's "if"
	void FinishSequence(float CameraBlend);
	ABaseCharacter* FindActor(const FString& Id) const;
	bool ResolveAnchor(const FString& Anchor, const FVector& Offset, FVector& Out) const;
	void SpawnActors();
	FString SpeakerName(const FString& Who) const;

	void TickLook(float DeltaSeconds);
	// The jolt: a decaying camera spasm layered over the look camera.
	void FireJolt(const FSequenceStep& Step);
	float ShakeLeft = 0.0f;
	float ShakeTotal = 0.0f;
	float ShakeClock = 0.0f;
	// A "move" in flight: the eye slides from MoveFrom to MoveTo with sway.
	// An actor turning on the spot (the "turn" step).
	TWeakObjectPtr<ABaseCharacter> TurnActor;
	float TurnFromYaw = 0.0f, TurnToYaw = 0.0f, TurnSeconds = 1.0f, TurnElapsed = 0.0f;
	bool bMoving = false;
	FVector MoveFrom = FVector::ZeroVector;
	FVector MoveTo = FVector::ZeroVector;
	float MoveSeconds = 1.0f;
	float MoveElapsed = 0.0f;
	float MoveSway = 1.0f;
	float SwayRoll = 0.0f;

	UPROPERTY() TObjectPtr<ABasePlayerController> Controller;
	UPROPERTY() TMap<FString, TObjectPtr<ABaseCharacter>> Actors;
	FSequence Sequence;
	int32 StepIndex = INDEX_NONE;
	float StepElapsed = 0.0f;
	float StepDuration = 0.0f;    // 0 = wait for input
	bool bActive = false;
	bool bEnding = false;
	bool bKeepBlink = false;   // a "release": the eyelids stay for the groggy walk
	bool bSkipping = false;    // SkipAhead in progress: BeginStep fast-forwards to SkipTarget
	int32 SkipTarget = INDEX_NONE;
	int32 NextSkipTarget() const;
	// Look state: the step whose targets steer the camera (a "look" with
	// seconds 0 keeps steering after the sequence has moved on).
	int32 LookStepIndex = INDEX_NONE;
	float LookElapsed = 0.0f;
	FVector LookEye = FVector::ZeroVector;
	FRotator LookRotation = FRotator::ZeroRotator;
	int32 LookIndex = 0;
	// The player's pre-sequence state when a "place" step moved them.
	bool bPlayerMoved = false;
	FTransform PlayerOriginal;
	bool bPlayerWasHidden = false;
	bool bPlayerHadCollision = true;
};
