#include "Weapons/ShotReactions.h"
#include "Characters/BaseCharacter.h"
#include "Weapons/ImpactEffects.h"
#include "Items/ItemCatalog.h"
#include "Characters/Vitality.h"
#include "Characters/DeathThrash.h"
#include "Weapons/SparkFx.h"
#include "Characters/Alertness.h"
#include "World/LootBoxActor.h"
#include "Core/BasePlayerController.h"
#include "Components/CapsuleComponent.h"
#include "Components/PoseableMeshComponent.h"
#include "Animation/AnimSequence.h"
#include "Animation/SkeletalMeshActor.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SkinnedMeshComponent.h"
#include "PhysicsEngine/BodyInstance.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/HitResult.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "PhysicsEngine/BodySetup.h"
#include "TimerManager.h"
#include "Components/DecalComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Components/SphereComponent.h"
#include "Engine/SkeletalMesh.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"

static AActor* GStrikeInstigator = nullptr;   // who is doing the hitting, for the hit thing's alertness
#include "PhysicsEngine/PhysicsAsset.h"

// RepliCan.Dismember 1: a head shot takes the head off (the part goes bouncing, the bone folds
// away, blood at the neck). 0 keeps heads on.
static TAutoConsoleVariable<int32> CVarDismember(TEXT("RepliCan.Dismember"), 1, TEXT("A destroyed head or limb comes off (1) or only folds away (0)"));
static FString GLastRegion;

FString ShotReactions::RegionOf(FName Bone)
{
	if (Bone.IsNone()) { return FString(); }
	const FString B = Bone.ToString().ToLower();
	if (B.StartsWith(TEXT("head"))) { return TEXT("Head"); }
	if (B.StartsWith(TEXT("neck"))) { return TEXT("Neck"); }
	const bool bLeft = B.EndsWith(TEXT("_l")) || B.Contains(TEXT("_l_"));
	if (B.StartsWith(TEXT("upperarm")) || B.StartsWith(TEXT("lowerarm")) || B.StartsWith(TEXT("hand")) || B.StartsWith(TEXT("thumb")) || B.StartsWith(TEXT("index")) || B.StartsWith(TEXT("middle")) || B.StartsWith(TEXT("ring")) || B.StartsWith(TEXT("pinky")))
	{
		return bLeft ? TEXT("ArmL") : TEXT("ArmR");
	}
	if (B.StartsWith(TEXT("thigh")) || B.StartsWith(TEXT("calf")) || B.StartsWith(TEXT("foot")) || B.StartsWith(TEXT("ball")))
	{
		return bLeft ? TEXT("LegL") : TEXT("LegR");
	}
	return TEXT("Torso");   // spine, pelvis, clavicles, root
}

const FString& ShotReactions::LastRegion() { return GLastRegion; }

namespace
{
	// A helmet, a nose, a badge: an attachment riding a person's bone (its own actor or not)
	// takes the hit for that bone. The hit is rewritten onto the skeletal mesh it hangs from, at
	// the bone its socket sits on, or the nearest bone to the impact when it is attached loosely.
	bool ResolveAttachment(FHitResult& Hit)
	{
		USceneComponent* Cur = Hit.GetComponent();
		if (!Cur || Cast<USkeletalMeshComponent>(Cur)) { return false; }
		for (int32 Hops = 0; Cur && Hops < 8; ++Hops)
		{
			USceneComponent* Parent = Cur->GetAttachParent();
			if (USkeletalMeshComponent* Skel = Cast<USkeletalMeshComponent>(Parent))
			{
				FName Bone = Skel->GetSocketBoneName(Cur->GetAttachSocketName());
				if (Bone.IsNone() || Skel->GetBoneIndex(Bone) == INDEX_NONE) { Bone = Skel->FindClosestBone(Hit.ImpactPoint); }
				Hit.HitObjectHandle = FActorInstanceHandle(Skel->GetOwner());
				Hit.Component = Skel;
				Hit.BoneName = Bone;
				return true;
			}
			Cur = Parent;
		}
		return false;
	}

	USkeletalMeshComponent* FirstSkelWithBone(AActor* Who, FName Bone)
	{
		TInlineComponentArray<USkeletalMeshComponent*> Skels(Who);
		for (USkeletalMeshComponent* S : Skels) { if (S && S->GetSkeletalMeshAsset() && S->GetBoneIndex(Bone) != INDEX_NONE) { return S; } }
		return nullptr;
	}

	// Which piece of gear stands in front of a hit: the head for the head and neck, the chest for
	// the trunk, gloves for a hand and sleeves above it, boots for a foot and legwear above.
	FString BodySlotFor(const FString& Region, FName Bone)
	{
		const FString B = Bone.ToString().ToLower();
		if (Region == TEXT("Head") || Region == TEXT("Neck")) { return TEXT("Head"); }
		if (Region == TEXT("ArmL") || Region == TEXT("ArmR")) { return B.StartsWith(TEXT("hand")) || B.StartsWith(TEXT("index")) || B.StartsWith(TEXT("middle")) || B.StartsWith(TEXT("ring")) || B.StartsWith(TEXT("pinky")) || B.StartsWith(TEXT("thumb")) ? TEXT("Hands") : TEXT("Arms"); }
		if (Region == TEXT("LegL") || Region == TEXT("LegR")) { return B.StartsWith(TEXT("foot")) || B.StartsWith(TEXT("ball")) ? TEXT("Feet") : TEXT("Legs"); }
		return TEXT("Chest");
	}

	// The armour in the way: a character's worn gear at that slot (its catalogue armor_value);
	// an extra wears nothing the score can see yet.
	float ArmourFor(AActor* Who, const FString& Region, FName Bone)
	{
		const ABaseCharacter* Ch = Cast<ABaseCharacter>(Who);
		const ABasePlayerController* PC = Ch ? Cast<ABasePlayerController>(Ch->GetController()) : nullptr;
		return PC ? PC->ArmourValueFor(BodySlotFor(Region, Bone)) : 0.0f;
	}

