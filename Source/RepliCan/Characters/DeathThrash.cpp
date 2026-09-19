#include "DeathThrash.h"
#include "BaseCharacter.h"
#include "ShotReactions.h"
#include "Animation/AnimSequence.h"
#include "Components/SkeletalMeshComponent.h"
#include "HAL/IConsoleManager.h"
#include "PhysicsEngine/BodyInstance.h"

static TAutoConsoleVariable<int32> CVarDeathStyle(TEXT("RepliCan.DeathStyle"), -1, TEXT("How bodies die: -1 random, 0 ragdoll, 1 leg first, 2 upper body first, 3 death clip, 4 robot"));

UDeathPlayComponent::UDeathPlayComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
}

EDeathStyle UDeathPlayComponent::Pick(const AActor* Who)
{
	const int32 Forced = CVarDeathStyle.GetValueOnGameThread();
	if (Forced >= 0 && Forced <= 4) { return (EDeathStyle)Forced; }
	if (Who && Who->ActorHasTag(TEXT("robot"))) { return EDeathStyle::Robot; }
	const float R = FMath::FRand();
	return R < 0.35f ? EDeathStyle::Clip : R < 0.6f ? EDeathStyle::Ragdoll : R < 0.8f ? EDeathStyle::LegFirst : EDeathStyle::UpperFirst;
}

void UDeathPlayComponent::Begin(AActor* Who, USkeletalMeshComponent* InBody, EDeathStyle InStyle, const FVector& InDir, int32 Brawn, FName InHitBone)
{
	if (!Who || !InBody) { return; }
	UDeathPlayComponent* C = NewObject<UDeathPlayComponent>(Who, TEXT("DeathPlay"));
	C->Body = InBody; C->Style = InStyle; C->ShotDir = InDir; C->HitBone = InHitBone;
	C->Strength = FMath::FRandRange(0.7f, 1.3f);
	C->RegisterComponent();
	// Weight, damping and the followers' collision, before anything simulates.
	ShotReactions::SettleRagdoll(Who, InBody, Brawn);
	InBody->SetCollisionProfileName(TEXT("Ragdoll"));
	// The Ragdoll profile ignores Visibility, which is the channel every trace here uses: a body that
	// ignored it could not be shot, swung at or picked out by the reticle to loot.
	InBody->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	switch (InStyle)
	{
	case EDeathStyle::LegFirst:
	{
		const FName Thigh(FMath::RandBool() ? TEXT("thigh_l") : TEXT("thigh_r"));
		InBody->SetAllBodiesBelowSimulatePhysics(Thigh, true, true);
		InBody->SetAllBodiesBelowPhysicsBlendWeight(Thigh, 1.0f, false, true);
		InBody->AddImpulse(InDir * 150.0f, Thigh, true);
		C->PhaseAt = FMath::FRandRange(0.35f, 0.7f);
		break;
	}
	case EDeathStyle::UpperFirst:
		InBody->SetAllBodiesBelowSimulatePhysics(TEXT("spine_02"), true, true);
		InBody->SetAllBodiesBelowPhysicsBlendWeight(TEXT("spine_02"), 1.0f, false, true);
		InBody->AddImpulse(InDir * 160.0f, TEXT("spine_03"), true);
		C->PhaseAt = FMath::FRandRange(0.4f, 0.8f);
		break;
	case EDeathStyle::Clip:
	{
		UAnimSequence* Clip = ShotReactions::DeathClipFor(Who, InDir);
		float Len = 0.0f;
		if (Clip)
		{
			if (ABaseCharacter* Ch = Cast<ABaseCharacter>(Who)) { if (Ch->PlayDeathClip(Clip)) { Len = Clip->GetPlayLength(); } }
			else if (InBody->GetAnimationMode() == EAnimationMode::AnimationSingleNode) { InBody->PlayAnimation(Clip, false); Len = Clip->GetPlayLength(); }
		}
		if (Len <= 0.0f) { C->Style = EDeathStyle::Ragdoll; C->FullRagdoll(); break; }   // no clip to play: a plain fall
		C->PhaseAt = Len * FMath::FRandRange(0.55f, 0.95f);   // the clip carries the body part of the way down; the ragdoll takes it from there
		break;
	}
	case EDeathStyle::Robot:
		for (const TCHAR* Limb : { TEXT("upperarm_l"), TEXT("upperarm_r"), TEXT("thigh_l"), TEXT("thigh_r") })
		{
			InBody->SetAllBodiesBelowSimulatePhysics(Limb, true, true);
			InBody->SetAllBodiesBelowPhysicsBlendWeight(Limb, 1.0f, false, true);
		}
		C->PhaseAt = FMath::FRandRange(2.0f, 4.0f);
		C->bThrash = false;
		C->NextKick = 0.05f;
		break;
	default:
		C->FullRagdoll();
		break;
	}
}

