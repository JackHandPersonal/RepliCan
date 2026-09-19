#include "Characters/Vitality.h"
#include "Characters/BaseCharacter.h"
#include "Characters/CharacterConfig.h"

UVitalityComponent* UVitalityComponent::FindOrAdd(AActor* Who)
{
	if (!Who) { return nullptr; }
	if (UVitalityComponent* Have = Who->FindComponentByClass<UVitalityComponent>()) { return Have; }
	UVitalityComponent* V = NewObject<UVitalityComponent>(Who, TEXT("Vitality"));
	V->RegisterComponent();
	if (const ABaseCharacter* Ch = Cast<ABaseCharacter>(Who)) { const FAttributes& A = Ch->GetAttributes(); V->Reset(PoolFor(A.Endurance, A.Brawn)); }
	else { V->Reset(PoolFor(50, 50)); }
	return V;
}

void UVitalityComponent::Reset(float Max)
{
	MaxVitality = FMath::Max(1.0f, Max); Vitality = MaxVitality;
	RegionDamage.Reset(); Injured.Reset(); Destroyed.Reset(); bDead = false; CorpseDamage = 0.0f;
}

UVitalityComponent::FOutcome UVitalityComponent::Apply(const FString& Region, float Raw, float Armour)
{
	FOutcome Out;
	float D = FMath::Max(0.0f, Raw - FMath::Max(0.0f, Armour));
	if (Region == TEXT("Head")) { D *= HeadFactor; }
	Out.Dealt = D;
	// A corpse keeps count: nothing left to take from the pool, but a limb can still come off.
	if (!bDead) { Vitality = FMath::Max(0.0f, Vitality - D); } else { CorpseDamage += D; }
	const FString Key = Region.IsEmpty() ? FString(TEXT("Torso")) : Region;
	float& Taken = RegionDamage.FindOrAdd(Key);
	Taken += D;
	if (IsLimb(Key) || Key == TEXT("Head"))
	{
		if (!Destroyed.Contains(Key))
		{
			bool bGone = false;
			if (bDead)
			{
				// A corpse: by force. One blow as hard as the limb is tough, or wear plus a finishing blow with half of it.
				const float Tough = Toughness(Key);
				bGone = D >= Tough || (Taken > MaxVitality * DestroyedFraction && D >= Tough * FinishFraction);
			}
			else { bGone = Taken > MaxVitality * DestroyedFraction; }
			if (bGone) { Destroyed.Add(Key); Injured.Add(Key); Out.bDestroyed = true; }
		}
		if (!Out.bDestroyed && !bDead && IsLimb(Key) && Taken > MaxVitality * InjuredFraction && !Injured.Contains(Key)) { Injured.Add(Key); Out.bInjured = true; }
	}
	// A head destroyed is a death, whatever the pool says.
	if (!bDead && (Vitality <= 0.0f || (Key == TEXT("Head") && Out.bDestroyed))) { Vitality = 0.0f; bDead = true; Out.bDied = true; }
	return Out;
}

float UVitalityComponent::Toughness(const FString& Region) const
{
	if (Region == TEXT("Head") || Region == TEXT("Neck")) { return MaxVitality * ToughHead; }
	if (Region == TEXT("ArmL") || Region == TEXT("ArmR")) { return MaxVitality * ToughArm; }
	if (Region == TEXT("LegL") || Region == TEXT("LegR")) { return MaxVitality * ToughLeg; }
	return 0.0f;
}

FString UVitalityComponent::Summary() const
{
	FString S = FString::Printf(TEXT("%.0f/%.0f%s"), Vitality, MaxVitality, bDead ? TEXT(" DEAD") : TEXT(""));
	for (const FString& R : Injured) { S += FString::Printf(TEXT("; %s %s"), *R, Destroyed.Contains(R) ? TEXT("destroyed") : TEXT("injured")); }
	return S;
}