	// THE STUMP: torn flesh round the cut -- overlapping lumps squashed against it and a couple of
	// strands hanging off -- and a short bone standing out of the meat, its end broken into
	// splinters with the marrow showing, rather than capped. On the body it hangs from the bone
	// above the joint, pointing on past it; on the dropped piece from the kept bone, back out of
	// the cut end. Base is the joint in the socket's frame, Dir the way out, both in that frame.
	void BoneStub(USceneComponent* On, FName Socket, const FVector& Base, const FVector& Dir, UObject* Outer, const TCHAR* Name, bool bArm)
	{
		static UStaticMesh* Cyl = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
		static UStaticMesh* Ball = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
		static UMaterialInterface* BoneMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/RepliCan/Materials/M_Bone.M_Bone"), nullptr, LOAD_NoWarn | LOAD_Quiet);
		static UMaterialInterface* MeatMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/RepliCan/Materials/M_Meat.M_Meat"), nullptr, LOAD_NoWarn | LOAD_Quiet);
		if (!On || !Cyl || !Ball || !Outer) { return; }
		// A frame on the stump: Dir out of it, U and V across it.
		const FVector U = FVector::CrossProduct(Dir, FMath::Abs(Dir.Z) < 0.9f ? FVector::UpVector : FVector::RightVector).GetSafeNormal();
		const FVector V = FVector::CrossProduct(Dir, U);
		int32 N = 0;
		auto Make = [&](UStaticMesh* Mesh, const FVector& At, const FQuat& Rot, const FVector& Scale, UMaterialInterface* Mat)
		{
			UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(Outer, *FString::Printf(TEXT("%s_%d"), Name, N++));
			C->SetStaticMesh(Mesh);
			if (Mat) { C->SetMaterial(0, Mat); }
			C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			C->SetCastShadow(false);
			C->AttachToComponent(On, FAttachmentTransformRules::KeepRelativeTransform, Socket);
			C->SetRelativeTransform(FTransform(Rot, At, Scale));
			C->RegisterComponent();
		};
		const FQuat Along = FRotationMatrix::MakeFromZ(Dir).ToQuat();
		const float Thick = bArm ? 0.018f : 0.024f;   // the engine cylinder is 100 cm across: a 1.8 cm arm bone, a 2.4 cm leg bone
		// The meat: lumps round the base, squashed against the cut.
		for (int32 i = 0; i < 4; ++i)
		{
			const float A = i * 1.7f + 0.4f;
			const FVector At = Base + Dir * FMath::FRandRange(0.2f, 1.4f) + (U * FMath::Cos(A) + V * FMath::Sin(A)) * FMath::FRandRange(0.8f, bArm ? 2.6f : 3.4f);
			const float R = FMath::FRandRange(bArm ? 0.028f : 0.036f, bArm ? 0.045f : 0.058f);
			Make(Ball, At, Along, FVector(R, R, R * 0.55f), MeatMat);
		}
		// Two strands hanging off it.
		for (int32 i = 0; i < 2; ++i)
		{
			const float A = i * 2.9f + 1.1f;
			const FVector Side = (U * FMath::Cos(A) + V * FMath::Sin(A)).GetSafeNormal();
			const FVector StrandDir = (Dir * 0.6f + Side * 0.8f).GetSafeNormal();
			Make(Cyl, Base + Side * 1.5f + StrandDir * 2.0f, FRotationMatrix::MakeFromZ(StrandDir).ToQuat(), FVector(0.008f, 0.008f, 0.045f), MeatMat);
		}
		// The bone: 5 cm, from inside the meat, and the break: three splinters off its end, marrow in the middle.
		Make(Cyl, Base + Dir * 2.6f, Along, FVector(Thick, Thick, 0.05f), BoneMat);
		for (int32 i = 0; i < 3; ++i)
		{
			const float A = i * 2.1f + 0.3f;
			const FVector Side = (U * FMath::Cos(A) + V * FMath::Sin(A)).GetSafeNormal();
			const FVector ShardDir = (Dir + Side * FMath::FRandRange(0.35f, 0.7f)).GetSafeNormal();
			const float Len = FMath::FRandRange(0.012f, 0.022f);
			Make(Cyl, Base + Dir * 4.9f + Side * (Thick * 35.0f) + ShardDir * (Len * 50.0f), FRotationMatrix::MakeFromZ(ShardDir).ToQuat(), FVector(Thick * 0.35f, Thick * 0.35f, Len), BoneMat);
		}
		Make(Ball, Base + Dir * 4.9f, Along, FVector(Thick * 0.7f), MeatMat);
	}

	// THE NECK: the head gone, what is left standing out of it -- the windpipe and a pair of
	// vessels as short red tubes, torn flesh round them, and the spine's end as a bone nub at the
	// back. Base is the head bone's origin in the neck bone's frame, Dir the way the head went.
	void NeckStub(USceneComponent* On, FName Socket, const FVector& Base, const FVector& Dir, UObject* Outer)
	{
		static UStaticMesh* Cyl = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
		static UStaticMesh* Ball = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
		static UMaterialInterface* BoneMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/RepliCan/Materials/M_Bone.M_Bone"), nullptr, LOAD_NoWarn | LOAD_Quiet);
		static UMaterialInterface* MeatMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/RepliCan/Materials/M_Meat.M_Meat"), nullptr, LOAD_NoWarn | LOAD_Quiet);
		if (!On || !Cyl || !Ball || !Outer) { return; }
		const FVector U = FVector::CrossProduct(Dir, FMath::Abs(Dir.Z) < 0.9f ? FVector::UpVector : FVector::RightVector).GetSafeNormal();
		const FVector V = FVector::CrossProduct(Dir, U);
		int32 N = 0;
		auto Make = [&](UStaticMesh* Mesh, const FVector& At, const FQuat& Rot, const FVector& Scale, UMaterialInterface* Mat)
		{
			UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(Outer, *FString::Printf(TEXT("NeckStump_%d"), N++));
			C->SetStaticMesh(Mesh);
			if (Mat) { C->SetMaterial(0, Mat); }
			C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			C->SetCastShadow(false);
			C->AttachToComponent(On, FAttachmentTransformRules::KeepRelativeTransform, Socket);
			C->SetRelativeTransform(FTransform(Rot, At, Scale));
			C->RegisterComponent();
		};
		const FQuat Along = FRotationMatrix::MakeFromZ(Dir).ToQuat();
		// A tumbled, random orientation. Five spheres on an even ring, all facing the same way and
		// all the same size, read as five spheres -- the eye finds the pattern before it finds the
		// wound. What stops a lump looking like a ball is being squashed on three different axes and
		// turned to an angle that shares nothing with its neighbours.
		auto Tumbled = [&]() { return FRotator(FMath::FRandRange(-180.f, 180.f), FMath::FRandRange(-180.f, 180.f), FMath::FRandRange(-180.f, 180.f)).Quaternion(); };

		// TORN FLESH ROUND THE CUT. Many small pieces at unrelated angles and unrelated sizes, some
		// sunk into the stump and some standing proud of it, so the rim is broken rather than round.
		const int32 Lumps = FMath::RandRange(13, 19);
		for (int32 i = 0; i < Lumps; ++i)
		{
			const float A = FMath::FRandRange(0.0f, 2.0f * PI);
			const float Out = FMath::FRandRange(0.6f, 4.4f);
			const FVector At = Base + Dir * FMath::FRandRange(-1.1f, 1.4f) + (U * FMath::Cos(A) + V * FMath::Sin(A)) * Out;
			const float R = FMath::FRandRange(0.012f, 0.042f);
			Make(Ball, At, Tumbled(), FVector(R * FMath::FRandRange(0.5f, 1.6f), R * FMath::FRandRange(0.5f, 1.6f), R * FMath::FRandRange(0.3f, 1.2f)), MeatMat);
		}
		// AND THE RAGGED ENDS -- strands of it hanging off the rim, stretched thin and pointing
		// wherever they were torn. These are what sell it as torn rather than cut.
		const int32 Strands = FMath::RandRange(4, 7);
		for (int32 i = 0; i < Strands; ++i)
		{
			const float A = FMath::FRandRange(0.0f, 2.0f * PI);
			const FVector Outward = (U * FMath::Cos(A) + V * FMath::Sin(A));
			// Mostly downward and outward: gravity has had them since the head left.
			const FVector Hang = (Outward * FMath::FRandRange(0.4f, 1.0f) + Dir * FMath::FRandRange(-0.9f, 0.4f)).GetSafeNormal();
			const float Len = FMath::FRandRange(0.012f, 0.035f);
			const float Thin = FMath::FRandRange(0.004f, 0.009f);
			Make(Cyl, Base + Outward * FMath::FRandRange(1.8f, 3.6f) + Hang * (Len * 45.0f),
				FRotationMatrix::MakeFromZ(Hang).ToQuat(), FVector(Thin, Thin * FMath::FRandRange(0.6f, 1.5f), Len), MeatMat);
		}
		// The windpipe, a little forward, and two vessels either side of it, leaning out. Jittered:
		// perfectly placed anatomy under torn flesh looks like a diagram.
		const FVector Jit = (U * FMath::FRandRange(-0.5f, 0.5f) + V * FMath::FRandRange(-0.5f, 0.5f));
		Make(Cyl, Base + Dir * 1.6f + U * 1.5f + Jit, FRotator(FMath::FRandRange(-12.f, 12.f), 0.0f, FMath::FRandRange(-12.f, 12.f)).Quaternion() * Along, FVector(0.02f, 0.023f, 0.035f), MeatMat);
		Make(Ball, Base + Dir * 3.3f + U * 1.5f, Along, FVector(0.016f, 0.016f, 0.006f), BoneMat);   // the pale ring of cartilage at its end
		for (int32 i = 0; i < 2; ++i)
		{
			const FVector Side = (i == 0 ? V : -V);
			const FVector TubeDir = (Dir + Side * 0.35f).GetSafeNormal();
			Make(Cyl, Base + Side * 2.2f + U * 0.5f + TubeDir * 1.4f, FRotationMatrix::MakeFromZ(TubeDir).ToQuat(), FVector(0.009f, 0.009f, 0.028f), MeatMat);
		}
		// The spine's end, at the back.
		Make(Cyl, Base + Dir * 0.9f - U * 2.0f, Along, FVector(0.016f, 0.016f, 0.02f), BoneMat);
		Make(Ball, Base + Dir * 1.9f - U * 2.0f, Along, FVector(0.011f), MeatMat);
	}

