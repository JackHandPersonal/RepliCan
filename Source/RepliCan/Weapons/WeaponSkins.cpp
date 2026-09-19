#include "WeaponSkins.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "UObject/SoftObjectPath.h"
#include "Misc/PackageName.h"
#include "Modules/ModuleManager.h"

namespace
{
	// "M_PolygonSciFiSpace_02_F" -> stem "M_PolygonSciFiSpace": the tail is two digits, an
	// underscore and one capital letter, and every variant of one paint shares the stem.
	bool SplitVariant(const FString& Name, FString& OutStem)
	{
		const int32 N = Name.Len();
		if (N < 6) { return false; }
		const TCHAR L = Name[N - 1];
		if (L < TEXT('A') || L > TEXT('Z') || Name[N - 2] != TEXT('_')) { return false; }
		if (!FChar::IsDigit(Name[N - 3]) || !FChar::IsDigit(Name[N - 4]) || Name[N - 5] != TEXT('_')) { return false; }
		OutStem = Name.Left(N - 5);
		return true;
	}

	// Every material of one stem under the pack's Materials folder (the alternates live in a
	// sibling folder of the base paint), name -> asset path. Found once per stem per session.
	const TMap<FString, FSoftObjectPath>& VariantsOfStem(const FString& Stem, const FString& NearPath)
	{
		static TMap<FString, TMap<FString, FSoftObjectPath>> Cache;
		if (const TMap<FString, FSoftObjectPath>* Hit = Cache.Find(Stem)) { return *Hit; }
		TMap<FString, FSoftObjectPath>& Out = Cache.Add(Stem);
		FString Folder = FPackageName::GetLongPackagePath(NearPath);
		int32 At = Folder.Find(TEXT("/Materials"), ESearchCase::IgnoreCase);
		if (At != INDEX_NONE) { Folder = Folder.Left(At + 10); }
		IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
		TArray<FAssetData> Assets;
		Registry.GetAssetsByPath(FName(*Folder), Assets, true);
		for (const FAssetData& A : Assets)
		{
			const FString ClassName = A.AssetClassPath.GetAssetName().ToString();
			if (ClassName != TEXT("Material") && ClassName != TEXT("MaterialInstanceConstant")) { continue; }
			FString S;
			if (SplitVariant(A.AssetName.ToString(), S) && S == Stem) { Out.Add(A.AssetName.ToString(), A.ToSoftObjectPath()); }
		}
		return Out;
	}

	UStaticMesh* MeshOf(const WeaponCatalog::FWeapon& W)
	{
		const FString& Path = W.BodyMeshPath.IsEmpty() ? W.MeshPath : W.BodyMeshPath;
		return Path.IsEmpty() ? nullptr : LoadObject<UStaticMesh>(nullptr, *Path, nullptr, LOAD_NoWarn | LOAD_Quiet);
	}
}

TArray<FString> WeaponSkins::Variants(const WeaponCatalog::FWeapon& W)
{
	TArray<FString> Names;
	UStaticMesh* Mesh = MeshOf(W);
	if (!Mesh) { return Names; }
	for (const FStaticMaterial& SM : Mesh->GetStaticMaterials())
	{
		if (!SM.MaterialInterface) { continue; }
		FString Stem;
		if (!SplitVariant(SM.MaterialInterface->GetName(), Stem)) { continue; }
		for (const TPair<FString, FSoftObjectPath>& V : VariantsOfStem(Stem, SM.MaterialInterface->GetPathName())) { Names.AddUnique(V.Key); }
		break;   // the first painted slot decides the family; a second slot of the same stem adds nothing
	}
	Names.Sort();
	return Names;
}

FString WeaponSkins::Current(const WeaponCatalog::FWeapon& W)
{
	const TArray<FString> V = Variants(W);
	if (!W.Skin.IsEmpty() && V.Contains(W.Skin)) { return W.Skin; }
	if (UStaticMesh* Mesh = MeshOf(W))
	{
		for (const FStaticMaterial& SM : Mesh->GetStaticMaterials())
		{
			FString Stem;
			if (SM.MaterialInterface && SplitVariant(SM.MaterialInterface->GetName(), Stem)) { return SM.MaterialInterface->GetName(); }
		}
	}
	return FString();
}

void WeaponSkins::Apply(UStaticMeshComponent* Comp, const WeaponCatalog::FWeapon& W)
{
	if (!Comp || !Comp->GetStaticMesh()) { return; }
	UStaticMesh* Mesh = Comp->GetStaticMesh();
	FString SkinStem;
	const bool bChosen = !W.Skin.IsEmpty() && SplitVariant(W.Skin, SkinStem);
	for (int32 i = 0; i < Comp->GetNumMaterials(); ++i)
	{
		UMaterialInterface* Default = Mesh->GetMaterial(i);
		FString Stem;
		if (!Default || !SplitVariant(Default->GetName(), Stem)) { continue; }
		UMaterialInterface* Want = Default;
		if (bChosen && Stem == SkinStem)
		{
			if (const FSoftObjectPath* P = VariantsOfStem(Stem, Default->GetPathName()).Find(W.Skin))
			{
				if (UMaterialInterface* M = Cast<UMaterialInterface>(P->TryLoad())) { Want = M; }
			}
		}
		Comp->SetMaterial(i, Want);
	}
}
