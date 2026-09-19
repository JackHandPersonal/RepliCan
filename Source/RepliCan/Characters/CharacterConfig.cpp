#include "Characters/CharacterConfig.h"
#include "Core/JsonDataFile.h"
#include "JsonObjectConverter.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/ARFilter.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceConstant.h"

// ---- CharacterConfigFile ------------------------------------------------

FString CharacterConfigFile::GetDirectory()
{
	return FPaths::Combine(JsonData::DataDir(), TEXT("Characters"));
}

FString CharacterConfigFile::GetPath(const FString& Name)
{
	// Keep the file name filesystem-safe without touching the display name.
	FString Stem = Name;
	for (TCHAR& C : Stem)
	{
		if (!FChar::IsAlnum(C) && C != TEXT('_') && C != TEXT('-') && C != TEXT(' ')) { C = TEXT('_'); }
	}
	return FPaths::Combine(GetDirectory(), Stem + TEXT(".json"));
}

bool CharacterConfigFile::Save(const FCharacterConfig& Config)
{
	FString Json;
	if (!FJsonObjectConverter::UStructToJsonObjectString(Config, Json))
	{
		return false;
	}
	IFileManager::Get().MakeDirectory(*GetDirectory(), true);
	const FString Path = GetPath(Config.Name);
	const bool bOk = FFileHelper::SaveStringToFile(Json, *Path);
	UE_LOG(LogTemp, Log, TEXT("CharacterConfig: %s %s"), bOk ? TEXT("saved") : TEXT("FAILED to save"), *Path);
	return bOk;
}

bool CharacterConfigFile::Load(const FString& Name, FCharacterConfig& OutConfig)
{
	FString Json;
	if (!FFileHelper::LoadFileToString(Json, *GetPath(Name)))
	{
		return false;
	}
	FCharacterConfig Loaded;
	if (!FJsonObjectConverter::JsonObjectStringToUStruct(Json, &Loaded))
	{
		UE_LOG(LogTemp, Warning, TEXT("CharacterConfig: %s is not a valid config"), *GetPath(Name));
		return false;
	}
	OutConfig = Loaded;
	return true;
}

TArray<FString> CharacterConfigFile::List()
{
	TArray<FString> Files;
	IFileManager::Get().FindFiles(Files, *FPaths::Combine(GetDirectory(), TEXT("*.json")), true, false);
	for (FString& File : Files)
	{
		File = FPaths::GetBaseFilename(File);
	}
	Files.Sort();
	return Files;
}

// ---- ModularHero --------------------------------------------------------

const TCHAR* ModularHero::BaseMeshPath()
{
	return TEXT("/Game/ModularCharacters/SK_Character_Base_01_Mesh.SK_Character_Base_01_Mesh");
}

const TCHAR* ModularHero::MaterialInstancePath()
{
	return TEXT("/Game/ModularCharacters/ModularParts/Polygon_Modular_Fantasy_Hero_Instance_Main.Polygon_Modular_Fantasy_Hero_Instance_Main");
}

const TArray<ModularHero::FSlotDef>& ModularHero::Slots()
{
	// Order here is the panel's display order. Mirrors the slot layout of
	// the pack's own BP_ModularCharacter (Gender_00..12, All_00..12).
	static const TArray<FSlotDef> Defs = []()
	{
		TArray<FSlotDef> S;
		auto G = [&S](const TCHAR* Slot, std::initializer_list<const TCHAR*> Prefixes)
		{
			FSlotDef D; D.Slot = Slot; D.bGendered = true;
			for (const TCHAR* P : Prefixes) { D.Prefixes.Add(P); }
			S.Add(D);
		};
		auto N = [&S](const TCHAR* Slot, std::initializer_list<const TCHAR*> Prefixes, FName Socket = NAME_None)
		{
			FSlotDef D; D.Slot = Slot; D.AttachSocket = Socket;
			for (const TCHAR* P : Prefixes) { D.Prefixes.Add(P); }
			S.Add(D);
		};
		G(TEXT("Head"), { TEXT("SK_Chr_Head_{G}_"), TEXT("SK_Chr_Head_No_Elements_{G}_") });
		G(TEXT("Eyebrows"), { TEXT("SK_Chr_Eyebrow_{G}_") });
		G(TEXT("FacialHair"), { TEXT("SK_Chr_FacialHair_{G}_") });
		N(TEXT("Hair"), { TEXT("SK_Chr_Hair_") });
		N(TEXT("Ears"), { TEXT("SK_Chr_Ear_Ear_") });
		N(TEXT("HeadCoverBaseHair"), { TEXT("SK_Chr_HeadCoverings_Base_Hair_") });
		N(TEXT("HeadCoverNoHair"), { TEXT("SK_Chr_HeadCoverings_No_Hair_") });
		N(TEXT("HeadCoverNoFacialHair"), { TEXT("SK_Chr_HeadCoverings_No_FacialHair_") });
		N(TEXT("Helmet"), { TEXT("SK_Chr_HelmetAttachment_") });
		G(TEXT("Torso"), { TEXT("SK_Chr_Torso_{G}_") });
		N(TEXT("ShoulderRight"), { TEXT("SK_Chr_ShoulderAttachRight_") });
		N(TEXT("ShoulderLeft"), { TEXT("SK_Chr_ShoulderAttachLeft_") });
		G(TEXT("ArmUpperRight"), { TEXT("SK_Chr_ArmUpperRight_{G}_") });
		G(TEXT("ArmUpperLeft"), { TEXT("SK_Chr_ArmUpperLeft_{G}_") });
		N(TEXT("ElbowRight"), { TEXT("SK_Chr_ElbowAttachRight_") });
		N(TEXT("ElbowLeft"), { TEXT("SK_Chr_ElbowAttachLeft_") });
		G(TEXT("ArmLowerRight"), { TEXT("SK_Chr_ArmLowerRight_{G}_") });
		G(TEXT("ArmLowerLeft"), { TEXT("SK_Chr_ArmLowerLeft_{G}_") });
		G(TEXT("HandRight"), { TEXT("SK_Chr_HandRight_{G}_") });
		G(TEXT("HandLeft"), { TEXT("SK_Chr_HandLeft_{G}_") });
		G(TEXT("Hips"), { TEXT("SK_Chr_Hips_{G}_") });
		N(TEXT("HipsAttachment"), { TEXT("SK_Chr_HipsAttachment_") });
		G(TEXT("LegRight"), { TEXT("SK_Chr_LegRight_{G}_") });
		G(TEXT("LegLeft"), { TEXT("SK_Chr_LegLeft_{G}_") });
		N(TEXT("KneeRight"), { TEXT("SK_Chr_KneeAttachRight_") });
		N(TEXT("KneeLeft"), { TEXT("SK_Chr_KneeAttachLeft_") });
		N(TEXT("Back"), { TEXT("SK_Chr_BackAttachment_") }, TEXT("spine_03"));
		return S;
	}();
	return Defs;
}

