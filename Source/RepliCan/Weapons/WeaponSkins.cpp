#include "Weapons/WeaponSkins.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstance.h"
#include "UObject/SoftObjectPath.h"
#include "Misc/PackageName.h"
#include "Modules/ModuleManager.h"

namespace
{
	// "M_PolygonSciFiSpace_02_F" -> stem "M_PolygonSciFiSpace": the tail is two digits, an
	// underscore and one capital letter, and every variant of one paint shares the stem.
	bool SplitVariant(const FString& Name, FString& OutStem)
	{
		// <stem>_NN_X, and the same again with _Alt on the end.
		//
		// THE _Alt SUFFIX IS WHERE EVERY ALTERNATE PAINT LIVES, and this test used to reject the
		// lot of them: it demanded a capital letter as the very last character, and the packs put
		// their colourways in a Skin_Alts folder as M_Pack_01_B_Alt. So the search found only the
		// base material, one variant meant nothing to choose between, and the skin control hid
		// itself on every screen -- which read as the control never having been added at all.
		// SUFFIXES WE PUT ON OURSELVES COME OFF FIRST. A paint is identified by the pack's own
		// <stem>_NN_X naming, and anything appended after that hides it: "_Alt" is where the packs
		// keep their alternates, and "_TwoSided" is a child instance this project makes so a sight
		// can be solid from behind. Adding that suffix silently cost every optic its skin list --
		// the material stopped looking like a variant of anything, so the paint control found one
		// choice and hid itself.
		FString Base = Name;
		for (const TCHAR* Tail : { TEXT("_TwoSided"), TEXT("_Alt") })
		{
			const int32 Len = FCString::Strlen(Tail);
			if (Base.EndsWith(Tail, ESearchCase::IgnoreCase)) { Base.LeftChopInline(Len, EAllowShrinking::No); }
		}
		const int32 N = Base.Len();
		if (N < 6) { return false; }
		const TCHAR L = Base[N - 1];
		if (L < TEXT('A') || L > TEXT('Z') || Base[N - 2] != TEXT('_')) { return false; }
		if (!FChar::IsDigit(Base[N - 3]) || !FChar::IsDigit(Base[N - 4]) || Base[N - 5] != TEXT('_')) { return false; }
		OutStem = Base.Left(N - 5);
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

	// THE MATERIAL THAT NAMES THE FAMILY, looking through any instance wrapping it.
	//
	// A paint is identified by the pack's <stem>_NN_X naming, and that name belongs to the material
	// the pack shipped. Anything this project puts in front of it -- MI_..._TwoSided so a sight is
	// solid from behind, or any other instance -- has a name of its own that parses as nothing, and
	// the weapon it is on silently loses every colourway it had. Walking up to the parent asks the
	// only object that actually knows.
	//
	// It matters beyond the suffix: the two-sided instance is called MI_PolygonScifiWorlds_01_A,
	// while the mesh it came from wears M_PolygonSciFiWorlds_01_A. Those are different stems -- the
	// MI prefix alone splits the family in two -- so a rifle and the scope bolted to it would offer
	// different lists of paints despite being the same atlas.
	UMaterialInterface* VariantSource(UMaterialInterface* Mat, FString& OutName, FString& OutStem)
	{
		for (int32 Depth = 0; Mat && Depth < 8; ++Depth)
		{
			OutName = Mat->GetName();
			if (SplitVariant(OutName, OutStem)) { return Mat; }
			UMaterialInstance* Inst = Cast<UMaterialInstance>(Mat);
			Mat = Inst ? Inst->Parent : nullptr;
		}
		return nullptr;
	}

	UStaticMesh* MeshOf(const WeaponCatalog::FWeapon& W)
	{
		const FString& Path = W.BodyMeshPath.IsEmpty() ? W.MeshPath : W.BodyMeshPath;
		return Path.IsEmpty() ? nullptr : LoadObject<UStaticMesh>(nullptr, *Path, nullptr, LOAD_NoWarn | LOAD_Quiet);
	}
}

TArray<FString> WeaponSkins::VariantsOfMesh(UStaticMesh* Mesh)
{
	TArray<FString> Names;
	if (!Mesh) { return Names; }
	for (const FStaticMaterial& SM : Mesh->GetStaticMaterials())
	{
		if (!SM.MaterialInterface) { continue; }
		FString Stem, Named;
		UMaterialInterface* Src = VariantSource(SM.MaterialInterface, Named, Stem);
		if (!Src) { continue; }
		for (const TPair<FString, FSoftObjectPath>& V : VariantsOfStem(Stem, Src->GetPathName())) { Names.AddUnique(V.Key); }
		break;
	}
	Names.Sort();
	return Names;
}

FString WeaponSkins::CurrentOfMesh(UStaticMesh* Mesh, const FString& Chosen)
{
	const TArray<FString> V = VariantsOfMesh(Mesh);
	if (!Chosen.IsEmpty() && V.Contains(Chosen)) { return Chosen; }
	if (Mesh)
	{
		for (const FStaticMaterial& SM : Mesh->GetStaticMaterials())
		{
			FString Stem;
			FString Named;
			if (SM.MaterialInterface && VariantSource(SM.MaterialInterface, Named, Stem)) { return Named; }
		}
	}
	return FString();
}

void WeaponSkins::ApplyVariant(UStaticMeshComponent* Comp, const FString& Variant)
{
	static const WeaponCatalog::FWeapon Unused;
	ApplyNamed(Comp, Unused, Variant);   // ApplyNamed reads the component mesh, never the weapon
}

TArray<FString> WeaponSkins::Variants(const WeaponCatalog::FWeapon& W)
{
	TArray<FString> Names;
	UStaticMesh* Mesh = MeshOf(W);
	if (!Mesh) { return Names; }
	for (const FStaticMaterial& SM : Mesh->GetStaticMaterials())
	{
		if (!SM.MaterialInterface) { continue; }
		FString Stem, Named;
		UMaterialInterface* Src = VariantSource(SM.MaterialInterface, Named, Stem);
		if (!Src) { continue; }
		for (const TPair<FString, FSoftObjectPath>& V : VariantsOfStem(Stem, Src->GetPathName())) { Names.AddUnique(V.Key); }
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
			FString Named;
			if (SM.MaterialInterface && VariantSource(SM.MaterialInterface, Named, Stem)) { return Named; }
		}
	}
	return FString();
}

void WeaponSkins::Apply(UStaticMeshComponent* Comp, const WeaponCatalog::FWeapon& W)
{
	ApplyNamed(Comp, W, W.Skin);
}

void WeaponSkins::ApplyNamed(UStaticMeshComponent* Comp, const WeaponCatalog::FWeapon& W, const FString& Variant)
{
	if (!Comp || !Comp->GetStaticMesh()) { return; }
	UStaticMesh* Mesh = Comp->GetStaticMesh();
	FString SkinStem;
	const bool bChosen = !Variant.IsEmpty() && SplitVariant(Variant, SkinStem);
	for (int32 i = 0; i < Comp->GetNumMaterials(); ++i)
	{
		UMaterialInterface* Default = Mesh->GetMaterial(i);
		FString Stem, Named;
		if (!Default || !VariantSource(Default, Named, Stem)) { continue; }
		UMaterialInterface* Want = Default;
		if (bChosen && Stem == SkinStem)
		{
			if (const FSoftObjectPath* P = VariantsOfStem(Stem, Default->GetPathName()).Find(Variant))
			{
				if (UMaterialInterface* M = Cast<UMaterialInterface>(P->TryLoad())) { Want = M; }
			}
		}
		Comp->SetMaterial(i, Want);
	}
}
