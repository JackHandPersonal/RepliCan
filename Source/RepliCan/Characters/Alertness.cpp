#include "Characters/Alertness.h"
#include "Characters/BaseCharacter.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/UObjectIterator.h"

UAlertnessComponent::UAlertnessComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

const TCHAR* UAlertnessComponent::StateName(EAlertState S)
{
	switch (S)
	{
	case EAlertState::Suspicious: return TEXT("suspicious");
	case EAlertState::Alert: return TEXT("alert");
	case EAlertState::Searching: return TEXT("searching");
	default: return TEXT("calm");
	}
}

float UAlertnessComponent::SecondsInState() const
{
	return GetWorld() ? GetWorld()->GetTimeSeconds() - StateSince : 0.0f;
}

FVector UAlertnessComponent::Eyes() const
{
	const AActor* O = GetOwner();
	if (const ACharacter* C = Cast<ACharacter>(O))
	{
		if (C->GetMesh() && C->GetMesh()->DoesSocketExist(TEXT("head"))) { return C->GetMesh()->GetSocketLocation(TEXT("head")) + FVector(0.0f, 0.0f, 8.0f); }
	}
	return O ? O->GetActorLocation() + FVector(0.0f, 0.0f, 70.0f) : FVector::ZeroVector;
}

namespace
{
	// Where a body can be seen: its head and its chest, or its centre for anything else.
	void PointsOn(const AActor* Who, TArray<FVector>& Out)
	{
		if (const ACharacter* C = Cast<ACharacter>(Who))
		{
			if (C->GetMesh() && C->GetMesh()->DoesSocketExist(TEXT("head"))) { Out.Add(C->GetMesh()->GetSocketLocation(TEXT("head"))); }
			if (C->GetMesh() && C->GetMesh()->DoesSocketExist(TEXT("spine_02"))) { Out.Add(C->GetMesh()->GetSocketLocation(TEXT("spine_02"))); }
		}
		if (Out.Num() == 0) { Out.Add(Who->GetActorLocation()); }
	}
	bool DeadOrGone(const AActor* Who)
	{
		if (!Who) { return true; }
		if (const ABaseCharacter* Ch = Cast<ABaseCharacter>(Who)) { return Ch->IsDead(); }
		return Who->ActorHasTag(TEXT("dead"));
	}
}

bool UAlertnessComponent::CanSee(const AActor* Who) const
{
	const AActor* O = GetOwner(); UWorld* World = GetWorld();
	if (!O || !Who || !World) { return false; }
	const FVector From = Eyes();
	TArray<FVector> Points; PointsOn(Who, Points);
	const FVector Forward = O->GetActorForwardVector();
	const float CosHalf = FMath::Cos(FMath::DegreesToRadians(SightFovDeg * 0.5f));
	FCollisionQueryParams Q(SCENE_QUERY_STAT(Alertness), true);
	Q.AddIgnoredActor(O); Q.AddIgnoredActor(Who);
	TArray<AActor*> Attached; O->GetAttachedActors(Attached, true, true); Q.AddIgnoredActors(Attached);
	Attached.Reset(); Who->GetAttachedActors(Attached, true, true); Q.AddIgnoredActors(Attached);
	for (const FVector& P : Points)
	{
		const FVector D = P - From; const float Dist = (float)D.Size();
		if (Dist > SightRange) { continue; }
		if (Dist > 120.0f && FVector::DotProduct(D / Dist, Forward) < CosHalf) { continue; }   // outside the cone (anything at arm's length is felt regardless)
		FHitResult Hit;
		if (!World->LineTraceSingleByChannel(Hit, From, P, ECC_Visibility, Q)) { return true; }   // nothing between
	}
	return false;
}

void UAlertnessComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	AActor* O = GetOwner(); UWorld* World = GetWorld();
	if (!O || !World || DeadOrGone(O)) { return; }
	const float Now = World->GetTimeSeconds();
	// Who is watched: the player. (Every observer is the player's enemy for now; a faction table
	// goes here when there is one.)
	APawn* Player = UGameplayStatics::GetPlayerPawn(World, 0);
	AActor* Seen = nullptr; float Gain = 0.0f;
	if (Player && Player != O && !DeadOrGone(Player) && CanSee(Player))
	{
		Seen = Player;
		const float Dist = (float)FVector::Dist(Eyes(), Player->GetActorLocation());
		float G = GainPerSecond * FMath::Pow(FMath::Clamp(1.0f - Dist / SightRange, 0.0f, 1.0f), 0.7f);
		if (const ACharacter* C = Cast<ACharacter>(Player)) { if (C->bIsCrouched) { G *= CrouchFactor; } }   // low is harder to see
		if (Player->GetVelocity().Size() < 25.0f) { G *= StillFactor; }                                    // still is harder again
		if (Dist < 250.0f) { G = FMath::Max(G, GainPerSecond * 2.0f); }                                       // nobody misses a person at arm's length
		Gain = G;
	}
	if (Seen)
	{
		Awareness = FMath::Min(AlertAt * 1.5f, Awareness + Gain * DeltaTime);
		LastSeenAt = Now; LastKnown = Seen->GetActorLocation(); bHasLastKnown = true;
		if (State == EAlertState::Alert || State == EAlertState::Searching || Awareness >= AlertAt) { Target = Seen; }
	}
	else if (State != EAlertState::Alert) { Awareness = FMath::Max(0.0f, Awareness - DecayPerSecond * DeltaTime); }
	switch (State)
	{
	case EAlertState::Calm:
		if (Awareness >= SuspiciousAt) { SetState(EAlertState::Suspicious); }
		break;
	case EAlertState::Suspicious:
		if (Awareness >= AlertAt) { SetState(EAlertState::Alert); }
		else if (Awareness < 0.05f) { SetState(EAlertState::Calm); }
		break;
	case EAlertState::Alert:
		if (!Seen && Now - LastSeenAt > 3.0f) { SetState(EAlertState::Searching); }
		break;
	case EAlertState::Searching:
		if (Seen) { SetState(EAlertState::Alert); }
		else if (SecondsInState() > SearchSeconds) { ForgetTarget(); SetState(EAlertState::Calm); }
		break;
	}
}

void UAlertnessComponent::ReportNoise(UWorld* World, const FVector& Where, float Radius, AActor* Source)
{
	if (!World) { return; }
	for (TObjectIterator<UAlertnessComponent> It; It; ++It)
	{
		if (It->IsTemplate() || It->GetWorld() != World || !It->GetOwner()) { continue; }
		It->Hear(Where, Radius, Source);
	}
}

void UAlertnessComponent::Hear(const FVector& Where, float Radius, AActor* Source)
{
	AActor* O = GetOwner();
	if (!O || Source == O || Radius <= 0.0f || DeadOrGone(O)) { return; }
	const float Dist = (float)FVector::Dist(O->GetActorLocation(), Where);
	if (Dist > Radius) { return; }
	const float Close = 1.0f - Dist / Radius;
	Awareness = FMath::Min(AlertAt * 1.5f, Awareness + 0.35f + 0.9f * Close);   // a shot right beside you is not a suspicion
	LastKnown = Where; bHasLastKnown = true;
	if (Awareness >= AlertAt && Source) { Target = Source; }
	if (State == EAlertState::Calm && Awareness >= SuspiciousAt) { SetState(EAlertState::Suspicious); }
	if (State != EAlertState::Alert && Awareness >= AlertAt) { SetState(EAlertState::Alert); }
}

void UAlertnessComponent::NoteDamage(AActor* Who, float Amount)
{
	if (DeadOrGone(GetOwner())) { return; }
	if (Who) { Threat.FindOrAdd(Who) += FMath::Max(1.0f, Amount); Target = Who; LastKnown = Who->GetActorLocation(); bHasLastKnown = true; }
	Awareness = AlertAt * 1.5f;
	if (GetWorld()) { LastSeenAt = GetWorld()->GetTimeSeconds(); }
	SetState(EAlertState::Alert);
}

void UAlertnessComponent::ForgetTarget()
{
	Target = nullptr; Threat.Reset(); Awareness = 0.0f; bHasLastKnown = false;
}

void UAlertnessComponent::SetState(EAlertState New)
{
	if (New == State) { return; }
	const EAlertState Old = State;
	State = New;
	StateSince = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	OnStateChanged.Broadcast(Old, New);
}