const ModularHero::FSlotDef* ModularHero::FindSlot(const FString& SlotName)
{
	return Slots().FindByPredicate([&SlotName](const FSlotDef& D) { return D.Slot == SlotName; });
}

TArray<FAssetData> ModularHero::PartOptions(const FSlotDef& Slot, const FString& Gender)
{
	// One registry query for the whole pack, cached -- ~1400 parts, and the
	// panel asks for every slot in a row.
	static TArray<FAssetData> AllParts;
	if (AllParts.Num() == 0)
	{
		const IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
		FARFilter Filter;
		Filter.ClassPaths.Add(USkeletalMesh::StaticClass()->GetClassPathName());
		Filter.PackagePaths.Add(TEXT("/Game/ModularCharacters/ModularParts"));
		Filter.PackagePaths.Add(TEXT("/Game/ModularCharacters/BackAttachments"));
		Filter.bRecursivePaths = false;
		AssetRegistry.GetAssets(Filter, AllParts);
		// Plain string order, NOT FName::LexicalLess -- FName splits a
		// trailing "_10" off as a number but leaves "_00" (leading zero) in
		// the string, so LexicalLess put every _10 before _00 and the
		// "first option" default became part 10 instead of part 00.
		AllParts.Sort([](const FAssetData& A, const FAssetData& B) { return A.AssetName.ToString() < B.AssetName.ToString(); });
	}

	TArray<FAssetData> Result;
	for (const FAssetData& Data : AllParts)
	{
		const FString Name = Data.AssetName.ToString();
		for (const FString& RawPrefix : Slot.Prefixes)
		{
			const FString Prefix = RawPrefix.Replace(TEXT("{G}"), *Gender);
			// Prefix followed by the part number -- guards Head_ against
			// Head_No_Elements_ and vice versa.
			if (Name.StartsWith(Prefix) && Name.Len() > Prefix.Len() && FChar::IsDigit(Name[Prefix.Len()]))
			{
				Result.Add(Data);
				break;
			}
		}
	}
	return Result;
}

const TArray<FString>& ModularHero::ColorParameters()
{
	static const TArray<FString> Params = {
		TEXT("Color_Primary"), TEXT("Color_Secondary"),
		TEXT("Color_Leather_Primary"), TEXT("Color_Leather_Secondary"),
		TEXT("Color_Metal_Primary"), TEXT("Color_Metal_Secondary"), TEXT("Color_Metal_Dark"),
		TEXT("Color_Skin"), TEXT("Color_Hair"), TEXT("Color_Stubble"), TEXT("Color_Scar"), TEXT("Color_BodyArt"),
	};
	return Params;
}

FCharacterConfig ModularHero::MakeDefaultConfig(const FString& Name)
{
	FCharacterConfig Config;
	Config.Name = Name;
	Config.Type = CharacterType::Modular;
	Config.Gender = TEXT("Male");
	Config.Face.bMouthDecal = false;

	// A generic, hairless human male with nothing on: the pack's _00 parts
	// are the bare body, no hair, no attachments. He does get a nose
	// (Face.NoseMesh keeps its default).
	struct FDefaultPart { const TCHAR* Slot; int32 OptionIndex; };
	static const FDefaultPart Defaults[] = {
		{ TEXT("Head"), 0 }, { TEXT("Eyebrows"), 0 },
		{ TEXT("Torso"), 0 }, { TEXT("ArmUpperRight"), 0 }, { TEXT("ArmUpperLeft"), 0 },
		{ TEXT("ArmLowerRight"), 0 }, { TEXT("ArmLowerLeft"), 0 },
		{ TEXT("HandRight"), 0 }, { TEXT("HandLeft"), 0 },
		{ TEXT("Hips"), 0 }, { TEXT("LegRight"), 0 }, { TEXT("LegLeft"), 0 },
	};
	for (const FDefaultPart& Default : Defaults)
	{
		if (const FSlotDef* Slot = FindSlot(Default.Slot))
		{
			const TArray<FAssetData> Options = PartOptions(*Slot, Config.Gender);
			if (Options.Num() > 0)
			{
				Config.Parts.Add(Slot->Slot, Options[FMath::Min(Default.OptionIndex, Options.Num() - 1)].GetObjectPathString());
			}
		}
	}
	return Config;
}