	// A flinch clip for the side the shot came in on, light or medium by where it landed. The
	// Lyra set: Front has four light takes and two medium; the other sides one of each.
	UAnimSequence* FlinchFor(const AActor* Who, const FVector& ShotDir, bool bHeavy)
	{
		const float Along = FVector::DotProduct(ShotDir, Who->GetActorForwardVector());
		const float Across = FVector::DotProduct(ShotDir, Who->GetActorRightVector());
		FString Side;
		if (Along > 0.5f) { Side = TEXT("Back"); }          // travelling the way they face: it came from behind
		else if (Along < -0.5f) { Side = TEXT("Front"); }
		else { Side = Across > 0.0f ? TEXT("Left") : TEXT("Right"); }   // moving toward their right: it struck the left side
		const int32 Take = bHeavy ? (Side == TEXT("Front") ? FMath::RandRange(1, 2) : 1) : (Side == TEXT("Front") ? FMath::RandRange(1, 4) : 1);
		const FString Path = FString::Printf(TEXT("/Game/Characters/Animations/Lyra/HitReactions/MM_HitReact_%s_%s_%02d"), *Side, bHeavy ? TEXT("Med") : TEXT("Lgt"), Take);
		return LoadObject<UAnimSequence>(nullptr, *Path, nullptr, LOAD_NoWarn | LOAD_Quiet);
	}

	// The mark: a blood decal on the bone that was struck, so it rides the limb. It smears a
	// little where a joint bends, which is the price of not painting the skin itself.
	void MarkWound(UWorld* World, const FHitResult& Hit, const FVector& ShotDir)
	{
		static UMaterialInterface* Mat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/RepliCan/Materials/M_BloodDecal.M_BloodDecal"), nullptr, LOAD_NoWarn | LOAD_Quiet);
		USceneComponent* On = Hit.GetComponent();
		if (!Mat || !On) { return; }
		FRotator Rot = (-Hit.ImpactNormal).Rotation(); Rot.Roll = FMath::FRandRange(0.0f, 360.0f);
		const float S = FMath::FRandRange(9.0f, 14.0f);
		if (UDecalComponent* D = UGameplayStatics::SpawnDecalAttached(Mat, FVector(6.0f, S, S), On, Hit.BoneName, Hit.ImpactPoint, Rot, EAttachLocation::KeepWorldPosition, 120.0f)) { D->SetFadeScreenSize(0.0004f); }
	}

	// Hides everything that hangs off a bone (a nose prop, hair, a helmet on a socket) along with the bone.
	void HideBoneAndHangers(AActor* Who, FName Bone)
	{
		// The bone and everything below it fold away on every mesh; the physics bodies below it go
		// too, so a ragdoll does not keep an invisible limb to catch on things.
		TInlineComponentArray<USkeletalMeshComponent*> Skels(Who);
		USkeletalMeshComponent* Ref = nullptr;
		for (USkeletalMeshComponent* S : Skels) { if (S && S->GetBoneIndex(Bone) != INDEX_NONE) { S->HideBoneByName(Bone, EPhysBodyOp::PBO_Term); if (!Ref) { Ref = S; } } }
		// Whatever hangs on that bone or any bone below it (a nose, hair, a helmet, the weapon in the hand) goes with it.
		TInlineComponentArray<USceneComponent*> All(Who);
		for (USceneComponent* C : All)
		{
			const FName At = C ? C->GetAttachSocketName() : NAME_None;
			if (At.IsNone()) { continue; }
			const FName AtBone = Ref ? Ref->GetSocketBoneName(At) : At;
			if (AtBone == Bone || (Ref && Ref->GetBoneIndex(AtBone) != INDEX_NONE && Ref->BoneIsChildOf(AtBone, Bone))) { C->SetVisibility(false, true); }
		}
		// And whole actors hung there (a pilot's helmet is its own actor on the head).
		TArray<AActor*> Hung; Who->GetAttachedActors(Hung, true, false);
		for (AActor* A : Hung)
		{
			USceneComponent* R = A ? A->GetRootComponent() : nullptr;
			const FName At = R ? R->GetAttachSocketName() : NAME_None;
			if (At.IsNone()) { continue; }
			const FName AtBone = Ref ? Ref->GetSocketBoneName(At) : At;
			if (AtBone == Bone || (Ref && Ref->GetBoneIndex(AtBone) != INDEX_NONE && Ref->BoneIsChildOf(AtBone, Bone))) { A->SetActorHiddenInGame(true); A->SetActorEnableCollision(false); }
		}
	}

	// The head comes off: the character's head piece (a modular part, or an extra's separate head
	// mesh) is spawned as its own thing on a small physics ball at the head's transform, the bone
	// and everything hung on it folded away, blood at the neck.
	bool SeverHead(UWorld* World, AActor* Who, USkeletalMeshComponent* Body, const FVector& ShotDir)
	{
		const FName HeadBone(TEXT("head"));
		if (!Body || Body->GetBoneIndex(HeadBone) == INDEX_NONE) { Body = FirstSkelWithBone(Who, HeadBone); }
		if (!Body || Who->ActorHasTag(TEXT("headless"))) { return false; }
		const FTransform HeadT = Body->GetSocketTransform(HeadBone, RTS_World);
		// The head's own mesh: the modular Head part on a built character, or an extra's head
		// component; a one-piece body has no separate head, so it just folds away.
		USkeletalMeshComponent* HeadPart = nullptr;
		TInlineComponentArray<USkeletalMeshComponent*> Skels(Who);
		for (USkeletalMeshComponent* S : Skels)
		{
			if (!S || S == Body || !S->GetSkeletalMeshAsset()) { continue; }
			const FString N = S->GetName();
			if (N.Contains(TEXT("Head"), ESearchCase::IgnoreCase) || S->GetSkeletalMeshAsset()->GetName().Contains(TEXT("Head"), ESearchCase::IgnoreCase)) { HeadPart = S; break; }
		}
		if (HeadPart)
		{
			FActorSpawnParameters P; P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			AActor* Chunk = World->SpawnActor<AActor>(AActor::StaticClass(), HeadT, P);
			if (Chunk)
			{
				USphereComponent* Ball = NewObject<USphereComponent>(Chunk, TEXT("Ball"));
				Ball->InitSphereRadius(11.0f);
				Ball->SetCollisionProfileName(TEXT("PhysicsActor"));
				Ball->SetWorldTransform(HeadT);
				Chunk->SetRootComponent(Ball);
				Ball->RegisterComponent();
				Ball->SetSimulatePhysics(true);
				Ball->SetMassOverrideInKg(NAME_None, 4.5f, true);
				USkeletalMeshComponent* M = NewObject<USkeletalMeshComponent>(Chunk, TEXT("Head"));
				M->SetSkeletalMeshAsset(HeadPart->GetSkeletalMeshAsset());
				for (int32 i = 0; i < HeadPart->GetNumMaterials(); ++i) { M->SetMaterial(i, HeadPart->GetMaterial(i)); }
				M->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				M->AttachToComponent(Ball, FAttachmentTransformRules::KeepRelativeTransform);
				M->RegisterComponent();
				// The piece's vertices sit at the skeleton's head, not at the component origin: pull them onto the ball.
				const FTransform HeadRef = M->GetSocketTransform(HeadBone, RTS_Component);
				M->SetRelativeTransform(HeadRef.Inverse());
				Ball->AddImpulse((ShotDir * 260.0f + FVector(0, 0, 160.0f)) * 4.5f, NAME_None, false);
				Ball->AddAngularImpulseInDegrees(FVector(FMath::FRandRange(-800.f, 800.f), FMath::FRandRange(-800.f, 800.f), FMath::FRandRange(-800.f, 800.f)), NAME_None, false);
				Chunk->SetLifeSpan(90.0f);
			}
			HeadPart->SetVisibility(false, true);
		}
		const FTransform NeckT = Body->GetBoneIndex(TEXT("neck_01")) != INDEX_NONE ? Body->GetSocketTransform(TEXT("neck_01"), RTS_World) : HeadT;
		HideBoneAndHangers(Who, HeadBone);
		if (Body->GetBoneIndex(TEXT("neck_01")) != INDEX_NONE)
		{
			const FVector HeadInNeck = NeckT.InverseTransformPosition(HeadT.GetLocation());
			NeckStub(Body, TEXT("neck_01"), HeadInNeck, HeadInNeck.GetSafeNormal(), Who);
		}
		ImpactEffects::SpawnBurst(World, { TEXT("/Game/Synty/PolygonGeneric/FX/NS_Blood_Splatter_01"), 0.9f, 0.9f }, NeckT.GetLocation(), FRotator(80.0f, 0.0f, 0.0f));
		ShotReactions::PoolAt(World, NeckT.GetLocation(), 55.0f);
		Who->Tags.AddUnique(TEXT("headless"));
		return true;
	}

