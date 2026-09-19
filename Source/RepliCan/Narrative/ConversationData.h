// Conversation trees, one JSON file per character at
// <Project>/Conversations/<CharacterName>.json:
//
//   {
//     "start": "greet",
//     "nodes": {
//       "greet": {
//         "text": "What the character says on entering this node.",
//         "choices": [
//           { "text": "What the player can say", "next": "wares" },
//           { "text": "Only once", "next": "gift", "once": true },
//           { "text": "Needs a flag", "next": "secret", "if": "knows_password" },
//           { "text": "Sets a flag", "next": "greet", "set": "asked_about_road" },
//           { "text": "Goodbye", "end": true }
//         ]
//       },
//       "wares": { "text": "...", "choices": [ ... ] }
//     }
//   }
//
// "text" may be a list of strings (one is picked at random). A node with no
// choices (or "end": true) ends the conversation after its line. Flags are
// global strings kept on the controller (and saved with the game); "once"
// choices are remembered per character+node.
//
// Voice: a tree with a top-level "voice" block ({"model": "en_US-ryan-medium",
// "length_scale", "noise_scale", "noise_w_scale"}) gets its lines baked to
// Conversations/Voice/<Character>/<node>_<lineIndex>.wav by Tools/bake_voices.py
// (a pre-build step; only lines whose text or voice changed are rebaked).
// Line text may carry performance markup, which the game strips for display:
//   [pause 0.6]   silence in seconds ([pause] = 0.5)
//   *emphasis*    slower, a little more varied
//   ~aside~       quicker, flatter
#pragma once

#include "CoreMinimal.h"
#include "ConversationData.generated.h"

USTRUCT(BlueprintType)
struct FConversationChoice
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) FString Text;
	UPROPERTY(BlueprintReadOnly) FString Next;
	UPROPERTY(BlueprintReadOnly) FString SetFlag;      // "set"
	UPROPERTY(BlueprintReadOnly) FString RequireFlag;  // "if" (prefix "!" = must NOT have)
	UPROPERTY(BlueprintReadOnly) bool bOnce = false;
	UPROPERTY(BlueprintReadOnly) bool bEnd = false;
};

USTRUCT(BlueprintType)
struct FConversationNode
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) FString Id;
	UPROPERTY(BlueprintReadOnly) TArray<FString> Lines;   // one is spoken
	UPROPERTY(BlueprintReadOnly) TArray<FConversationChoice> Choices;
	UPROPERTY(BlueprintReadOnly) bool bEnd = false;
};

USTRUCT(BlueprintType)
struct FConversation
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) FString Character;
	UPROPERTY(BlueprintReadOnly) FString StartNode;
	UPROPERTY(BlueprintReadOnly) TMap<FString, FConversationNode> Nodes;
	UPROPERTY(BlueprintReadOnly) bool bRemote = false;
	UPROPERTY(BlueprintReadOnly) bool bUnskippable = false;   // top-level "unskippable": true -> Tab/Esc cannot leave it   // top-level "remote": true -> camera feed beside the panel
	bool IsValid() const { return Nodes.Contains(StartNode); }
};

namespace ConversationFile
{
	REPLICAN_API FString GetDirectory();
	REPLICAN_API FString GetPath(const FString& CharacterName);
	REPLICAN_API bool Exists(const FString& CharacterName);
	REPLICAN_API bool Load(const FString& CharacterName, FConversation& Out);
	// The baked voice file for one line (may not exist).
	REPLICAN_API FString GetVoicePath(const FString& CharacterName, const FString& NodeId, int32 LineIndex);
	// Line text with the performance markup removed.
	REPLICAN_API FString StripVoiceMarkup(const FString& Text);
}