// ---- SyntyCharacters ----------------------------------------------------

namespace
{
	TArray<FAssetData> QueryAssets(const UClass* Class, std::initializer_list<const TCHAR*> Paths, bool bRecursive)
	{
		const IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
		FARFilter Filter;
		Filter.ClassPaths.Add(Class->GetClassPathName());
		for (const TCHAR* Path : Paths) { Filter.PackagePaths.Add(Path); }
		Filter.bRecursivePaths = bRecursive;
		TArray<FAssetData> Result;
		AssetRegistry.GetAssets(Filter, Result);
		// String order, not FName::LexicalLess -- see ModularHero::PartOptions.
		Result.Sort([](const FAssetData& A, const FAssetData& B) { return A.AssetName.ToString() < B.AssetName.ToString(); });
		return Result;
	}
}

TArray<FAssetData> SyntyCharacters::BaseMeshOptions()
{
	static TArray<FAssetData> Cached;
	if (Cached.Num() == 0)
	{
		for (const FAssetData& Data : QueryAssets(USkeletalMesh::StaticClass(), { TEXT("/Game") }, true))
		{
			const FString Name = Data.AssetName.ToString();
			const FString Path = Data.PackagePath.ToString();
			if (!Name.StartsWith(TEXT("SK_Chr_")) || Name.Contains(TEXT("_Attach")) || Path.StartsWith(TEXT("/Game/ModularCharacters")))
			{
				continue;
			}
			Cached.Add(Data);
		}
	}
	return Cached;
}

TArray<FAssetData> SyntyCharacters::PaletteOptions(const USkeletalMesh* Mesh)
{
	TArray<FAssetData> Result;
	if (!Mesh || Mesh->GetMaterials().Num() == 0 || !Mesh->GetMaterials()[0].MaterialInterface)
	{
		return Result;
	}
	const FString Folder = FPackageName::GetLongPackagePath(Mesh->GetMaterials()[0].MaterialInterface->GetOutermost()->GetName());
	return QueryAssets(UMaterialInstanceConstant::StaticClass(), { *Folder }, false);
}

TArray<FAssetData> SyntyCharacters::NoseOptions()
{
	TArray<FAssetData> Result;
	for (const FAssetData& Data : QueryAssets(UStaticMesh::StaticClass(), { TEXT("/Game/Characters") }, true))
	{
		if (Data.AssetName.ToString().StartsWith(TEXT("SM_Nose"))) { Result.Add(Data); }
	}
	return Result;
}

TArray<FAssetData> SyntyCharacters::HairOptions()
{
	// Only the packs whose attachments share the SOC_head convention of the
	// character meshes we use; other packs' hair sits on a different pivot.
	TArray<FAssetData> Result;
	for (const FAssetData& Data : QueryAssets(UStaticMesh::StaticClass(), { TEXT("/Game/PolygonSciFiWorlds"), TEXT("/Game/PolygonSciFiSpace"), TEXT("/Game/PolygonCyberCity"), TEXT("/Game/PolygonMilitary/Meshes/Characters/Attachments"), TEXT("/Game/Synty/PolygonPoliceStation/Meshes/CharacterAttachments") }, true))
	{
		if (Data.AssetName.ToString().StartsWith(TEXT("SM_Chr_Attach_Hair"))) { Result.Add(Data); }
	}
	Result.Sort([](const FAssetData& A, const FAssetData& B) { return A.AssetName.ToString() < B.AssetName.ToString(); });
	return Result;
}

TArray<FAssetData> SyntyCharacters::HeadGearOptions()
{
	TArray<FAssetData> Result;
	for (const FAssetData& Data : QueryAssets(UStaticMesh::StaticClass(), { TEXT("/Game/PolygonSciFiWorlds"), TEXT("/Game/PolygonSciFiSpace"), TEXT("/Game/PolygonCyberCity"), TEXT("/Game/PolygonMilitary/Meshes/Characters/Attachments"), TEXT("/Game/Synty/PolygonPoliceStation/Meshes/CharacterAttachments") }, true))
	{
		const FString Name = Data.AssetName.ToString();
		if (!Name.StartsWith(TEXT("SM_Chr_Attach_")) && !Name.StartsWith(TEXT("SM_Char_Attach_"))) { continue; }
		// The Military and Police packs add berets, beanies, turbans, eyepatches, ear defenders, night-vision rigs and glasses.
		if (Name.Contains(TEXT("Hat")) || Name.Contains(TEXT("Helmet")) || Name.Contains(TEXT("Cap")) || Name.Contains(TEXT("Mask")) || Name.Contains(TEXT("Headset")) || Name.Contains(TEXT("Goggles"))
			|| Name.Contains(TEXT("Beret")) || Name.Contains(TEXT("Beanie")) || Name.Contains(TEXT("Turban")) || Name.Contains(TEXT("Eyepatch")) || Name.Contains(TEXT("Earmuffs")) || Name.Contains(TEXT("NVG")) || Name.Contains(TEXT("Glasses")) || Name.Contains(TEXT("Headpiece")) || Name.Contains(TEXT("Ear_Piece")))
		{
			if (!Name.Contains(TEXT("Glass"))) { Result.Add(Data); }
		}
	}
	Result.Sort([](const FAssetData& A, const FAssetData& B) { return A.AssetName.ToString() < B.AssetName.ToString(); });
	return Result;
}

