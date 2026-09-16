#include "ImpactEffects.h"
#include "AmbientPlayer.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Dom/JsonObject.h"
#include "Engine/World.h"
#include "Engine/HitResult.h"
#include "GameFramework/Actor.h"
#include "Components/PrimitiveComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/SkeletalMesh.h"
#include "Components/PointLightComponent.h"
#include "Components/DecalComponent.h"
#include "Materials/MaterialInterface.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "TimerManager.h"

namespace
{
	struct FRule
	{
		FString Match;              // substring tested against the hit's description; empty = default
		TArray<ImpactEffects::FBurst> Fx;   // what is spawned, in order: a puff, then sparks
		FString Decal;
		float DecalSize = 8.0f;
		float DecalLifeSeconds = 25.0f;
		FLinearColor LightColor = FLinearColor(1.0f, 0.75f, 0.35f);
		float LightIntensity = 0.0f;
		float LightRadius = 220.0f;
		float LightSeconds = 0.06f;
		FString Sound;
		// How many numbered takes of that sound exist (impact_metal_1..N). One sample on repeat
		// is the fastest way to make a gunfight sound cheap, and four is enough to hide it.
		int32 SoundVariants = 1;
		float Volume = 0.55f;
	};

	TArray<FRule> GRules;
	bool GImpactEffectsLoaded = false;

	FString RuleFile()
	{
		return FPaths::Combine(FPaths::ProjectDir(), TEXT("UI"), TEXT("Impacts.json"));
	}

	void ReadRule(const TSharedPtr<FJsonObject>& Obj, FRule& R)
	{
		Obj->TryGetStringField(TEXT("match"), R.Match);
		// One system as "niagara" + "scale" (the old shape), or several as "fx": [{niagara, scale,
		// seconds}, ...]. Either replaces the list the rule inherited. "seconds" cuts the
		// emitter off after that long: the packs' spark and smoke systems are ambient loops,
		// and a tenth of a second of one is a burst.
		FString One;
		if (Obj->TryGetStringField(TEXT("niagara"), One))
		{
			R.Fx.Reset();
			double Sc = 1.0, Sec = 0.0; Obj->TryGetNumberField(TEXT("scale"), Sc); Obj->TryGetNumberField(TEXT("seconds"), Sec);
			if (!One.IsEmpty()) { R.Fx.Add({ One, (float)Sc, (float)Sec }); }
		}
		const TArray<TSharedPtr<FJsonValue>>* FxArr = nullptr;
		if (Obj->TryGetArrayField(TEXT("fx"), FxArr) && FxArr)
		{
			R.Fx.Reset();
			for (const TSharedPtr<FJsonValue>& V : *FxArr)
			{
				const TSharedPtr<FJsonObject> O = V->AsObject();
				if (!O.IsValid()) { continue; }
				ImpactEffects::FBurst B; O->TryGetStringField(TEXT("niagara"), B.System);
				double Sc = 1.0, Sec = 0.0; if (O->TryGetNumberField(TEXT("scale"), Sc)) { B.Scale = Sc; } if (O->TryGetNumberField(TEXT("seconds"), Sec)) { B.Seconds = Sec; }
				if (!B.System.IsEmpty()) { R.Fx.Add(B); }
			}
		}
		Obj->TryGetStringField(TEXT("decal"), R.Decal);
		Obj->TryGetStringField(TEXT("sound"), R.Sound);
		double N = 0.0;
		if (Obj->TryGetNumberField(TEXT("decal_size"), N)) { R.DecalSize = N; }
		if (Obj->TryGetNumberField(TEXT("decal_life"), N)) { R.DecalLifeSeconds = N; }
		if (Obj->TryGetNumberField(TEXT("light_intensity"), N)) { R.LightIntensity = N; }
		if (Obj->TryGetNumberField(TEXT("light_radius"), N)) { R.LightRadius = N; }
		if (Obj->TryGetNumberField(TEXT("light_seconds"), N)) { R.LightSeconds = N; }
		if (Obj->TryGetNumberField(TEXT("volume"), N)) { R.Volume = N; }
		if (Obj->TryGetNumberField(TEXT("variants"), N)) { R.SoundVariants = FMath::Max(1, (int32)N); }
		const TArray<TSharedPtr<FJsonValue>>* Col = nullptr;
		if (Obj->TryGetArrayField(TEXT("light_color"), Col) && Col && Col->Num() >= 3)
		{
			R.LightColor = FLinearColor((*Col)[0]->AsNumber(), (*Col)[1]->AsNumber(), (*Col)[2]->AsNumber());
		}
	}

