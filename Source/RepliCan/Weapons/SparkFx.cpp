#include "Weapons/SparkFx.h"
#include "Components/PointLightComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"

// HOW BIG A MOTE IS, as a fraction of the engine sphere (which is a metre across). Spawn and Tick
// both size from this. They used to carry separate literals -- 0.030 and 0.014 -- and the smaller
// one won from the second frame onward; a one-pixel additive speck is exactly what temporal
// anti-aliasing averages away, so however hot the colour, the result was a dim fleck.
static constexpr float SparkMoteScale = 0.030f;

ASparkBurstActor::ASparkBurstActor()
{
	PrimaryActorTick.bCanEverTick = true;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
}

void ASparkBurstActor::Init(const FVector& Where, const FVector& Normal, int32 Count, float Scale, const FLinearColor& ColourA, const FLinearColor& ColourB)
{
	static UStaticMesh* Ball = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	// A SPARK MATERIAL, NOT THE BEAM'S. M_LaserBeam fades its opacity by 1 - Fresnel, which is right
	// for a cylinder -- a wide band of it faces the camera -- and wrong for a sphere, where only the
	// centre point does and everything else curves away to a grazing angle. On an additive material
	// that opacity scales the whole contribution, so most of every mote was being faded to nothing
	// and no amount of colour or Heat could put it back. M_Spark is the same emissive with the fade
	// removed (Tools/make_spark_material).
	static UMaterialInterface* Hot = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/RepliCan/Materials/M_Spark.M_Spark"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	// Two colours, about half and half. The beam's material already glows through anything; it
	// only needs its colour told.
	UMaterialInstanceDynamic* Blue = Hot ? UMaterialInstanceDynamic::Create(Hot, this) : nullptr;
	UMaterialInstanceDynamic* Yellow = Hot ? UMaterialInstanceDynamic::Create(Hot, this) : nullptr;
	if (Blue) { Blue->SetVectorParameterValue(TEXT("Colour"), ColourA); Blue->SetScalarParameterValue(TEXT("Heat"), 16.0f); }
	if (Yellow) { Yellow->SetVectorParameterValue(TEXT("Colour"), ColourB); Yellow->SetScalarParameterValue(TEXT("Heat"), 14.0f); }
	FVector N = Normal.GetSafeNormal();
	if (N.IsNearlyZero()) { N = FVector::UpVector; }
	// One instanced component per colour. Everything a mote needs is in its instance transform, so
	// nothing here is per-mote except a struct in an array.
	auto MakeBank = [&](const TCHAR* Name, UMaterialInstanceDynamic* Mat) -> UInstancedStaticMeshComponent*
	{
		UInstancedStaticMeshComponent* C = NewObject<UInstancedStaticMeshComponent>(this, Name);
		C->SetStaticMesh(Ball);
		if (Mat) { C->SetMaterial(0, Mat); }
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->SetCastShadow(false);
		C->SetMobility(EComponentMobility::Movable);
		C->NumCustomDataFloats = 0;
		C->SetupAttachment(GetRootComponent());
		C->SetAbsolute(true, true, true);
		C->RegisterComponent();
		return C;
	};
	Cool = MakeBank(TEXT("Cool"), Blue);
	Warm = MakeBank(TEXT("Warm"), Yellow);
	Motes.Reserve(Count);
	for (int32 i = 0; i < Count; ++i)
	{
		FMote M;
		M.Pos = Where + N * 2.0f;
		// Mostly out along the normal, scattered, with a lift: sparks jump before they fall.
		M.Vel = N * FMath::FRandRange(90.0f, 320.0f) + FMath::VRand() * FMath::FRandRange(60.0f, 220.0f) + FVector(0.0f, 0.0f, FMath::FRandRange(40.0f, 160.0f));
		M.Life = FMath::FRandRange(0.22f, 0.55f);
		M.Size = FMath::FRandRange(0.7f, 1.3f) * Scale;
		M.bWarm = (i % 2) != 0;
		// BIG ENOUGH TO SURVIVE THE ANTI-ALIASING. The engine sphere is a metre across, and a mote
		// was 0.014 of it -- fourteen millimetres, which a few metres out is about one pixel. A
		// one-pixel additive speck is exactly what temporal anti-aliasing is built to average away,
		// so however hot the colour the result on screen is a dim fleck: the reason these read as
		// specks of oil rather than fire was never the colour, it was the size.
		const FTransform Xf(FQuat::Identity, M.Pos, FVector(SparkMoteScale * M.Size));
		UInstancedStaticMeshComponent* Bank = M.bWarm ? Warm : Cool;
		M.Slot = Bank ? Bank->AddInstance(Xf, /*bWorldSpace=*/true) : 0;
		Motes.Add(M);
	}
	Flash = NewObject<UPointLightComponent>(this, TEXT("Flash"));
	Flash->SetupAttachment(GetRootComponent());
	Flash->SetAbsolute(true, true, true);
	Flash->SetIntensityUnits(ELightUnits::Candelas);
	// AND THE LIGHT IT THROWS. Nine candelas is not a light, it is a rounding error -- a single
	// bullet hit in Impacts.json flashes at 1600. A shower of sparks off a machine should light the
	// wall it happens against, which is most of what sells it as hot.
	FlashPeak = 520.0f * Scale;
	Flash->SetIntensity(FlashPeak);
	Flash->SetAttenuationRadius(320.0f * Scale);
	Flash->SetLightColor(FLinearColor(0.55f, 0.75f, 1.0f));
	Flash->SetCastShadows(false);
	Flash->SetWorldLocation(Where + N * 8.0f);
	Flash->RegisterComponent();
	SetLifeSpan(1.2f);   // whatever is left by then goes with it
}

void ASparkBurstActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Clock += DeltaSeconds;
	if (Flash) { Flash->SetIntensity(Clock < FlashSeconds ? FlashPeak * (1.0f - Clock / FlashSeconds) : 0.0f); }
	// The two banks are rewritten in two batched calls rather than one component update per mote,
	// which is the whole point of the instancing: the cost is in the count of BANKS, not motes.
	bool bAny = false;
	TArray<FTransform> CoolXf, WarmXf;
	if (Cool) { CoolXf.SetNum(Cool->GetInstanceCount()); }
	if (Warm) { WarmXf.SetNum(Warm->GetInstanceCount()); }
	for (FMote& M : Motes)
	{
		TArray<FTransform>& Into = M.bWarm ? WarmXf : CoolXf;
		if (!Into.IsValidIndex(M.Slot)) { continue; }
		M.Age += DeltaSeconds;
		if (M.Age >= M.Life)
		{
			Into[M.Slot] = FTransform(FQuat::Identity, M.Pos, FVector::ZeroVector);   // a dead mote is a zero-scale instance: no draw, no removal churn
			continue;
		}
		bAny = true;
		M.Vel.Z -= 980.0f * DeltaSeconds;
		M.Vel *= FMath::Max(0.0f, 1.0f - 1.6f * DeltaSeconds);   // the air takes the speed off quickly
		M.Pos += M.Vel * DeltaSeconds;
		const float Fade = 1.0f - M.Age / M.Life;
		// THE SAME BASE SIZE THE MOTE WAS BORN AT. This read 0.014 -- the old sub-pixel value the
		// spawn above was fixed away from -- so every burst was the right size for exactly one
		// frame and fourteen millimetres or less from the second frame on. The size fix was inert
		// wherever anyone could actually see it, which is why these still read as dark flecks after
		// the material was corrected. One constant now, so the two cannot drift apart again.
		Into[M.Slot] = FTransform(FQuat::Identity, M.Pos, FVector(SparkMoteScale * M.Size * (0.60f + 0.40f * Fade)));
	}
	if (Cool && CoolXf.Num() > 0) { Cool->BatchUpdateInstancesTransforms(0, CoolXf, /*bWorldSpace=*/true, /*bMarkRenderStateDirty=*/true, /*bTeleport=*/true); }
	if (Warm && WarmXf.Num() > 0) { Warm->BatchUpdateInstancesTransforms(0, WarmXf, /*bWorldSpace=*/true, /*bMarkRenderStateDirty=*/true, /*bTeleport=*/true); }
	if (!bAny && Clock > FlashSeconds) { Destroy(); }
}

namespace SparkFx
{
	void Burst(UWorld* World, const FVector& Where, const FVector& Normal, int32 Count, float Scale, const FLinearColor& ColourA, const FLinearColor& ColourB)
	{
		if (!World) { return; }
		FActorSpawnParameters P; P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		if (ASparkBurstActor* A = World->SpawnActor<ASparkBurstActor>(ASparkBurstActor::StaticClass(), Where, FRotator::ZeroRotator, P)) { A->Init(Where, Normal, Count, Scale, ColourA, ColourB); }
	}
}