const TCHAR* SyntyCharacters::DefaultBaseMeshPath()
{
	// A plain Sci-Fi Space crew member; every Synty UE4-Mannequin character
	// shares its rig, so face offsets and grips carry over.
	return TEXT("/Game/PolygonSciFiSpace/Meshes/CharactersUE4/SK_Chr_Crew_Male_01.SK_Chr_Crew_Male_01");
}

FCharacterConfig SyntyCharacters::MakeDefaultConfig(const FString& Name, const FString& BaseMeshPath, const FString& MaterialPath)
{
	FCharacterConfig Config;
	Config.Name = Name;
	Config.Type = CharacterType::Single;
	Config.BaseMesh = BaseMeshPath.IsEmpty() ? DefaultBaseMeshPath() : BaseMeshPath;
	Config.Material = MaterialPath;
	return Config;
}

// ---- Cut library --------------------------------------------------------------

const TArray<FString>& CutLibrary::Slots()
{
	static const TArray<FString> S = { TEXT("CutHead"), TEXT("CutTorso"), TEXT("CutArms"), TEXT("CutLegs") };
	return S;
}

FString CutLibrary::PartSuffix(const FString& Slot)
{
	return Slot.StartsWith(TEXT("Cut")) ? TEXT("_") + Slot.RightChop(3) : FString();
}

const TCHAR* CutLibrary::BaseMeshPath()
{
	return TEXT("/Game/PolygonSciFiSpace/Meshes/CharactersUE4/SK_Chr_Crew_Male_01.SK_Chr_Crew_Male_01");
}

TArray<FAssetData> CutLibrary::PartOptions(const FString& Slot)
{
	static TArray<FAssetData> All;
	if (All.Num() == 0)
	{
		const IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
		FARFilter Filter;
		Filter.ClassPaths.Add(USkeletalMesh::StaticClass()->GetClassPathName());
		Filter.PackagePaths.Add(TEXT("/Game/RepliCan/CutLibrary"));
		Filter.bRecursivePaths = true;
		AssetRegistry.GetAssets(Filter, All);
		All.Sort([](const FAssetData& A, const FAssetData& B) { return A.AssetName.ToString() < B.AssetName.ToString(); });
	}
	const FString Suffix = PartSuffix(Slot);
	TArray<FAssetData> Out;
	for (const FAssetData& D : All) { if (!Suffix.IsEmpty() && D.AssetName.ToString().EndsWith(Suffix)) { Out.Add(D); } }
	return Out;
}

// ---- Appearance palette ---------------------------------------------------------

static TArray<FAssetData> AppearanceScan(const TArray<FString>& Paths, bool bRecursive)
{
	const IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
	FARFilter Filter;
	Filter.ClassPaths.Add(UStaticMesh::StaticClass()->GetClassPathName());
	for (const FString& P : Paths) { Filter.PackagePaths.Add(*P); }
	Filter.bRecursivePaths = bRecursive;
	TArray<FAssetData> Out;
	AssetRegistry.GetAssets(Filter, Out);
	Out.Sort([](const FAssetData& A, const FAssetData& B) { return A.AssetName.ToString() < B.AssetName.ToString(); });
	return Out;
}

static FString PrettyName(FString Name)
{
	for (const TCHAR* Strip : { TEXT("SM_Chr_Attach_"), TEXT("SK_Chr_"), TEXT("_Head") }) { Name.ReplaceInline(Strip, TEXT("")); }
	Name.ReplaceInline(TEXT("_"), TEXT(" "));
	return Name;
}