	void LoadIfNeeded()
	{
		if (GImpactEffectsLoaded) { return; }
		GImpactEffectsLoaded = true;
		GRules.Reset();
		FString Text;
		if (!FFileHelper::LoadFileToString(Text, *RuleFile()))
		{
			UE_LOG(LogTemp, Warning, TEXT("ImpactEffects: could not read %s"), *RuleFile());
			return;
		}
		TSharedPtr<FJsonObject> Root;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Text);
		if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid()) { return; }

		// A "default" block every rule starts from, so a rule only has to say what differs.
		FRule Base;
		const TSharedPtr<FJsonObject>* Def = nullptr;
		if (Root->TryGetObjectField(TEXT("default"), Def)) { ReadRule(*Def, Base); }

		const TArray<TSharedPtr<FJsonValue>>* Rules = nullptr;
		if (Root->TryGetArrayField(TEXT("rules"), Rules) && Rules)
		{
			for (const TSharedPtr<FJsonValue>& V : *Rules)
			{
				const TSharedPtr<FJsonObject> Obj = V->AsObject();
				if (!Obj.IsValid()) { continue; }
				FRule R = Base;
				ReadRule(Obj, R);
				GRules.Add(MoveTemp(R));
			}
		}
		// The fallback goes last, so the first match always wins and order in the file is the
		// order of specificity -- which is how anyone reading the file would expect it to work.
		Base.Match.Reset();
		GRules.Add(MoveTemp(Base));
		UE_LOG(LogTemp, Log, TEXT("ImpactEffects: %d rules"), GRules.Num());
	}

	// Everything known about what was hit, as one lowercase string for the rules to match
	// against. Deliberately generous: a rule can key off the actor's label, its tags, the mesh
	// it is built from, or the material on the face that was struck.
	FString DescribeHit(const FHitResult& Hit)
	{
		FString Out;
		if (const AActor* A = Hit.GetActor())
		{
			Out += A->GetActorNameOrLabel() + TEXT("|");
			for (const FName& Tag : A->Tags) { Out += Tag.ToString() + TEXT("|"); }
		}
		if (const UPrimitiveComponent* C = Hit.GetComponent())
		{
			if (const UMaterialInterface* M = C->GetMaterial(0))
			{
				Out += M->GetName() + TEXT("|");
			}
			Out += C->GetName() + TEXT("|");
			// The asset path says more than anything else here: Synty sorts its content into
			// Walls, Floors, Props, Characters, and a folder name is a serviceable surface type
			// when the materials are all called M_PolygonSciFiSpace_01_A.
			if (const UStaticMeshComponent* SM = Cast<UStaticMeshComponent>(C))
			{
				if (const UStaticMesh* Mesh = SM->GetStaticMesh()) { Out += Mesh->GetPathName() + TEXT("|"); }
			}
			else if (const USkeletalMeshComponent* SK = Cast<USkeletalMeshComponent>(C))
			{
				if (const USkeletalMesh* Mesh = SK->GetSkeletalMeshAsset()) { Out += Mesh->GetPathName() + TEXT("|"); }
			}
		}
		return Out.ToLower();
	}

	const FRule& PickRule(const FString& Description)
	{
		LoadIfNeeded();
		for (const FRule& R : GRules)
		{
			if (R.Match.IsEmpty() || Description.Contains(R.Match)) { return R; }
		}
		static const FRule None;
		return None;
	}

	// A light that exists for a few hundredths of a second. The same trick as the muzzle flash:
	// what sells an impact is not the particles, it is the room lighting up for one frame.
	void FlashAt(UWorld* World, const FVector& Where, const FRule& R)
	{
		if (R.LightIntensity <= 0.0f) { return; }
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AActor* Holder = World->SpawnActor<AActor>(AActor::StaticClass(), FTransform(Where), Params);
		if (!Holder) { return; }
		UPointLightComponent* Light = NewObject<UPointLightComponent>(Holder);
		Light->SetMobility(EComponentMobility::Movable);
		Holder->SetRootComponent(Light);
		Light->RegisterComponent();
		Light->SetWorldLocation(Where);
		Light->SetLightColor(R.LightColor);
		Light->SetIntensity(R.LightIntensity);
		Light->SetAttenuationRadius(R.LightRadius);
		Light->SetCastShadows(false);
		Holder->SetLifeSpan(FMath::Max(0.01f, R.LightSeconds));
	}
}

