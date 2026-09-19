// Scripted sequences (the New Game intro and any later set piece), one
// JSON file per sequence at <Project>/Sequences/<Name>.json, played by
// USequenceDirector through the controller's conversation panel and a
// cinematic camera:
//
//   {
//     "actors": {
//       "tech": { "character": "Tovin", "relativeTo": "player",
//                 "offset": [180, 0, 0], "facePlayer": true }
//     },
//     "voices": { "tech": { "model": "en_US-lessac-medium" } },   // bake_voices.py
//     "steps": [
//       { "fade":   { "from": 1, "to": 0, "seconds": 1.5 } },
//       { "camera": { "anchor": "player.head", "offset": [55, 0, 4],
//                     "lookAt": "player.head", "lookOffset": [0, -10, 0],
//                     "fov": 40, "blend": 0 } },
//       { "say":    { "who": "tech", "text": "Blink for me." } },
//       { "choice": [ { "text": "Everything.", "goto": "mem_all", "set": "flag" } ] },
//       { "label":  "mem_all" },
//       { "goto":   "after" },
//       { "goto":   "twin", "if": "name=Repli Can" },     // only when the condition holds: "name=<display name>", a flag, or "!" + either
//       { "wait":   0.8 },
//       { "end":    { "fade": 0.5 } }
//     ]
//   }
//
// Anchors: "player", an actor id, or either with ".head"; offsets are in
// that actor's frame (X forward, Y right, Z up). "world" makes the offset
// an absolute position. A "say" line shows in the panel, plays its baked
// voice (Conversations/Voice/Sequences/<Name>/<stepIndex>.wav) and moves on
// when the voice ends; a click or key moves on early. "who": "player" is the
// player's own line. Choices set flags on the controller like conversations.
// "{name}" in a line is replaced by the player's chosen display name.
// Text may carry the same voice markup as conversations.
#pragma once

#include "CoreMinimal.h"
#include "SequenceData.generated.h"

UENUM()
// "shock": the activation jolt. The file starts at once (a hum that builds),
// the step lasts "build" seconds, and as it ends the jolt fires: a camera
// spasm for "spasm" seconds and a flash / eyelid flutter on the overlay.
//   { "shock": { "file": "electric_shock.wav", "build": 2.2, "spasm": 0.8, "volume": 0.8 } }
// "remote": the remote-communication feed of an actor beside the panel, or off.
//   { "remote": { "who": "officer" } }   { "remote": { "off": true } }
// "move": the look camera's eye travels to a world point over seconds with
// an unsteady sway (lateral drift, a walking bob, a roll), the current
// look targets still steering where it faces. Blocks until it arrives.
//   { "move": { "to": [330, 900, 168], "seconds": 3.4, "sway": 1.4 } }
// "panel": { "panel": "hide" } takes the conversation panel down until the next say.
// Look steps also take "turnSpeed" (default 1.1, how quickly the gaze
// settles) and "wander" (default 1, the idle drift's amplitude), so a
// groggy look can drift slowly and heavily and an alert one snap to.
// Targets and anchors may be "world:x,y,z" for a fixed point.
// "appearance": { "appearance": true } opens the appearance chooser and waits for ACCEPT.
// "release": ends the sequence into the groggy walk: the pawn is stood where the
// look camera is, first person, forward/back only, eyelids kept, until it comes
// within "radius" of "goal".
//   { "release": { "goal": [2349, 2178, 100], "radius": 170, "speed": 95 } }
enum class ESequenceStepType : uint8 { Fade, Camera, Say, Choice, Wait, Label, Goto, End, Look, Blink, Place, WaitBlinks, Sound, Shock, Remote, Move, Panel, Appearance, Turn, Ambient, Release };
// "waitBlinks": hold until the eyelid overlay has completed that many blinks.
//   { "waitBlinks": 5 }
// "sound": a one-shot from RawAudio, not blocking.
//   { "sound": { "file": "electric_buzz.wav", "volume": 0.7, "pitch": 1.0 } }
// A "look" with "seconds": 0 does not block: it keeps steering the camera
// in the background until another camera/look step or the end. Its optional
// "yawBias" (degrees, positive turns right) shifts the subject in frame.