const TArray<Appearance::FBody>& Appearance::Bodies()
{
	static const TArray<FBody> B = {
		// Our own assets (Content/RepliCan/PlayerCharacter): the cryo pair copied, the head stripped of its hair and fringe.
		{ TEXT("Female"), TEXT("/Game/RepliCan/PlayerCharacter/SK_PC_Female_01_Torso.SK_PC_Female_01_Torso"), TEXT("/Game/RepliCan/PlayerCharacter/SK_PC_Female_01_Arms.SK_PC_Female_01_Arms"), TEXT("/Game/RepliCan/PlayerCharacter/SK_PC_Female_01_Legs.SK_PC_Female_01_Legs"), TEXT("/Game/RepliCan/PlayerCharacter/SK_PC_Female_01_Head.SK_PC_Female_01_Head"), FVector(11.0f, 12.3f, 4.5f) },
		{ TEXT("Male"),   TEXT("/Game/RepliCan/PlayerCharacter/SK_PC_Male_01_Torso.SK_PC_Male_01_Torso"),     TEXT("/Game/RepliCan/PlayerCharacter/SK_PC_Male_01_Arms.SK_PC_Male_01_Arms"),     TEXT("/Game/RepliCan/PlayerCharacter/SK_PC_Male_01_Legs.SK_PC_Male_01_Legs"),     TEXT("/Game/RepliCan/PlayerCharacter/SK_PC_Male_01_Head.SK_PC_Male_01_Head"),     FVector(11.7f, 12.3f, 4.3f) },
		// The same bodies with the Fantasy Hero base skull (SK_PC_*_02_Head: hero geometry on our rig, hero
		// parametric material). A true bald head, so the hero hair pieces sit on it as designed.
		{ TEXT("Female / hero head"), TEXT("/Game/RepliCan/PlayerCharacter/SK_PC_Female_01_Torso.SK_PC_Female_01_Torso"), TEXT("/Game/RepliCan/PlayerCharacter/SK_PC_Female_01_Arms.SK_PC_Female_01_Arms"), TEXT("/Game/RepliCan/PlayerCharacter/SK_PC_Female_01_Legs.SK_PC_Female_01_Legs"), TEXT("/Game/RepliCan/PlayerCharacter/SK_PC_Female_02_Head.SK_PC_Female_02_Head"), FVector(11.0f, 12.3f, 4.5f) },
		{ TEXT("Male / hero head"),   TEXT("/Game/RepliCan/PlayerCharacter/SK_PC_Male_01_Torso.SK_PC_Male_01_Torso"),     TEXT("/Game/RepliCan/PlayerCharacter/SK_PC_Male_01_Arms.SK_PC_Male_01_Arms"),     TEXT("/Game/RepliCan/PlayerCharacter/SK_PC_Male_01_Legs.SK_PC_Male_01_Legs"),     TEXT("/Game/RepliCan/PlayerCharacter/SK_PC_Male_02_Head.SK_PC_Male_02_Head"),     FVector(11.7f, 12.3f, 4.3f) },
		// The junkers' kit (POLYGON Sci-Fi Space, cut by bone into the library by Tools/cut_library): the salvager's
		// clothes on the same two heads, so hair, brows and skin carry across. The junker head itself stays in the
		// cut library for extras; the player keeps their own face.
		{ TEXT("Female / junker"), TEXT("/Game/RepliCan/CutLibrary/SciFiSpace/Junker_Female_01_Torso.Junker_Female_01_Torso"), TEXT("/Game/RepliCan/CutLibrary/SciFiSpace/Junker_Female_01_Arms.Junker_Female_01_Arms"), TEXT("/Game/RepliCan/CutLibrary/SciFiSpace/Junker_Female_01_Legs.Junker_Female_01_Legs"), TEXT("/Game/RepliCan/PlayerCharacter/SK_PC_Female_01_Head.SK_PC_Female_01_Head"), FVector(11.0f, 12.3f, 4.5f) },
		{ TEXT("Male / junker"),   TEXT("/Game/RepliCan/CutLibrary/SciFiSpace/Junker_Male_01_Torso.Junker_Male_01_Torso"),     TEXT("/Game/RepliCan/CutLibrary/SciFiSpace/Junker_Male_01_Arms.Junker_Male_01_Arms"),     TEXT("/Game/RepliCan/CutLibrary/SciFiSpace/Junker_Male_01_Legs.Junker_Male_01_Legs"),     TEXT("/Game/RepliCan/PlayerCharacter/SK_PC_Male_01_Head.SK_PC_Male_01_Head"),     FVector(11.7f, 12.3f, 4.3f) },
	};
	return B;
}

const TArray<Appearance::FOption>& Appearance::Heads()
{
	static TArray<FOption> H;
	if (H.Num() == 0)
	{
		const TCHAR* NotHuman[] = { TEXT("Alien"), TEXT("Robot"), TEXT("Bot_"), TEXT("War_"), TEXT("Psionic"), TEXT("EVA"), TEXT("Strider"), TEXT("Helper") };
		for (const FAssetData& D : CutLibrary::PartOptions(TEXT("CutHead")))
		{
			const FString Name = D.AssetName.ToString();
			bool bSkip = false;
			for (const TCHAR* N : NotHuman) { if (Name.Contains(N)) { bSkip = true; break; } }
			if (!bSkip) { H.Add({ PrettyName(Name), D.GetObjectPathString() }); }
		}
	}
	return H;
}

// Names for the hero-pack hairstyles (SM_Hero_Hair_01..38), from a contact sheet.
static const TCHAR* HeroHairNames[38] = {
	TEXT("High ponytail"), TEXT("Long straight"), TEXT("Side part"), TEXT("Slicked back"), TEXT("Short flick"), TEXT("Thin comb-over"),
	TEXT("Short crop"), TEXT("Undercut"), TEXT("Short wavy"), TEXT("Quiff"), TEXT("Textured crop"), TEXT("Crew cut"),
	TEXT("Messy spikes"), TEXT("Side-swept"), TEXT("Mohawk"), TEXT("Buzz cut"), TEXT("Fauxhawk"), TEXT("Short back and sides"),
	TEXT("Top knot"), TEXT("Bob with fringe"), TEXT("Rounded bob"), TEXT("Dreadlocks"), TEXT("Fringe flick"), TEXT("Braids"),
	TEXT("Headband, long"), TEXT("Messy bun"), TEXT("Wavy"), TEXT("Angled bob"), TEXT("Pigtails"), TEXT("Headband, ponytail"),
	TEXT("Layered"), TEXT("Windswept"), TEXT("Pageboy"), TEXT("Medium straight"), TEXT("Shoulder length"), TEXT("Swept back, long"),
	TEXT("Curly"), TEXT("Long braid"),
};