	// The mesh that carries a limb, to drop it as its own thing: the modular Arms or Legs part on
	// a built character, else the cut library's piece for a one-piece body (the extras' bodies were
	// all cut). MaterialSource is whose paint the piece takes.
	USkeletalMesh* LimbPieceFor(AActor* Who, USkeletalMeshComponent* Body, bool bArm, USkeletalMeshComponent*& MaterialSource)
	{
		const TCHAR* Word = bArm ? TEXT("Arms") : TEXT("Legs");
		TInlineComponentArray<USkeletalMeshComponent*> Skels(Who);
		for (USkeletalMeshComponent* S : Skels)
		{
			if (!S || !S->GetSkeletalMeshAsset()) { continue; }
			if (S->GetName().Contains(Word) || S->GetSkeletalMeshAsset()->GetName().EndsWith(Word)) { MaterialSource = S; return S->GetSkeletalMeshAsset(); }
		}
		if (!Body || !Body->GetSkeletalMeshAsset()) { return nullptr; }
		FString Name = Body->GetSkeletalMeshAsset()->GetName();
		Name.RemoveFromStart(TEXT("SK_Chr_")); Name.RemoveFromStart(TEXT("SK_"));
		static const TCHAR* Packs[] = { TEXT("SciFiSpace"), TEXT("SciFiWorlds"), TEXT("SciFiCity"), TEXT("CyberCity"), TEXT("Police") };
		for (const TCHAR* Pack : Packs)
		{
			const FString Path = FString::Printf(TEXT("/Game/RepliCan/CutLibrary/%s/%s_%s.%s_%s"), Pack, *Name, Word, *Name, Word);
			if (USkeletalMesh* Piece = LoadObject<USkeletalMesh>(nullptr, *Path, nullptr, LOAD_NoWarn | LOAD_Quiet)) { MaterialSource = Body; return Piece; }
		}
		return nullptr;
	}

	// A limb comes off at the joint: an arm at the elbow, a leg at the knee. Below the joint folds
	// away on every mesh of the body (its physics bodies with it), blood at the joint, and the
	// piece goes down on a physics capsule laid along it: the Arms or Legs mesh the body uses,
	// posed with the segment above the joint shrunk to nothing and the other side folded away, so
	// only the forearm and hand, or the shin and foot, show. A body with no piece just loses the limb.
	bool SeverLimb(UWorld* World, AActor* Who, USkeletalMeshComponent* Body, const FString& Region, const FVector& ShotDir)
	{
		const bool bArm = Region.StartsWith(TEXT("Arm")); const bool bLeft = Region.EndsWith(TEXT("L"));
		const TCHAR* Side = bLeft ? TEXT("l") : TEXT("r");
		const FName Upper(*FString::Printf(TEXT("%s_%s"), bArm ? TEXT("upperarm") : TEXT("thigh"), Side));
		const FName Root(*FString::Printf(TEXT("%s_%s"), bArm ? TEXT("lowerarm") : TEXT("calf"), Side));
		const FName End(*FString::Printf(TEXT("%s_%s"), bArm ? TEXT("hand") : TEXT("foot"), Side));
		const FName Tip(*FString::Printf(TEXT("%s_%s"), bArm ? TEXT("middle_01") : TEXT("ball"), Side));   // where the piece really ends, when the rig has it
		const FName OtherRoot(*FString::Printf(TEXT("%s_%s"), bArm ? TEXT("clavicle") : TEXT("thigh"), bLeft ? TEXT("r") : TEXT("l")));
		if (!Body || Body->GetBoneIndex(Root) == INDEX_NONE) { Body = FirstSkelWithBone(Who, Root); }
		if (!Body) { return false; }
		const FName Tag(*FString::Printf(TEXT("severed_%s"), *Region));
		if (Who->ActorHasTag(Tag)) { return false; }
		const FName Far = Body->GetBoneIndex(Tip) != INDEX_NONE ? Tip : End;
		const FVector RootPos = Body->GetSocketLocation(Root), EndPos = Body->GetSocketLocation(Far);
		// The joint in the upper bone's frame, before the bone below it folds away: where the stump's bone goes.
		const FTransform UpperW = Body->GetSocketTransform(Upper, RTS_World);
		const FVector JointInUpper = UpperW.InverseTransformPosition(RootPos);
		const FVector Dir = (EndPos - RootPos).GetSafeNormal();
		const float Len = FMath::Max(15.0f, (float)FVector::Dist(RootPos, EndPos));
		USkeletalMeshComponent* Paint = nullptr;
		if (USkeletalMesh* PieceMesh = LimbPieceFor(Who, Body, bArm, Paint))
		{
			FActorSpawnParameters P; P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			const FTransform CapT(FRotationMatrix::MakeFromZ(Dir).ToQuat(), RootPos + Dir * (Len * 0.5f));
			if (AActor* Chunk = World->SpawnActor<AActor>(AActor::StaticClass(), CapT, P))
			{
				const float R = bArm ? 5.0f : 7.0f;
				UCapsuleComponent* Cap = NewObject<UCapsuleComponent>(Chunk, TEXT("Limb"));
				Cap->InitCapsuleSize(R, Len * 0.5f + R);
				Cap->SetCollisionProfileName(TEXT("PhysicsActor"));
				Cap->SetWorldTransform(CapT);
				Chunk->SetRootComponent(Cap);
				Cap->RegisterComponent();
				Cap->SetSimulatePhysics(true);
				Cap->SetMassOverrideInKg(NAME_None, bArm ? 2.0f : 5.0f, true);
				// A poseable copy of the piece, so single bones can be shrunk without taking their children with them.
				UPoseableMeshComponent* M = NewObject<UPoseableMeshComponent>(Chunk, TEXT("Piece"));
				M->SetSkinnedAssetAndUpdate(PieceMesh, true);
				if (Paint) { for (int32 i = 0; i < Paint->GetNumMaterials() && i < M->GetNumMaterials(); ++i) { if (Paint->GetMaterial(i)) { M->SetMaterial(i, Paint->GetMaterial(i)); } } }
				M->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				M->AttachToComponent(Cap, FAttachmentTransformRules::KeepRelativeTransform);
				M->RegisterComponent();
				M->RefreshBoneTransforms();
				if (M->GetBoneIndex(OtherRoot) != INDEX_NONE) { M->HideBoneByName(OtherRoot, EPhysBodyOp::PBO_None); }   // the piece carries both sides
				// The piece rests in its reference pose. The segment above the joint is shrunk to nothing
				// (its skin collapses to a point) and the kept segment put back exactly where the
				// reference pose had it, so what hangs below it is untouched.
				const FTransform KeepRef = M->GetBoneTransformByName(Root, EBoneSpaces::ComponentSpace);
				const FName PieceFar = M->GetBoneIndex(Far) != INDEX_NONE ? Far : End;
				const FVector RefEndPos = M->GetBoneTransformByName(PieceFar, EBoneSpaces::ComponentSpace).GetLocation();
				if (M->GetBoneIndex(Upper) != INDEX_NONE)
				{
					FTransform Shrunk = M->GetBoneTransformByName(Upper, EBoneSpaces::ComponentSpace);
					Shrunk.SetScale3D(FVector(0.001f));
					M->SetBoneTransformByName(Upper, Shrunk, EBoneSpaces::ComponentSpace);
					M->SetBoneTransformByName(Root, KeepRef, EBoneSpaces::ComponentSpace);
					M->RefreshBoneTransforms();
				}
				const FVector RefRootPos = KeepRef.GetLocation();
				const FQuat Turn = FQuat::FindBetweenNormals((RefEndPos - RefRootPos).GetSafeNormal(), FVector::UpVector);
				M->SetRelativeTransform(FTransform(Turn, FVector(0.0f, 0.0f, -Len * 0.5f) - Turn.RotateVector(RefRootPos)));
				// The bone out of the piece's cut end: from the kept bone, back the way the limb came.
				BoneStub(M, Root, FVector::ZeroVector, KeepRef.InverseTransformVectorNoScale((RefRootPos - RefEndPos).GetSafeNormal()), Chunk, TEXT("PieceBone"), bArm);
				Cap->AddImpulse(ShotDir * 220.0f + FVector(0.0f, 0.0f, 140.0f), NAME_None, true);
				Cap->AddAngularImpulseInDegrees(FVector(FMath::FRandRange(-600.f, 600.f), FMath::FRandRange(-600.f, 600.f), FMath::FRandRange(-600.f, 600.f)), NAME_None, true);
				Chunk->SetLifeSpan(90.0f);
			}
		}
		HideBoneAndHangers(Who, Root);
		if (Body->GetBoneIndex(Upper) != INDEX_NONE) { BoneStub(Body, Upper, JointInUpper, JointInUpper.GetSafeNormal(), Who, *FString::Printf(TEXT("StumpBone_%s"), *Region), bArm); }
		ImpactEffects::SpawnBurst(World, { TEXT("/Game/Synty/PolygonGeneric/FX/NS_Blood_Splatter_01"), 0.8f, 0.8f }, RootPos, (-ShotDir).Rotation());
		ShotReactions::PoolAt(World, RootPos, 45.0f);
		Who->Tags.AddUnique(Tag);
		return true;
	}

