#include "CombatAnimLibrary.h"
#include "Animation/AnimSequence.h"

void FCombatAnimLibrary::Build(const TArray<UAnimSequence*>& Clips)
{
	Infos.Reset();
	KeyToIndex.Reset();

	static const TArray<FString> Phases = { TEXT("ReturnToIdle"), TEXT("ReturnToBlock"), TEXT("Begin"), TEXT("Loop"), TEXT("End"), TEXT("Pose"), TEXT("React"), TEXT("Stagger"), TEXT("Break") };

	for (UAnimSequence* Clip : Clips)
	{
		if (!Clip) { continue; }
		FCombatClipInfo Info;
		Info.Clip = Clip;

		FString Name = Clip->GetName();
		Name.RemoveFromStart(TEXT("A_"));
		Name.RemoveFromEnd(TEXT("_Sword"));
		Info.Key = Name;

		TArray<FString> Tokens;
		Name.ParseIntoArray(Tokens, TEXT("_"));
		if (Tokens.Num() == 0) { continue; }
		Info.Category = Tokens[0];
		for (const FString& Token : Tokens)
		{
			if (Token == TEXT("F") || Token == TEXT("B") || Token == TEXT("L") || Token == TEXT("R")) { Info.Direction = Token; }
			else if (Token == TEXT("Masc") || Token == TEXT("Femn")) { Info.Gender = Token; }
			else if (Token.StartsWith(TEXT("RootMotion"))) { Info.bRootMotion = true; }
			else if (Phases.Contains(Token)) { Info.Phase = Token; }
		}

		KeyToIndex.Add(Info.Key, Infos.Num());
		Infos.Add(MoveTemp(Info));
	}
}

UAnimSequence* FCombatAnimLibrary::Find(const FString& Key) const
{
	const int32* Index = KeyToIndex.Find(Key);
	return Index ? Infos[*Index].Clip : nullptr;
}

TArray<FString> FCombatAnimLibrary::Keys() const
{
	TArray<FString> Out;
	KeyToIndex.GetKeys(Out);
	Out.Sort();
	return Out;
}

FString FCombatAnimLibrary::Describe() const
{
	// Group the in-place clips (RootMotion siblings are noted as a count)
	// by category, then compact what the names say about them.
	TMap<FString, TArray<const FCombatClipInfo*>> ByCategory;
	int32 NumRootMotion = 0;
	for (const FCombatClipInfo& Info : Infos)
	{
		if (Info.bRootMotion) { ++NumRootMotion; continue; }
		ByCategory.FindOrAdd(Info.Category).Add(&Info);
	}

	TArray<FString> Categories;
	ByCategory.GetKeys(Categories);
	Categories.Sort();

	FString Out;
	Out += FString::Printf(TEXT("%d clips (%d in-place + %d RootMotion siblings)\n"), Infos.Num(), Infos.Num() - NumRootMotion, NumRootMotion);
	for (const FString& Category : Categories)
	{
		const TArray<const FCombatClipInfo*>& Group = ByCategory[Category];
		Out += FString::Printf(TEXT("%s (%d):"), *Category, Group.Num());

		if (Category == TEXT("Attack"))
		{
			// Pair each attack with its ReturnToIdle recovery and spot combos.
			TMap<FString, bool> Attacks;   // base key -> has ReturnToIdle
			for (const FCombatClipInfo* Info : Group)
			{
				FString Base = Info->Key;
				const bool bReturn = Base.RemoveFromEnd(TEXT("_ReturnToIdle"));
				Base.RemoveFromStart(TEXT("Attack_"));
				bool& HasReturn = Attacks.FindOrAdd(Base);
				HasReturn |= bReturn;
			}
			TArray<FString> Names; Attacks.GetKeys(Names); Names.Sort();
			TMap<FString, FString> Combos;
			TArray<FString> Singles;
			for (const FString& N : Names)
			{
				const TCHAR Last = N.Len() > 0 ? N[N.Len() - 1] : TEXT(' ');
				if (N.Contains(TEXT("Combo")) && FChar::IsAlpha(Last) && FChar::IsUpper(Last))
				{
					FString Family = N.LeftChop(1);
					Combos.FindOrAdd(Family) += FString::Chr(Last);
				}
				else { Singles.Add(N); }
			}
			Out += TEXT("\n  every attack has its own _ReturnToIdle recovery; combo steps chain A->B->C");
			for (const TPair<FString, FString>& C : Combos)
			{
				Out += FString::Printf(TEXT("\n  combo %s: steps %s"), *C.Key, *C.Value);
			}
			Out += TEXT("\n  singles: ") + FString::Join(Singles, TEXT(", "));
		}
		else
		{
			TArray<FString> Keys;
			for (const FCombatClipInfo* Info : Group) { Keys.Add(Info->Key); }
			Keys.Sort();
			Out += TEXT("\n  ") + FString::Join(Keys, TEXT(", "));
		}
		Out += TEXT("\n");
	}
	return Out;
}