const TArray<Appearance::FOption>& Appearance::Hair()
{
	// Hero-pack pieces only: the Synty SM_Chr_Attach hair is cut for heads
	// with painted hair and reads as broken or nearly bald on ours.
	static TArray<FOption> H;
	if (H.Num() == 0)
	{
		for (const FOption& O : HeroPieces(TEXT("SM_Hero_Hair_")))
		{
			const int32 N = FCString::Atoi(*FPaths::GetBaseFilename(O.Path).RightChop(13));   // after "SM_Hero_Hair_"
			H.Add({ (N >= 1 && N <= 38) ? HeroHairNames[N - 1] : O.Label, O.Path });
		}
	}
	return H;
}

const TArray<Appearance::FOption>& Appearance::Beards()
{
	static TArray<FOption> B;
	if (B.Num() == 0)
	{
		// Worlds' beards, then the Military pack's twelve (with their moustache and patch variants) and the Police pack's one.
		for (const FAssetData& D : AppearanceScan({ TEXT("/Game/PolygonSciFiWorlds/Models/CharactersUE4Mannequin/Attachments"), TEXT("/Game/PolygonMilitary/Meshes/Characters/Attachments"), TEXT("/Game/Synty/PolygonPoliceStation/Meshes/CharacterAttachments") }, false))
		{
			const FString Name = D.AssetName.ToString();
			if (Name.Contains(TEXT("Beard")) || Name.Contains(TEXT("Mustache"))) { B.Add({ PrettyName(Name), D.GetObjectPathString() }); }
		}
		B.Append(HeroPieces(TEXT("SM_Hero_FacialHair_")));
	}
	return B;
}

const TArray<Appearance::FOption>& Appearance::HeadsFor(bool bFemale)
{
	static TArray<FOption> F, M;
	if (F.Num() == 0 && M.Num() == 0)
	{
		for (const FOption& O : Heads())
		{
			const bool bF = O.Path.Contains(TEXT("Female"));
			const bool bM = !bF && O.Path.Contains(TEXT("Male"));
			if (bF || !bM) { F.Add(O); }
			if (bM || !bF) { M.Add(O); }
		}
	}
	return bFemale ? F : M;
}

const TArray<Appearance::FNose>& Appearance::Noses()
{
	static const TCHAR* Plain = TEXT("/Game/Characters/KnightDemo/SM_Nose.SM_Nose");
	static const TCHAR* Sharp = TEXT("/Game/Characters/KnightDemo/SM_NoseSharp.SM_NoseSharp");
	static const TCHAR* Roman = TEXT("/Game/Characters/KnightDemo/SM_Nose_01.SM_Nose_01");
	static const TArray<FNose> N = {
		{ TEXT("Straight"),      Plain, FVector(1.0f, 1.0f, 1.0f) },
		{ TEXT("Button"),        Plain, FVector(0.8f, 0.85f, 0.85f) },
		{ TEXT("Broad"),         Plain, FVector(1.0f, 1.35f, 1.0f) },
		{ TEXT("Long"),          Plain, FVector(1.0f, 1.0f, 1.3f) },
		{ TEXT("Sharp"),         Sharp, FVector(1.0f, 1.0f, 1.0f) },
		{ TEXT("Sharp, small"),  Sharp, FVector(0.85f, 0.85f, 0.85f) },
		{ TEXT("Sharp, long"),   Sharp, FVector(1.0f, 1.0f, 1.3f) },
		{ TEXT("Roman"),         Roman, FVector(1.0f, 1.0f, 1.0f) },
		{ TEXT("Roman, broad"),  Roman, FVector(1.0f, 1.3f, 1.05f) },
		{ TEXT("Roman, small"),  Roman, FVector(0.85f, 0.85f, 0.9f) },
		{ TEXT("None"),          TEXT(""), FVector(1.0f, 1.0f, 1.0f) },
	};
	return N;
}

TArray<Appearance::FOption> Appearance::HeroPieces(const TCHAR* Prefix)
{
	TArray<FOption> Out;
	for (const FAssetData& D : AppearanceScan({ TEXT("/Game/RepliCan/PlayerCharacter/Hero") }, false))
	{
		const FString Name = D.AssetName.ToString();
		if (Name.StartsWith(Prefix)) { Out.Add({ Name.Mid(FCString::Strlen(TEXT("SM_Hero_"))).Replace(TEXT("_"), TEXT(" ")), D.GetObjectPathString() }); }
	}
	Out.Sort([](const FOption& A, const FOption& B) { return A.Label < B.Label; });
	return Out;
}

const TArray<Appearance::FHairColor>& Appearance::HairColors()
{
	static const TArray<FHairColor> C = {
		{ TEXT("Brown"),      FLinearColor(0, 0, 0, 0) },   // the pieces as painted (alpha 0 = no tint)
		{ TEXT("Black"),      FLinearColor(0.02f, 0.02f, 0.025f, 1) },
		{ TEXT("Dark brown"), FLinearColor(0.10f, 0.06f, 0.035f, 1) },
		{ TEXT("Chocolate"),  FLinearColor(0.24f, 0.13f, 0.07f, 1) },
		{ TEXT("Auburn"),     FLinearColor(0.36f, 0.12f, 0.06f, 1) },
		{ TEXT("Red"),        FLinearColor(0.55f, 0.16f, 0.07f, 1) },
		{ TEXT("Blonde"),     FLinearColor(0.72f, 0.55f, 0.28f, 1) },
		{ TEXT("Ash"),        FLinearColor(0.42f, 0.40f, 0.36f, 1) },
		{ TEXT("Grey"),       FLinearColor(0.60f, 0.60f, 0.60f, 1) },
		{ TEXT("White"),      FLinearColor(0.90f, 0.90f, 0.88f, 1) },
		{ TEXT("Teal"),       FLinearColor(0.10f, 0.45f, 0.45f, 1) },
		{ TEXT("Violet"),     FLinearColor(0.35f, 0.12f, 0.50f, 1) },
	};
	return C;
}