	bool SeverRegion(UWorld* World, AActor* Who, USkeletalMeshComponent* Body, const FString& Region, const FVector& ShotDir)
	{
		if (Region == TEXT("Head")) { return SeverHead(World, Who, Body, ShotDir); }
		if (UVitalityComponent::IsLimb(Region)) { return SeverLimb(World, Who, Body, Region, ShotDir); }
		return false;
	}

	// WHAT THE DEAD CARRY. A body becomes a container the reticle can Loot: a hidden loot box actor
	// rides its pelvis (the transfer screen already knows how to open one of those). Its contents
	// are "loot:" tags on the actor when the layout gave it any, else a few things from the pockets,
	// drawn from the catalogue by kind. The menu loses Talk and gains Loot; the description says.
	void CorpseLoot(UWorld* World, AActor* Who, USkeletalMeshComponent* Body)
	{
		if (!World || !Who || Who->ActorHasTag(TEXT("looted_body"))) { return; }
		TArray<FString> Items;
		FString Name;
		for (int32 i = Who->Tags.Num() - 1; i >= 0; --i)
		{
			const FString T = Who->Tags[i].ToString();
			if (T.StartsWith(TEXT("loot:"))) { Items.Add(T.Mid(5)); }
			else if (T.StartsWith(TEXT("name:"))) { Name = T.Mid(5); }
			else if (T.StartsWith(TEXT("desc:")) || T == TEXT("action:Talk")) { Who->Tags.RemoveAt(i); }
		}
		if (Items.Num() == 0)
		{
			static const TCHAR* Pockets[] = { TEXT("Cigarette"), TEXT("Pills"), TEXT("Food"), TEXT("Drink"), TEXT("Note"), TEXT("Keycard"), TEXT("Badge"), TEXT("Photo"), TEXT("Consumable") };
			const int32 Count = FMath::RandRange(1, 3);
			for (int32 n = 0; n < Count; ++n)
			{
				const FString Kind = Pockets[FMath::RandRange(0, UE_ARRAY_COUNT(Pockets) - 1)];
				TArray<FString> Of;
				for (const ItemCatalog::FRecord& R : ItemCatalog::Records()) { if (R.Get(TEXT("kind")) == Kind && !R.Name.IsEmpty() && R.Get(TEXT("hidden")) != TEXT("true")) { Of.Add(R.Name); } }
				if (Of.Num() > 0) { Items.AddUnique(Of[FMath::RandRange(0, Of.Num() - 1)]); }
			}
		}
		FActorSpawnParameters P; P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		const FVector At = (Body && Body->GetBoneIndex(TEXT("pelvis")) != INDEX_NONE) ? Body->GetSocketLocation(TEXT("pelvis")) : Who->GetActorLocation();
		ALootBoxActor* Box = World->SpawnActor<ALootBoxActor>(ALootBoxActor::StaticClass(), At, FRotator::ZeroRotator, P);
		if (!Box) { return; }
		Box->Items = Items;
		Box->DisplayName = Name.IsEmpty() ? FString(TEXT("Body")) : Name;
		Box->Description = TEXT("Dead. Whatever they carried is still on them.");
		Box->SetActorHiddenInGame(true);
		Box->SetActorEnableCollision(false);
		if (Body && Body->GetBoneIndex(TEXT("pelvis")) != INDEX_NONE) { Box->AttachToComponent(Body, FAttachmentTransformRules::KeepWorldTransform, TEXT("pelvis")); }
		else { Box->AttachToActor(Who, FAttachmentTransformRules::KeepWorldTransform); }
		Box->Tags.Add(TEXT("corpse_loot"));
		Who->Tags.AddUnique(TEXT("inspectable"));
		Who->Tags.AddUnique(TEXT("action:Loot"));
		Who->Tags.AddUnique(*FString::Printf(TEXT("desc:%s. Dead. Whatever they carried is still on them."), Name.IsEmpty() ? TEXT("A body") : *Name));
		Who->Tags.AddUnique(TEXT("looted_body"));
	}