void ImpactEffects::Play(UWorld* World, const FHitResult& Hit, AActor* Instigator, float VolumeScale)
{
	if (!World || !Hit.bBlockingHit) { return; }
	const FRule& R = PickRule(DescribeHit(Hit));

	// Sparks and dust fly back out of the surface, so the burst is oriented along the normal
	// rather than along the shot -- a hit on a wall should spray into the room, not into it.
	const FRotator Facing = Hit.ImpactNormal.Rotation();

	for (const ImpactEffects::FBurst& B : R.Fx) { ImpactEffects::SpawnBurst(World, B, Hit.ImpactPoint, Facing); }

	if (!R.Decal.IsEmpty() && R.DecalSize > 0.0f)
	{
		if (UMaterialInterface* Mat = LoadObject<UMaterialInterface>(nullptr, *R.Decal, nullptr, LOAD_NoWarn | LOAD_Quiet))
		{
			// Decals project down their own -X, so the decal faces INTO the surface: the
			// rotation is the inverse normal, not the normal. Sized as a box, and given a
			// random roll so a burst into one wall is not the same stamp nine times.
			const FVector Size(FMath::Max(2.0f, R.DecalSize * 0.5f), R.DecalSize, R.DecalSize);
			FRotator DecalRot = (-Hit.ImpactNormal).Rotation();
			DecalRot.Roll = FMath::FRandRange(0.0f, 360.0f);
			if (UDecalComponent* D = UGameplayStatics::SpawnDecalAtLocation(World, Mat, Size, Hit.ImpactPoint, DecalRot, R.DecalLifeSeconds))
			{
				// Fade out over the last fifth of its life rather than blinking away.
				D->SetFadeOut(R.DecalLifeSeconds * 0.8f, R.DecalLifeSeconds * 0.2f, false);
			}
		}
	}

	FlashAt(World, Hit.ImpactPoint + Hit.ImpactNormal * 4.0f, R);

	if (!R.Sound.IsEmpty())
	{
		FString Wav = R.Sound;
		if (R.SoundVariants > 1 && !Wav.EndsWith(TEXT(".wav")))
		{
			Wav += FString::Printf(TEXT("_%d"), FMath::RandRange(1, R.SoundVariants));
		}
		if (!Wav.EndsWith(TEXT(".wav"))) { Wav += TEXT(".wav"); }
		UAmbientPlayer::PlayOneShot(Instigator, World, Wav, R.Volume * VolumeScale, FMath::FRandRange(0.9f, 1.12f));
	}
}

UNiagaraComponent* ImpactEffects::SpawnBurst(UWorld* World, const FBurst& B, const FVector& Where, const FRotator& Facing)
{
	if (!World || B.System.IsEmpty()) { return nullptr; }
	UNiagaraSystem* Sys = LoadObject<UNiagaraSystem>(nullptr, *B.System, nullptr, LOAD_NoWarn | LOAD_Quiet);
	if (!Sys) { return nullptr; }
	UNiagaraComponent* C = UNiagaraFunctionLibrary::SpawnSystemAtLocation(World, Sys, Where, Facing, FVector(FMath::Max(0.01f, B.Scale)), /*bAutoDestroy=*/true);
	if (C && B.Seconds > 0.0f)
	{
		// Deactivate, not destroy: what has been emitted finishes its own life, nothing more is
		// emitted, and the auto-destroy takes the component once the last particle is gone.
		FTimerHandle H;
		World->GetTimerManager().SetTimer(H, FTimerDelegate::CreateWeakLambda(C, [C]() { C->Deactivate(); }), B.Seconds, false);
	}
	return C;
}

void ImpactEffects::Reload() { GImpactEffectsLoaded = false; }
int32 ImpactEffects::NumRules() { LoadIfNeeded(); return GRules.Num(); }