const TArray<Appearance::FBrow>& Appearance::Brows()
{
	static const TCHAR* Bar = TEXT("/Game/RepliCan/PlayerCharacter/SM_PC_Brow_01.SM_PC_Brow_01");
	static const TCHAR* Fine = TEXT("/Game/RepliCan/PlayerCharacter/SM_PC_Brow_02.SM_PC_Brow_02");
	static TArray<FBrow> B;
	if (B.Num() > 0) { return B; }
	for (const FOption& O : HeroPieces(TEXT("SM_Hero_Eyebrow_"))) { B.Add({ O.Label, O.Path, 0.0f, 0.0f, 0.0f, FVector(1.0f, 1.0f, 1.0f) }); }
	B.Append({
		{ TEXT("Natural"),  Bar,  0.0f,   0.0f,  0.0f, FVector(1.0f, 1.0f, 1.0f) },
		{ TEXT("Heavy"),    Bar,  0.0f,   0.0f,  0.0f, FVector(1.6f, 1.3f, 1.15f) },
		{ TEXT("Thin"),     Fine, 0.0f,   0.0f,  0.0f, FVector(1.0f, 1.0f, 1.0f) },
		{ TEXT("Arched"),   Fine, -8.0f,  0.6f,  0.0f, FVector(1.0f, 1.0f, 1.05f) },
		{ TEXT("Angry"),    Bar,  14.0f, -0.3f,  0.0f, FVector(1.2f, 1.1f, 1.0f) },
		{ TEXT("Worried"),  Bar,  -14.0f, 0.4f,  0.0f, FVector(1.0f, 1.0f, 1.0f) },
		{ TEXT("Raised"),   Bar,  0.0f,   1.4f,  0.0f, FVector(1.0f, 1.0f, 1.0f) },
		{ TEXT("Low"),      Bar,  4.0f,  -1.0f,  0.0f, FVector(1.3f, 1.1f, 1.0f) },
		{ TEXT("Wide"),     Bar,  0.0f,   0.2f,  1.2f, FVector(1.0f, 1.0f, 1.1f) },
		{ TEXT("Close"),    Bar,  6.0f,   0.0f, -0.9f, FVector(1.1f, 1.0f, 0.95f) },
		{ TEXT("None"),     TEXT(""), 0.0f, 0.0f, 0.0f, FVector(1.0f, 1.0f, 1.0f) },
	});
	return B;
}

static bool PieceFitsSex(const FString& NameOrPath, bool bFemale)
{
	const FString Leaf = FPaths::GetBaseFilename(NameOrPath);
	if (Leaf.Contains(TEXT("Female"), ESearchCase::CaseSensitive)) { return bFemale; }
	if (Leaf.Contains(TEXT("Male"), ESearchCase::CaseSensitive)) { return !bFemale; }
	return true;
}

const TArray<Appearance::FOption>& Appearance::HairFor(bool bFemale)
{
	static TArray<FOption> Lists[2];
	TArray<FOption>& L = Lists[bFemale ? 1 : 0];
	if (L.Num() == 0)
	{
		for (const FOption& O : Hair()) { if (PieceFitsSex(O.Path, bFemale)) { L.Add({ O.Label, O.Path }); } }
	}
	return L;
}

const TArray<Appearance::FOption>& Appearance::BeardsFor(bool bFemale)
{
	static TArray<FOption> Lists[2];
	TArray<FOption>& L = Lists[bFemale ? 1 : 0];
	if (L.Num() == 0 && !bFemale)
	{
		for (const FOption& O : Beards()) { if (PieceFitsSex(O.Path, bFemale)) { L.Add({ FString::FromInt(L.Num() + 1), O.Path }); } }
	}
	return L;
}

const TArray<Appearance::FBrow>& Appearance::BrowsFor(bool bFemale)
{
	static TArray<FBrow> Lists[2];
	TArray<FBrow>& L = Lists[bFemale ? 1 : 0];
	if (L.Num() == 0)
	{
		int32 N = 0;
		for (const FBrow& B : Brows())
		{
			if (!PieceFitsSex(B.Mesh, bFemale)) { continue; }
			FBrow Copy = B;
			if (B.Mesh.Contains(TEXT("/Hero/"))) { Copy.Label = FString::Printf(TEXT("Brow %d"), ++N); }
			L.Add(Copy);
		}
	}
	return L;
}

