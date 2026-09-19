#include "Weapons/BrassFx.h"
#include "World/AmbientPlayer.h"
#include "Narrative/VoiceLines.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Misc/Paths.h"

void UBrassFx::Eject(AActor* Owner, const FVector& Where, const FVector& Right, const FVector& Up, const FVector& Forward, bool bShotgunHull)
{
	if (!Owner || !Owner->GetWorld()) { return; }
	// The pool lives on the pawn (a controller's components never draw); a new pawn gets a new pool.
	if (PoolOwner != Owner) { for (FBrassCase& C : Pool) { if (C.Comp) { C.Comp->DestroyComponent(); } } Pool.Reset(); PoolOwner = Owner; Next = 0; }
	if (Pool.Num() < PoolSize)
	{
		FBrassCase C;
		C.Comp = NewObject<UStaticMeshComponent>(Owner, *FString::Printf(TEXT("Brass_%d"), Pool.Num()));
		static UStaticMesh* Cyl = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
		static UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
		C.Comp->SetStaticMesh(Cyl);
		if (Base)
		{
			UMaterialInstanceDynamic* M = UMaterialInstanceDynamic::Create(Base, Owner);
			M->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.85f, 0.62f, 0.22f, 1.0f));   // brass
			C.Comp->SetMaterial(0, M);
		}
		C.Comp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C.Comp->SetCastShadow(false);
		C.Comp->SetAbsolute(true, true, true);
		C.Comp->RegisterComponent();
		C.Comp->SetVisibility(false);
		Pool.Add(C);
	}
	FBrassCase& C = Pool[Next % Pool.Num()]; Next = (Next + 1) % PoolSize;
	if (!C.Comp) { return; }
	C.bShell = bShotgunHull;
	// A casing is a centimetre across and two and a half long; a hull thicker and longer. The
	// engine cylinder is 100 tall, so the scale is in hundredths.
	C.Comp->SetWorldScale3D(bShotgunHull ? FVector(0.02f, 0.02f, 0.06f) : FVector(0.0095f, 0.0095f, 0.025f));
	if (bShotgunHull && C.Comp->GetMaterial(0)) { if (UMaterialInstanceDynamic* M = Cast<UMaterialInstanceDynamic>(C.Comp->GetMaterial(0))) { M->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.75f, 0.12f, 0.08f, 1.0f)); } }
	else if (C.Comp->GetMaterial(0)) { if (UMaterialInstanceDynamic* M = Cast<UMaterialInstanceDynamic>(C.Comp->GetMaterial(0))) { M->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.85f, 0.62f, 0.22f, 1.0f)); } }
	C.Comp->SetWorldLocation(Where);
	C.Comp->SetWorldRotation(FRotationMatrix::MakeFromZ(Forward).Rotator());
	// Out of the port to the right and up, a little back, with the scatter of a real extractor.
	C.Velocity = Right * FMath::FRandRange(210.0f, 320.0f) + Up * FMath::FRandRange(130.0f, 230.0f) - Forward * FMath::FRandRange(20.0f, 90.0f);
	C.Spin = FVector(FMath::FRandRange(-900.0f, 900.0f), FMath::FRandRange(600.0f, 1400.0f), FMath::FRandRange(-500.0f, 500.0f));
	// Where the floor is under the port: found once, so the flight needs no traces.
	FHitResult Hit;
	FCollisionQueryParams Q(SCENE_QUERY_STAT(BrassFloor), false, Owner);
	C.FloorZ = Owner->GetWorld()->LineTraceSingleByChannel(Hit, Where, Where - FVector(0.0f, 0.0f, 600.0f), ECC_Visibility, Q) ? (float)Hit.ImpactPoint.Z : (float)(Where.Z - 120.0f);
	C.Age = 0.0f; C.bLanded = false; C.bLive = true; C.Bounces = 0;
	C.Comp->SetVisibility(true);
}

void UBrassFx::Tick(float DeltaSeconds)
{
	for (FBrassCase& C : Pool)
	{
		if (!C.bLive || !C.Comp) { continue; }
		C.Age += DeltaSeconds;
		if (C.Age > LifeSeconds) { C.bLive = false; C.Comp->SetVisibility(false); continue; }
		if (C.bLanded) { continue; }
		C.Velocity.Z -= 980.0f * DeltaSeconds;
		FVector P = C.Comp->GetComponentLocation() + C.Velocity * DeltaSeconds;
		const float Rest = C.FloorZ + (C.bShell ? 1.0f : 0.5f);
		if (P.Z <= Rest)
		{
			P.Z = Rest;
			if (C.Velocity.Z < -60.0f)
			{
				// The bounce: most of the drop lost, a little of the slide kept, and the tink.
				const float Drop = -C.Velocity.Z;   // how hard it came down, for how loud it lands
				C.Velocity = FVector(C.Velocity.X * 0.45f, C.Velocity.Y * 0.45f, -C.Velocity.Z * 0.3f);
				C.Spin *= 0.5f;
				// A casing is a small thing on a steel floor a metre away, not an event: only the
				// first two bounces are heard at all, and each is scaled by how hard it fell.
				if (++C.Bounces <= 2) { Tink(P, C.bShell, FMath::Clamp(Drop / 450.0f, 0.0f, 1.0f) / C.Bounces); }
			}
			else
			{
				// Down for good: on its side, still.
				C.bLanded = true; C.Velocity = FVector::ZeroVector;
				const FRotator R = C.Comp->GetComponentRotation();
				C.Comp->SetWorldRotation(FRotator(90.0f, R.Yaw, 0.0f));
			}
		}
		C.Comp->SetWorldLocation(P);
		if (!C.bLanded) { C.Comp->AddWorldRotation(FRotator(C.Spin.Y * DeltaSeconds, C.Spin.Z * DeltaSeconds, C.Spin.X * DeltaSeconds)); }
	}
}

void UBrassFx::Tink(const FVector& At, bool bShell, float Strength)
{
	if (!PoolOwner || !PoolOwner->GetWorld() || Strength < 0.05f) { return; }
	// Every casing used to leave a procedural wave alive for the rest of the session. A magazine's
	// worth of them is a roomful of clicking that follows you about.
	const FString File = bShell ? FString(TEXT("hull_01.wav")) : FString::Printf(TEXT("brass_%02d.wav"), FMath::RandRange(1, 3));
	UAmbientPlayer::PlayFileAt(this, PoolOwner->GetWorld(), File, At, (bShell ? 0.10f : 0.06f) * Strength, FMath::FRandRange(0.95f, 1.2f));
}
