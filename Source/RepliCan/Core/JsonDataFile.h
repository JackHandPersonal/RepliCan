#pragma once
#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

// THE ONE PLACE THAT KNOWS WHERE THE GAME'S DATA LIVES.
//
// RepliCan's data is not in Content/. The weapon and item catalogues, the character sheet spec,
// terminal text, conversations, sequences, character configs and the impact tables are plain JSON
// under the project directory, read at runtime. Thirteen files used to implement load / parse /
// log independently across twenty-three read sites, each re-deciding what to do about a missing
// file, a parse failure and a documentation key.
//
// Two things that were learned the hard way and are now true in one place rather than thirteen:
//
//  * PATH RESOLUTION IS A PACKAGING CONCERN. These directories are staged into the pak by
//    DirectoriesToAlwaysStageAsUFS in DefaultGame.ini. A cooked build has no loose files, and a
//    read that bypasses IFileManager will not find them. Everything here goes through FFileHelper,
//    which resolves pak contents transparently.
//  * A FAILURE MUST NAME THE FILE. A catalogue that silently comes back empty looks exactly like a
//    catalogue with nothing in it, which is how a packaging bug survived until someone cooked.
//
// The per-caller part -- which fields an entry has, what struct it fills -- deliberately stays with
// the caller. This owns the plumbing, not the schema.

namespace JsonData
{
	// A key beginning with "_" is documentation written for a human reading the file, not an entry.
	// Every catalogue in the project uses this convention; it used to be re-spelled at each loop.
	inline bool IsDocKey(const FString& Key) { return Key.StartsWith(TEXT("_")); }

	// THE ROOT OF THE AUTHORED DATA: <ProjectContentDir>/GameData.
	//
	// It lives under Content/ for one reason: DirectoriesToAlwaysStageAsUFS resolves its paths
	// relative to the content directory, not the project root, so data outside Content/ CANNOT be
	// staged and a packaged build comes up with every catalogue empty -- while the cook still
	// reports success. One container rather than six top-level folders because Content/Characters
	// already exists with 1,577 animation assets.
	REPLICAN_API FString DataDir();

	// <DataDir>/<SubDir>/<FileName>, e.g. Path(TEXT("UI"), TEXT("Weapons.json")).
	REPLICAN_API FString Path(const TCHAR* SubDir, const TCHAR* FileName);

	// Load and parse one JSON object. Returns null and logs a warning naming both the file and
	// Who (the caller's name, used as the log prefix) if the file is missing or does not parse.
	REPLICAN_API TSharedPtr<FJsonObject> LoadObject(const FString& FullPath, const TCHAR* Who);

	// Path() + LoadObject() in one call, which is what almost every caller wants.
	REPLICAN_API TSharedPtr<FJsonObject> Load(const TCHAR* SubDir, const TCHAR* FileName, const TCHAR* Who);

	// The object-valued entries of Root's Field, with documentation keys skipped. Returns false if
	// the field is absent or is not an object, so a caller can tell "no such section" from "empty".
	REPLICAN_API bool ForEachEntry(const TSharedPtr<FJsonObject>& Root, const TCHAR* Field,
		TFunctionRef<void(const FString& Key, const TSharedPtr<FJsonObject>& Entry)> Fn);

	// File modification time, for the live-reload checks. Returns an invalid FDateTime when the
	// file is not there -- and note it is meaningless for a file inside a pak, so reload polling
	// is an editor convenience and must never be something the game depends on.
	REPLICAN_API FDateTime TimeStamp(const FString& FullPath);
}