const TArray<Appearance::FSkin>& Appearance::Skins()
{
	// Linear-space tints; the painted skin is roughly (1.0, 0.6, 0.42).
	static const TArray<FSkin> S = {
		// Always applied: there is no "as painted" entry, the selected tone is the skin.
		{ TEXT("Porcelain"), FLinearColor(1.00f, 0.82f, 0.70f, 1.0f) },
		{ TEXT("Pale"),     FLinearColor(0.95f, 0.74f, 0.60f, 1.0f) },
		{ TEXT("Fair"),     FLinearColor(0.92f, 0.66f, 0.50f, 1.0f) },
		{ TEXT("Peach"),    FLinearColor(1.00f, 0.60f, 0.42f, 1.0f) },
		{ TEXT("Golden"),   FLinearColor(0.88f, 0.58f, 0.34f, 1.0f) },
		{ TEXT("Light tan"), FLinearColor(0.80f, 0.52f, 0.31f, 1.0f) },
		{ TEXT("Olive"),    FLinearColor(0.66f, 0.44f, 0.27f, 1.0f) },
		{ TEXT("Tan"),      FLinearColor(0.55f, 0.33f, 0.18f, 1.0f) },
		{ TEXT("Brown"),    FLinearColor(0.36f, 0.20f, 0.10f, 1.0f) },
		{ TEXT("Deep"),     FLinearColor(0.20f, 0.10f, 0.05f, 1.0f) },
		{ TEXT("Ebony"),    FLinearColor(0.10f, 0.05f, 0.03f, 1.0f) },
		// Not a human tone, and last on the list for that reason.
		//
		// MEASURED, not invented. The cafeteria line-up wears the SciFi Space alternate palette
		// M_PolygonSciFiSpace_02_F, and the skin swatch in that variant is a dark yellow-green:
		// the junkers came out green and this is the colour they came out. Tools/sample_skin_colour.py
		// read it off the atlas at the face's own UVs -- sRGB (99, 115, 79), #63734F. The same
		// tool read the reference head on palette 01_A as linear (1.000, 0.604, 0.418), which is
		// the "roughly (1.0, 0.6, 0.42)" noted at the top of this list, so the method is sound.
		{ TEXT("Moss"),     FLinearColor(0.125f, 0.171f, 0.078f, 1.0f) },
	};
	return S;
}

// ---- Attributes -------------------------------------------------------------

const TArray<FName>& FAttributes::Names()
{
	static const TArray<FName> N = { TEXT("Brawn"), TEXT("Agility"), TEXT("Endurance"), TEXT("Cognition"), TEXT("Tech"), TEXT("Presence") };
	return N;
}

int32 FAttributes::Get(FName Which) const
{
	if (Which == TEXT("Brawn")) { return Brawn; }
	if (Which == TEXT("Agility")) { return Agility; }
	if (Which == TEXT("Endurance")) { return Endurance; }
	if (Which == TEXT("Cognition")) { return Cognition; }
	if (Which == TEXT("Tech")) { return Tech; }
	if (Which == TEXT("Presence")) { return Presence; }
	return 0;
}

void FAttributes::Set(FName Which, int32 Value)
{
	const int32 V = FMath::Clamp(Value, Min, Max);
	if (Which == TEXT("Brawn")) { Brawn = V; }
	else if (Which == TEXT("Agility")) { Agility = V; }
	else if (Which == TEXT("Endurance")) { Endurance = V; }
	else if (Which == TEXT("Cognition")) { Cognition = V; }
	else if (Which == TEXT("Tech")) { Tech = V; }
	else if (Which == TEXT("Presence")) { Presence = V; }
}

TArray<TPair<FName, int32>> FAttributes::Derived(const FAttributes& A)
{
	// Deliberately simple and readable off the sheet: a player should be able to raise Endurance
	// and see exactly which three lines moved. Balance can come later; opacity cannot be undone.
	return {
		// Everything on the same hundred-point scale as the attributes: a weighted mean of the
		// scores that stand in front of that kind of harm, so a score of 50 everywhere reads 50 here.
		{ TEXT("Vitality"),   (A.Endurance * 3 + A.Brawn) / 4 },     // how much punishment before it matters
		{ TEXT("Stamina"),    (A.Endurance * 3 + A.Agility) / 4 },   // how long the body keeps going at pace
		// The six DEFENCES, one per damage type: Endurance, the body's tolerance, with the
		// attribute that stands in front of that kind of harm.
		{ TEXT("Kinetic"),    (A.Endurance + A.Brawn) / 2 },         // bullets and blades, before armour
		{ TEXT("Heat"),       (A.Endurance + A.Tech) / 2 },          // fire, plasma, vacuum burn: suit discipline
		{ TEXT("Cold"),       A.Endurance },                         // vacuum chill, cryo, a dead heater
		{ TEXT("Electrical"), (A.Tech + A.Cognition) / 2 },          // arcs, shock sticks, a live panel
		{ TEXT("Corrosion"),  (A.Endurance + A.Presence) / 2 },      // acid, solvents, bad air: keeping a head about it
		{ TEXT("Radiation"),  (A.Endurance + A.Cognition) / 2 },     // the reactor deck, a cracked cell
	};
}

FString FAttributes::Describe(FName Which)
{
	if (Which == TEXT("Brawn")) { return TEXT("muscle, lifting, melee force"); }
	if (Which == TEXT("Agility")) { return TEXT("coordination, speed, dodging"); }
	if (Which == TEXT("Endurance")) { return TEXT("stamina, immunity, toxins"); }
	if (Which == TEXT("Cognition")) { return TEXT("memory, deduction, learning"); }
	if (Which == TEXT("Tech")) { return TEXT("repair, hacking, ship systems"); }
	if (Which == TEXT("Presence")) { return TEXT("leadership, threat, persuasion"); }
	return FString();
}