	// THE PILE. A corpse that has taken more than a few pools' worth since it died is no longer a
	// body: what was there is hidden (the mesh, the head actor, whatever rode it), its loot box
	// goes with it, and a heap of meat and bone is left on the floor where it lay, in a pool. The
	// heap is inspectable but there is nothing to loot. It stays, like the corpses do.
	void PulpBody(UWorld* World, AActor* Who, USkeletalMeshComponent* Body)
	{
		if (!World || !Who || Who->ActorHasTag(TEXT("pulped"))) { return; }
		static UStaticMesh* Ball = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
		static UStaticMesh* Cyl = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
		static UMaterialInterface* MeatMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/RepliCan/Materials/M_Meat.M_Meat"), nullptr, LOAD_NoWarn | LOAD_Quiet);
		static UMaterialInterface* BoneMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/RepliCan/Materials/M_Bone.M_Bone"), nullptr, LOAD_NoWarn | LOAD_Quiet);
		FString Label;
		for (const FName& T : Who->Tags) { const FString S = T.ToString(); if (S.StartsWith(TEXT("name:"))) { Label = S.Mid(5); } }
		const FVector Centre = (Body && Body->GetBoneIndex(TEXT("pelvis")) != INDEX_NONE) ? Body->GetSocketLocation(TEXT("pelvis")) : Who->GetActorLocation();
		FVector Floor = Centre;
		{
			FHitResult Hit; FCollisionQueryParams Q(SCENE_QUERY_STAT(Pulp), false); Q.AddIgnoredActor(Who);
			if (World->LineTraceSingleByChannel(Hit, Centre + FVector(0, 0, 40.0f), Centre - FVector(0, 0, 250.0f), ECC_Visibility, Q) && !Cast<USkinnedMeshComponent>(Hit.GetComponent())) { Floor = Hit.ImpactPoint; }
		}
		FActorSpawnParameters P; P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AActor* Pile = World->SpawnActor<AActor>(AActor::StaticClass(), Floor, FRotator(0.0f, FMath::FRandRange(0.0f, 360.0f), 0.0f), P);
		if (!Pile) { return; }
		USceneComponent* Root = NewObject<USceneComponent>(Pile, TEXT("Root"));
		Root->SetWorldLocation(Floor);
		Pile->SetRootComponent(Root);
		Root->RegisterComponent();
		int32 N = 0;
		auto Make = [&](UStaticMesh* Mesh, const FVector& At, const FRotator& Rot, const FVector& Scale, UMaterialInterface* Mat)
		{
			UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(Pile, *FString::Printf(TEXT("Pile_%d"), N++));
			C->SetStaticMesh(Mesh);
			if (Mat) { C->SetMaterial(0, Mat); }
			C->SetCollisionEnabled(ECollisionEnabled::QueryOnly);   // the reticle can find it; nothing is knocked about
			C->SetCollisionResponseToAllChannels(ECR_Block);
			C->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
			C->SetCastShadow(false);
			C->AttachToComponent(Root, FAttachmentTransformRules::KeepRelativeTransform);
			C->SetRelativeTransform(FTransform(Rot, At, Scale));
			C->RegisterComponent();
		};
		if (Ball) { for (int32 i = 0; i < 9; ++i)
		{
			const float R = FMath::FRandRange(0.07f, 0.16f);
			const FVector At(FMath::FRandRange(-28.0f, 28.0f), FMath::FRandRange(-22.0f, 22.0f), R * 30.0f);
			Make(Ball, At, FRotator(FMath::FRandRange(-20.f, 20.f), FMath::FRandRange(0.f, 360.f), FMath::FRandRange(-20.f, 20.f)), FVector(R, R * FMath::FRandRange(0.7f, 1.0f), R * 0.55f), MeatMat);
		} }
		if (Cyl) { for (int32 i = 0; i < 4; ++i)
		{
			const float Len = FMath::FRandRange(0.08f, 0.16f);
			Make(Cyl, FVector(FMath::FRandRange(-24.0f, 24.0f), FMath::FRandRange(-18.0f, 18.0f), 2.0f), FRotator(FMath::FRandRange(80.f, 100.f), FMath::FRandRange(0.f, 360.f), 0.0f), FVector(0.02f, 0.02f, Len), BoneMat);
		} }
		Pile->Tags.Add(TEXT("inspectable"));
		Pile->Tags.Add(*FString::Printf(TEXT("name:%s"), Label.IsEmpty() ? TEXT("Remains") : *FString::Printf(TEXT("Remains of %s"), *Label)));
		Pile->Tags.Add(TEXT("desc:Nothing left that could be called a body. Nothing left worth taking, either."));
		Pile->Tags.Add(TEXT("action:Inspect"));
		Pile->Tags.Add(TEXT("pile"));
		ShotReactions::PoolAt(World, Floor + FVector(0, 0, 5.0f), 150.0f);
		ImpactEffects::SpawnBurst(World, { TEXT("/Game/Synty/PolygonGeneric/FX/NS_Blood_Splatter_01"), 1.2f, 1.0f }, Centre, FRotator(80.0f, 0.0f, 0.0f));
		// The body goes, and everything that rode it: the head actor, a helmet, the loot box.
		TArray<AActor*> Attached; Who->GetAttachedActors(Attached, true, true);
		for (AActor* A : Attached)
		{
			if (!A) { continue; }
			if (A->ActorHasTag(TEXT("corpse_loot"))) { A->Destroy(); continue; }
			A->SetActorHiddenInGame(true); A->SetActorEnableCollision(false);
		}
		Who->SetActorHiddenInGame(true);
		Who->SetActorEnableCollision(false);
		for (int32 i = Who->Tags.Num() - 1; i >= 0; --i) { const FString T = Who->Tags[i].ToString(); if (T == TEXT("inspectable") || T.StartsWith(TEXT("action:"))) { Who->Tags.RemoveAt(i); } }
		Who->Tags.AddUnique(TEXT("pulped"));
		GLastRegion = TEXT("nothing left");
	}

	// The body falls. A character knows how (movement off, the mesh let go); an extra's idle stops
	// and every bone simulates, so whatever rides its pose (its head actor) comes down with it.
	void KillBody(UWorld* World, AActor* Who, USkeletalMeshComponent* Body, const FVector& ShotDir, const FHitResult* Hit)
	{
		Who->Tags.AddUnique(TEXT("dead"));
		if (ABaseCharacter* Ch = Cast<ABaseCharacter>(Who)) { Ch->Die(ShotDir); return; }
		CorpseLoot(World, Who, Body ? Body : FirstSkelWithBone(Who, TEXT("pelvis")));
		if (!Body) { Body = FirstSkelWithBone(Who, TEXT("pelvis")); }
		if (!Body) { return; }
		int32 Brawn = 50;   // an extra's, from a "brawn:NN" tag when it has one
		for (const FName& T : Who->Tags) { const FString Tag = T.ToString(); if (Tag.StartsWith(TEXT("brawn:"))) { Brawn = FCString::Atoi(*Tag.Mid(6)); } }
		UDeathPlayComponent::Begin(Who, Body, UDeathPlayComponent::Pick(Who), ShotDir, Brawn, Hit ? Hit->BoneName : NAME_None);
	}