void UDeathPlayComponent::FullRagdoll()
{
	Phase = 1;
	if (!Body) { return; }
	Body->SetAllBodiesSimulatePhysics(true);
	Body->SetAllBodiesPhysicsBlendWeight(1.0f);
	Body->SetSimulatePhysics(true);
	Body->WakeAllRigidBodies();
	// The last shot's push, as a change of speed so a heavy body and a light one move alike.
	if (!HitBone.IsNone() && Body->GetBodyInstance(HitBone)) { Body->AddImpulse(ShotDir * 180.0f, HitBone, true); } else { Body->AddImpulse(ShotDir * 120.0f, NAME_None, true); }
	ThrashFrom = Clock + FMath::FRandRange(0.2f, 0.5f);
	ThrashUntil = bThrash ? ThrashFrom + FMath::FRandRange(0.9f, 2.4f) : 0.0f;
	NextKick = ThrashFrom;
}

void UDeathPlayComponent::Kick(bool bRobot, float Fade)
{
	static const FName Limbs[] = { TEXT("upperarm_l"), TEXT("upperarm_r"), TEXT("lowerarm_l"), TEXT("lowerarm_r"), TEXT("hand_l"), TEXT("hand_r"),
	                               TEXT("thigh_l"), TEXT("thigh_r"), TEXT("calf_l"), TEXT("calf_r"), TEXT("foot_l"), TEXT("foot_r"), TEXT("spine_02"), TEXT("spine_03"), TEXT("head") };
	const int32 Count = bRobot ? FMath::RandRange(1, 2) : FMath::RandRange(1, 3);
	for (int32 i = 0; i < Count; ++i)
	{
		const FName Bone = Limbs[FMath::RandRange(0, UE_ARRAY_COUNT(Limbs) - 1)];
		if (!Body->GetBodyInstance(Bone)) { continue; }
		// A person's limb mostly lifts and drops back; a machine's snaps any way at all.
		FVector Dir = FMath::VRand();
		if (!bRobot) { Dir.Z = FMath::Abs(Dir.Z) * 0.8f + 0.2f; }
		const float Lin = (bRobot ? FMath::FRandRange(150.0f, 350.0f) : FMath::FRandRange(90.0f, 240.0f)) * Strength * Fade;
		const float Ang = (bRobot ? FMath::FRandRange(900.0f, 2000.0f) : FMath::FRandRange(200.0f, 600.0f)) * Strength * Fade;
		Body->AddImpulse(Dir.GetSafeNormal() * Lin, Bone, true);
		Body->AddAngularImpulseInDegrees(FMath::VRand() * Ang, Bone, true);
	}
	if (!bRobot && FMath::FRand() < 0.25f && Body->GetBodyInstance(TEXT("pelvis"))) { Body->AddImpulse(FVector(0.0f, 0.0f, FMath::FRandRange(60.0f, 140.0f)) * Strength * Fade, TEXT("pelvis"), true); }
}

void UDeathPlayComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	Clock += DeltaTime;
	if (!Body) { DestroyComponent(); return; }
	if (Phase == 0)
	{
		if (Clock >= PhaseAt) { FullRagdoll(); return; }
		if (Style == EDeathStyle::Robot && Clock >= NextKick)
		{
			// Spasms, with the odd hold between them.
			NextKick = Clock + (FMath::FRand() < 0.2f ? FMath::FRandRange(0.4f, 0.8f) : FMath::FRandRange(0.05f, 0.25f));
			Kick(true, 1.0f);
		}
		return;
	}
	// Down: the pool spreads under the trunk once it has come to rest a moment.
	if (!bPooled && Clock >= ThrashFrom + 1.5f) { bPooled = true; if (Body->GetBoneIndex(TEXT("pelvis")) != INDEX_NONE) { ShotReactions::PoolAt(GetWorld(), Body->GetSocketLocation(TEXT("pelvis")), 110.0f); } }
	if (!Body->IsSimulatingPhysics() || (ThrashUntil <= 0.0f && bPooled) || (ThrashUntil > 0.0f && Clock >= ThrashUntil && bPooled)) { DestroyComponent(); return; }
	if (ThrashUntil <= 0.0f || Clock < NextKick || Clock >= ThrashUntil) { return; }
	const float T = FMath::Clamp((Clock - ThrashFrom) / FMath::Max(0.1f, ThrashUntil - ThrashFrom), 0.0f, 1.0f);
	NextKick = Clock + FMath::FRandRange(0.08f, 0.22f) * (1.0f + T);
	Kick(false, 1.0f - T * T);
}
