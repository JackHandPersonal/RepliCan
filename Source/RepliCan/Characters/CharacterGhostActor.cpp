#include "Characters/CharacterGhostActor.h"
#include "Characters/CharacterConfig.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"

namespace
{
	// Translucent, unlit, two-sided; parameters GhostColor / Opacity.
	constexpr const TCHAR* GhostMaterialPath = TEXT("/Game/Characters/M_CharacterGhost.M_CharacterGhost");
	const FLinearColor ValidColor(0.25f, 0.9f, 0.5f, 1.0f);
	const FLinearColor InvalidColor(1.0f, 0.25f, 0.2f, 1.0f);
}

ACharacterGhostActor::ACharacterGhostActor()
{
	PrimaryActorTick.bCanEverTick = false;
	// Same mesh placement a character uses under its capsule (see
	// ABaseCharacter's constructor), so the ghost's feet sit where the real
	// character's will once the actor location is the capsule center.
	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;
}

USkeletalMeshComponent* ACharacterGhostActor::AddPreviewMesh(const FString& AssetPath)
{
	USkeletalMesh* Mesh = AssetPath.IsEmpty() ? nullptr : LoadObject<USkeletalMesh>(nullptr, *AssetPath);
	if (!Mesh) { return nullptr; }

	USkeletalMeshComponent* Comp = NewObject<USkeletalMeshComponent>(this);
	Comp->SetSkeletalMeshAsset(Mesh);
	Comp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Comp->SetCastShadow(false);
	Comp->SetAnimationMode(EAnimationMode::AnimationCustomMode);   // reference pose, no anim instance
	Comp->RegisterComponent();
	Comp->AttachToComponent(RootComponent, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	// Same offset ABaseCharacter gives its mesh: feet at the capsule bottom,
	// facing +X.
	Comp->SetRelativeLocation(FVector(0.0f, 0.0f, -90.0f));
	Comp->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));
	if (GhostMID)
	{
		for (int32 i = 0; i < Comp->GetNumMaterials(); ++i) { Comp->SetMaterial(i, GhostMID); }
	}
	PreviewMeshes.Add(Comp);
	return Comp;
}

void ACharacterGhostActor::BuildFromConfig(const FCharacterConfig& Config)
{
	for (USkeletalMeshComponent* Comp : PreviewMeshes)
	{
		if (Comp) { Comp->DestroyComponent(); }
	}
	PreviewMeshes.Reset();

	if (!GhostMID)
	{
		if (UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr, GhostMaterialPath))
		{
			GhostMID = UMaterialInstanceDynamic::Create(Material, this);
		}
	}

	if (Config.Type == CharacterType::Modular)
	{
		// Every part's reference pose is the same rig pose, so they line up
		// without a leader.
		for (const TPair<FString, FString>& Part : Config.Parts) { AddPreviewMesh(Part.Value); }
	}
	else
	{
		AddPreviewMesh(Config.BaseMesh);
	}
	SetActorScale3D(Config.Scale);
	SetValid(bCurrentValid);
}

void ACharacterGhostActor::SetValid(bool bValid)
{
	bCurrentValid = bValid;
	if (GhostMID)
	{
		GhostMID->SetVectorParameterValue(TEXT("GhostColor"), bValid ? ValidColor : InvalidColor);
	}
}