	void ReactPerson(UWorld* World, const FHitResult& Hit, const FVector& ShotDir, float Damage, bool bBlunt, ShotReactions::FStrike* Info = nullptr)
	{
		AActor* Who = Hit.GetActor();
		const FString Region = ShotReactions::RegionOf(Hit.BoneName);
		GLastRegion = Region.IsEmpty() ? TEXT("body") : Region.ToLower();
		// Blood at the wound, thrown back along the shot, and the mark it leaves; a blunt blow bruises
		// instead. A machine bleeds sparks, blue and yellow, whatever hit it, and keeps its limbs.
		const bool bMachine = Who && Who->ActorHasTag(TEXT("robot"));
		// EVERY HIT ON A MACHINE SPARKS, and sparks properly: a machine that is hit SHOWS it. These
		// counts were a dozen motes because a dozen was all a burst could afford; a burst is
		// instanced now (see SparkFx.h) and costs about the same at sixty.
		if (bMachine) { SparkFx::Burst(World, Hit.ImpactPoint, -ShotDir, bBlunt ? 34 : 64, bBlunt ? 1.2f : 1.45f); }
		else
		{
			if (!bBlunt) { ImpactEffects::SpawnBurst(World, { TEXT("/Game/Synty/PolygonGeneric/FX/NS_Blood_Splatter_01"), 0.45f, 0.5f }, Hit.ImpactPoint, (-ShotDir).Rotation()); }
			MarkWound(World, Hit, ShotDir);   // a bruise for a blow, a wound for a shot: the mark rides the bone either way (a blow used to leave nothing to see)
		}
		USkeletalMeshComponent* Body = Cast<USkeletalMeshComponent>(Hit.GetComponent());
		UVitalityComponent* V = UVitalityComponent::FindOrAdd(Who);
		// A body already down just takes the shove.
		if (Info) { Info->bPerson = true; Info->Spent = Damage; }
		// What this blow would have to get through, for a strike's budget (read before it lands).
		const FString Key = Region.IsEmpty() ? FString(TEXT("Torso")) : Region;
		const float Factor = (Region == TEXT("Head") && V) ? V->HeadFactor : 1.0f;
		const float Remain = V ? V->Vitality : 0.0f;
		const float TakenBefore = V ? V->RegionDamage.FindRef(Key) : 0.0f;
		const float Thresh = V ? V->MaxVitality * V->DestroyedFraction : 0.0f;
		if (V && V->bDead)
		{
			// The shove a body already down takes. As a velocity change, so a heavy one and a light
			// one move alike; enough to roll it and drag a limb rather than just twitch the bone.
			if (Body && Body->IsSimulatingPhysics(Hit.BoneName)) { Body->AddImpulse(ShotDir * (bBlunt ? 380.0f : 210.0f), Hit.BoneName, true); }
			GLastRegion += TEXT(", dead");
			// A corpse keeps count: enough into a limb still takes it off.
			if (Damage > 0.0f)
			{
				const float Armour = ArmourFor(Who, Region, Hit.BoneName);
				const UVitalityComponent::FOutcome Out = V->Apply(Region, Damage, Armour);
				if (Out.bDestroyed && !bBlunt && !bMachine && CVarDismember.GetValueOnGameThread() != 0 && SeverRegion(World, Who, Body, Region, ShotDir)) { GLastRegion += TEXT(", torn off"); }
				if (Info) { Info->bDestroyed = Out.bDestroyed; if (Out.bDestroyed) { Info->Spent = FMath::Min(Damage, FMath::Min(V->Toughness(Key), FMath::Max(0.0f, Thresh - TakenBefore)) / Factor + Armour); } }   // the cheaper route it came off by
				if (V->IsPulped()) { PulpBody(World, Who, Body); }
			}
			return;
		}
		if (V && Damage > 0.0f)
		{
			const float Armour = ArmourFor(Who, Region, Hit.BoneName);
			const UVitalityComponent::FOutcome Out = V->Apply(Region, Damage, Armour);
			if (UAlertnessComponent* Aware = Who->FindComponentByClass<UAlertnessComponent>()) { Aware->NoteDamage(GStrikeInstigator, Out.Dealt); }   // whoever did this is known at once
		if (ABaseCharacter* Hurt = Cast<ABaseCharacter>(Who)) { Hurt->NoteHurt(Out.Dealt); }
			if (Info)
			{
				Info->bDied = Out.bDied; Info->bDestroyed = Out.bDestroyed;
				if (Out.bDied) { Info->Spent = FMath::Min(Damage, Remain / Factor + Armour); }
				else if (Out.bDestroyed) { Info->Spent = FMath::Min(Damage, FMath::Max(0.0f, Thresh - TakenBefore) / Factor + Armour); }
			}
			GLastRegion = FString::Printf(TEXT("%s -%.0f%s, %.0f/%.0f"), *GLastRegion, Out.Dealt, Armour > 0.0f ? *FString::Printf(TEXT(" (armour %.0f)"), Armour) : TEXT(""), V->Vitality, V->MaxVitality);
			ABaseCharacter* Ch = Cast<ABaseCharacter>(Who);
			if (Out.bInjured || Out.bDestroyed) { if (Ch) { Ch->NoteInjury(Region, Out.bDestroyed); } GLastRegion += Out.bDestroyed ? TEXT(", DESTROYED") : TEXT(", injured"); }
			if (Out.bDestroyed && !bBlunt && !bMachine && CVarDismember.GetValueOnGameThread() != 0) { SeverRegion(World, Who, Body, Region, ShotDir); }   // an edge takes it off; a hammer only breaks it
			if (Out.bDied) { KillBody(World, Who, Body, ShotDir, &Hit); GLastRegion += TEXT(", DEAD"); return; }
		}
		UAnimSequence* Flinch = FlinchFor(Who, ShotDir, bBlunt || Region == TEXT("Head") || Region == TEXT("Neck"));
		if (!Flinch) { return; }
		if (ABaseCharacter* Ch = Cast<ABaseCharacter>(Who)) { Ch->PlayHitReaction(Flinch); return; }
		// A posed extra (a single-node idle): the flinch plays through, then the pose comes back.
		if (ASkeletalMeshActor* Extra = Cast<ASkeletalMeshActor>(Who))
		{
			USkeletalMeshComponent* C = Extra->GetSkeletalMeshComponent();
			if (!C || C->GetAnimationMode() != EAnimationMode::AnimationSingleNode) { return; }
			UAnimationAsset* Was = C->AnimationData.AnimToPlay;
			C->PlayAnimation(Flinch, false);
			FTimerHandle H;
			World->GetTimerManager().SetTimer(H, FTimerDelegate::CreateWeakLambda(C, [C, Was]() { if (Was && C->GetOwner() && !C->GetOwner()->ActorHasTag(TEXT("dead")) && !C->IsSimulatingPhysics()) { C->PlayAnimation(Was, true); } }), FMath::Max(0.1f, Flinch->GetPlayLength() - 0.05f), false);   // not on a body that died meanwhile
		}
	}

	// Loose things: a prop the reticle knows (tagged interactable), with real collision to
	// simulate, and light enough to move. Mass from the catalogue when it is an item there,
	// else a guess from its size. Small breakables mostly burst.
	void ReactProp(UWorld* World, const FHitResult& Hit, const FVector& ShotDir, bool bBlunt, ShotReactions::FStrike* Info = nullptr)
	{
		AStaticMeshActor* Prop = Cast<AStaticMeshActor>(Hit.GetActor());
		if (!Prop || !Prop->ActorHasTag(TEXT("inspectable"))) { return; }
		UStaticMeshComponent* C = Prop->GetStaticMeshComponent();
		UStaticMesh* Mesh = C ? C->GetStaticMesh() : nullptr;
		if (!Mesh) { return; }
		FString Name;
		for (const FName& Tag : Prop->Tags) { const FString T = Tag.ToString(); if (T.StartsWith(TEXT("name:"))) { Name = T.Mid(5); break; } }
		const ItemCatalog::FRecord* R = Name.IsEmpty() ? nullptr : ItemCatalog::FindRecord(Name);
		float Mass = R ? static_cast<float>(R->Number(TEXT("mass_kg"), 0.0)) : 0.0f;
		const FBoxSphereBounds B = Mesh->GetBounds();
		if (Mass <= 0.0f)
		{
			const FVector E = B.BoxExtent * 2.0f * Prop->GetActorScale3D();
			Mass = FMath::Clamp(E.X * E.Y * E.Z * 1e-6f * 220.0f, 0.2f, 200.0f);   // a hollow-ish thing at 220 kg per cubic metre
		}
		if (Mass > 25.0f) { return; }   // bolted down as far as a bullet is concerned
		if (Info) { Info->Spent = FMath::Min(Info->Spent, Mass * 4.0f); }   // a loose thing costs a swing its weight; a wall costs everything
		const FString Lower = (Name.IsEmpty() ? Mesh->GetName() : Name).ToLower();
		static const TCHAR* Breakable[] = { TEXT("can"), TEXT("cup"), TEXT("bottle"), TEXT("drink"), TEXT("snack"), TEXT("food"), TEXT("tube"), TEXT("glass"), TEXT("vial"), TEXT("mug"), TEXT("plate"), TEXT("pad"), TEXT("noodle"), TEXT("packet"), TEXT("candy") };
		bool bBreakable = R && R->Category == TEXT("consumables");
		for (const TCHAR* K : Breakable) { if (Lower.Contains(K)) { bBreakable = true; break; } }
		if (bBreakable && Mass < 1.5f && FMath::FRand() < 0.6f)
		{
			// Burst: a puff and a spit of sparks where it was, and it is gone.
			ImpactEffects::SpawnBurst(World, { TEXT("/Game/PolygonSciFiWorlds/FX/Niagara/NS_Smoke_Small_01"), 0.3f, 0.08f }, Prop->GetActorLocation() + FVector(0, 0, B.BoxExtent.Z * 0.5f), FRotator::ZeroRotator);
			ImpactEffects::SpawnBurst(World, { TEXT("/Game/Synty/PolygonSciFiHorror/FX/NS_Spark_Shower_01"), 0.6f, 0.15f }, Hit.ImpactPoint, (-ShotDir).Rotation());
			Prop->SetActorHiddenInGame(true);
			Prop->SetActorEnableCollision(false);
			Prop->Tags.Add(TEXT("shot_out"));
			return;
		}
		// Knocked about: needs simple collision to simulate (a cut piece with complex-as-simple cannot).
		UBodySetup* Body = Mesh->GetBodySetup();
		if (!Body || Body->CollisionTraceFlag == CTF_UseComplexAsSimple || Body->AggGeom.GetElementCount() == 0) { return; }
		if (!C->IsSimulatingPhysics())
		{
			C->SetMobility(EComponentMobility::Movable);
			C->SetCollisionProfileName(TEXT("PhysicsActor"));
			C->SetSimulatePhysics(true);
			C->SetMassOverrideInKg(NAME_None, Mass, true);
			Prop->Tags.AddUnique(TEXT("loose"));
		}
		// A shove along the shot with a little lift, as a velocity change so light and heavy things move alike.
		C->AddImpulseAtLocation((ShotDir * (bBlunt ? 520.0f : 320.0f) + FVector(0, 0, bBlunt ? 160.0f : 110.0f)) * Mass, Hit.ImpactPoint);   // a swung weight moves more than a bullet
	}
}