// "look": a first-person camera parked at an anchor that drifts its gaze
// between targets, one every Period seconds, for Seconds.
//   { "look": { "anchor": "player", "offset": [0, -120, 40], "targets":
//               ["officer.head", "tech.head"], "period": 4.5, "seconds": 26, "fov": 70 } }
// "blink": the sleepy-eyes overlay (BlinkOverlayWidget), stays until the
// sequence ends or { "blink": { "off": true } }. Not a blocking step.
//   { "blink": { "openMin": 0.2, "openMax": 0.6, "closedSeconds": 3, "openSeconds": 1.4,
//                "darkStart": 0.8, "darkEnd": 0.3, "darkSeconds": 25 } }
// "place": teleport an actor (or "player") to a world position/yaw, optionally
// hidden and without collision; the player is put back when the sequence ends.
//   { "place": { "who": "player", "at": [300, 600, 60], "yaw": 90, "hidden": true } }

USTRUCT()
struct FSequenceShot
{
	GENERATED_BODY()
	UPROPERTY() FString Anchor = TEXT("player");
	UPROPERTY() FVector Offset = FVector(300, 0, 60);
	UPROPERTY() FString LookAt = TEXT("player");
	UPROPERTY() FVector LookOffset = FVector::ZeroVector;
	UPROPERTY() float Fov = 50.0f;
	UPROPERTY() float Blend = 1.0f;
};

USTRUCT()
struct FSequenceOption
{
	GENERATED_BODY()
	UPROPERTY() FString Text;
	UPROPERTY() FString Goto;
	UPROPERTY() FString SetFlag;
};

USTRUCT()
struct FSequenceStep
{
	GENERATED_BODY()
	UPROPERTY() ESequenceStepType Type = ESequenceStepType::Wait;
	UPROPERTY() float Seconds = 0.0f;          // Wait / Fade duration / End fade
	UPROPERTY() float FadeFrom = 1.0f;
	UPROPERTY() float FadeTo = 0.0f;
	UPROPERTY() FSequenceShot Shot;
	UPROPERTY() FString Who;                   // Say: actor id or "player"
	UPROPERTY() FString Text;                  // Say: display text (markup stripped)
	UPROPERTY() bool bWaitForInput = false;    // Say: hold until the player clicks
	UPROPERTY() TArray<FSequenceOption> Options;
	UPROPERTY() FString Label;                 // Label / Goto target
	UPROPERTY() FString Condition;             // Goto: "name=<display name>" or "<flag>", "!" negates; empty = always
	UPROPERTY() TArray<FString> LookTargets;   // Look
	UPROPERTY() float LookPeriod = 4.0f;
	UPROPERTY() TMap<FString, float> Numbers;  // Blink parameters by name; Place: "yaw"
	UPROPERTY() FVector Position = FVector::ZeroVector;   // Place
	UPROPERTY() bool bFlag = false;            // Blink: off; Place: hidden
};

USTRUCT()
struct FSequenceActor
{
	GENERATED_BODY()
	UPROPERTY() FString Id;
	UPROPERTY() FString Character;             // Characters/<Character>.json
	UPROPERTY() FString RelativeTo = TEXT("player");   // "player", an actor id, or "world"
	UPROPERTY() FVector Offset = FVector(180, 0, 0);    // world position when RelativeTo is "world"
	UPROPERTY() float Yaw = 180.0f;            // relative (or absolute, for "world") yaw when not facing the player
	UPROPERTY() bool bFacePlayer = true;
};

USTRUCT()
struct FSequence
{
	GENERATED_BODY()
	UPROPERTY() FString Name;
	UPROPERTY() TArray<FSequenceActor> Actors;
	UPROPERTY() TArray<FSequenceStep> Steps;
	UPROPERTY() bool bUnskippable = false;   // "unskippable": true -> Tab/Esc cannot cut it short
	// "skips": ["label", ...]: Space during a non-interactive stretch jumps to the next of these
	// labels, applying the blink/look/ambient/place state steps on the way and collapsing moves.
	UPROPERTY() TArray<FString> SkipLabels;
	int32 FindLabel(const FString& Label) const;
};

namespace SequenceFile
{
	REPLICAN_API FString GetDirectory();
	REPLICAN_API FString GetPath(const FString& Name);
	REPLICAN_API bool Exists(const FString& Name);
	REPLICAN_API bool Load(const FString& Name, FSequence& Out);
	// Baked voice for one Say step (may not exist).
	REPLICAN_API FString GetVoicePath(const FString& Name, int32 StepIndex);
}