UAnimSequence* ShotReactions::DeathClipFor(const AActor* Who, const FVector& ShotDir)
{
	if (!Who) { return nullptr; }
	const float Along = FVector::DotProduct(ShotDir, Who->GetActorForwardVector());
	const float Across = FVector::DotProduct(ShotDir, Who->GetActorRightVector());
	FString Side;
	if (Along > 0.5f) { Side = TEXT("Back"); }
	else if (Along < -0.5f) { Side = TEXT("Front"); }
	else { Side = Across > 0.0f ? TEXT("Left") : TEXT("Right"); }
	const int32 Take = Side == TEXT("Front") ? FMath::RandRange(1, 3) : 1;
	const FString Path = FString::Printf(TEXT("/Game/Characters/Animations/Lyra/death/MM_Death_%s_%02d"), *Side, Take);
	return LoadObject<UAnimSequence>(nullptr, *Path, nullptr, LOAD_NoWarn | LOAD_Quiet);
}

void ShotReactions::PoolAt(UWorld* World, const FVector& From, float SizeCm)
{
	static UMaterialInterface* Mat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/RepliCan/Materials/M_BloodPool.M_BloodPool"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	if (!World || !Mat) { return; }
	// The floor under the point; a body in the way is not a floor.
	FHitResult Floor; FCollisionQueryParams Q(SCENE_QUERY_STAT(BloodPool), false);
	if (!World->LineTraceSingleByChannel(Floor, From + FVector(0.0f, 0.0f, 20.0f), From - FVector(0.0f, 0.0f, 200.0f), ECC_Visibility, Q)) { return; }
	if (Cast<USkinnedMeshComponent>(Floor.GetComponent())) { return; }
	UDecalComponent* D = UGameplayStatics::SpawnDecalAtLocation(World, Mat, FVector(12.0f, SizeCm, SizeCm), Floor.ImpactPoint + FVector(0.0f, 0.0f, 1.0f), FRotator(-90.0f, FMath::FRandRange(0.0f, 360.0f), 0.0f), 600.0f);
	if (!D) { return; }
	D->SetFadeScreenSize(0.0004f);
	// It spreads: small at first, then wider by steps over six seconds.
	D->SetWorldScale3D(FVector(1.0f, 0.25f, 0.25f));
	TWeakObjectPtr<UDecalComponent> Weak(D);
	for (int32 Step = 1; Step <= 10; ++Step)
	{
		const float S = 0.25f + 0.75f * (Step / 10.0f);
		FTimerHandle H;
		World->GetTimerManager().SetTimer(H, FTimerDelegate::CreateLambda([Weak, S]() { if (Weak.IsValid()) { Weak->SetWorldScale3D(FVector(1.0f, S, S)); } }), 0.6f * Step, false);
	}
}

void ShotReactions::SettleRagdoll(AActor* Who, USkeletalMeshComponent* Body, int32 Brawn)
{
	if (!Who || !Body) { return; }
	// WEIGHT. The physics asset's capsules weigh what their volume says, which is next to nothing,
	// so any shot sends the body flying. Scaled to a person's mass by Brawn, and damped so it
	// settles instead of twitching.
	const float Target = 48.0f + 0.64f * FMath::Clamp(Brawn, 0, 100);
	const float Current = Body->GetMass();
	if (Current > KINDA_SMALL_NUMBER) { Body->SetAllMassScale(Target / Current); }
	for (FBodyInstance* BI : Body->Bodies) { if (BI) { BI->LinearDamping = 0.6f; BI->AngularDamping = 1.5f; BI->UpdateDampingProperties(); } }
	// The Ragdoll profile ignores BOTH of these, and for opposite reasons neither of which is ours.
	// Visibility: a body that ignored it could not be shot, swung at or picked out to loot.
	// Pawn: a corpse you can walk through is not a corpse, it is a decal -- and because the capsule
	// is switched off the moment the character dies (ABaseCharacter::Die), the ragdoll is the only
	// thing left that could stop anyone. Blocking it makes the body solid, and since SettleRagdoll
	// has just given it a person's mass, walking into one shoves it about rather than bouncing off.
	Body->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	Body->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
	// Whatever rides its pose keeps bodies of its own sitting inside the ragdoll's: followers, not
	// obstacles. The same for anything attached (an extra's head actor, a nose).
	TInlineComponentArray<UPrimitiveComponent*> Prims(Who);
	for (UPrimitiveComponent* P : Prims) { if (P && P != Body && Cast<USkinnedMeshComponent>(P)) { P->SetCollisionEnabled(ECollisionEnabled::NoCollision); } }
	TArray<AActor*> Attached; Who->GetAttachedActors(Attached, true, true);
	for (AActor* A : Attached) { TInlineComponentArray<UPrimitiveComponent*> AP(A); for (UPrimitiveComponent* P : AP) { if (P) { P->SetCollisionEnabled(ECollisionEnabled::NoCollision); } } }
}

bool ShotReactions::Sever(UWorld* World, AActor* Who, const FString& Region, const FVector& Dir)
{
	if (!World || !Who) { return false; }
	UVitalityComponent* V = UVitalityComponent::FindOrAdd(Who);
	if (V) { V->Destroyed.Add(Region); V->Injured.Add(Region); }
	if (ABaseCharacter* Ch = Cast<ABaseCharacter>(Who)) { Ch->NoteInjury(Region, true); }
	const bool bOff = SeverRegion(World, Who, nullptr, Region, Dir);
	// A head off is a death, however it came off: the body drops rather than standing there idling.
	if (bOff && Region == TEXT("Head") && V && !V->bDead) { V->Vitality = 0.0f; V->bDead = true; KillBody(World, Who, nullptr, Dir, nullptr); }
	return bOff;
}

void ShotReactions::Kill(UWorld* World, AActor* Who, const FVector& Dir)
{
	if (!World || !Who) { return; }
	if (UVitalityComponent* V = UVitalityComponent::FindOrAdd(Who)) { V->Vitality = 0.0f; V->bDead = true; }
	KillBody(World, Who, nullptr, Dir, nullptr);
}

ShotReactions::FStrike ShotReactions::Strike(UWorld* World, const FHitResult& Hit, const FVector& Dir, AActor* Instigator, float Budget, bool bBlunt)
{
	FStrike Info; Info.Spent = Budget;
	GLastRegion.Reset(); GStrikeInstigator = Instigator;
	if (!World || !Hit.GetActor() || Hit.GetActor() == Instigator) { Info.Spent = 0.0f; return Info; }
	FHitResult Resolved = Hit;
	if (ResolveAttachment(Resolved) && Resolved.GetActor() != Instigator) { ReactPerson(World, Resolved, Dir, Budget, bBlunt, &Info); }
	else if (Cast<USkeletalMeshComponent>(Hit.GetComponent()) || Cast<ABaseCharacter>(Hit.GetActor())) { ReactPerson(World, Hit, Dir, Budget, bBlunt, &Info); }
	else { ReactProp(World, Hit, Dir, bBlunt, &Info); }
	if (bBlunt) { Info.Spent = Budget; }   // a hammer stops in whatever it hits
	return Info;
}

void ShotReactions::React(UWorld* World, const FHitResult& Hit, const FVector& ShotDir, AActor* Instigator, float Damage, bool bBlunt)
{
	GLastRegion.Reset(); GStrikeInstigator = Instigator;
	if (!World || !Hit.GetActor() || Hit.GetActor() == Instigator) { return; }
	FHitResult Resolved = Hit;
	if (ResolveAttachment(Resolved) && Resolved.GetActor() != Instigator) { ReactPerson(World, Resolved, ShotDir, Damage, bBlunt); return; }
	if (Cast<USkeletalMeshComponent>(Hit.GetComponent()) || Cast<ABaseCharacter>(Hit.GetActor())) { ReactPerson(World, Hit, ShotDir, Damage, bBlunt); return; }
	ReactProp(World, Hit, ShotDir, bBlunt);
}
