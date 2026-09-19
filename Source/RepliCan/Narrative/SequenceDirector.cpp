#include "Narrative/SequenceDirector.h"
#include "Characters/BaseCharacter.h"
#include "Core/BasePlayerController.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Camera/CameraActor.h"
#include "Camera/PlayerCameraManager.h"
#include "UI/BlinkOverlayWidget.h"
#include "World/AmbientPlayer.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"

bool USequenceDirector::Start(ABasePlayerController* InController, const FSequence& InSequence)
{
	if (!InController || InSequence.Steps.Num() == 0) { return false; }
	Controller = InController;
	Sequence = InSequence;
	Actors.Reset();
	bActive = true;
	bEnding = false;
	Controller->BeginCinematic();
	// Black until the first step that wants the screen (a "blink" overlay
	// or a "fade"), so nothing of the set shows before the script says so.
	if (Controller->PlayerCameraManager) { Controller->PlayerCameraManager->SetManualCameraFade(1.0f, FLinearColor::Black, false); }
	SpawnActors();
	UE_LOG(LogTemp, Log, TEXT("Sequence: '%s' started (%d steps, %d actors)"), *Sequence.Name, Sequence.Steps.Num(), Actors.Num());
	BeginStep(0);
	return true;
}

void USequenceDirector::SpawnActors()
{
	UWorld* World = Controller->GetWorld();
	APawn* Player = Controller->GetPawn();
	for (const FSequenceActor& Def : Sequence.Actors)
	{
		// A character already in the level with that config name is used as is.
		for (TActorIterator<ABaseCharacter> It(World); It; ++It)
		{
			if (*It != Player && It->MatchesConfigName(Def.Character)) { Actors.Add(Def.Id, *It); break; }
		}
		if (Actors.Contains(Def.Id)) { continue; }

		FVector Base = FVector::ZeroVector;
		FRotator BaseRot = FRotator::ZeroRotator;
		if (Def.RelativeTo == TEXT("player") && Player) { Base = Player->GetActorLocation(); BaseRot = FRotator(0, Player->GetActorRotation().Yaw, 0); }
		else if (Def.RelativeTo != TEXT("world")) { if (ABaseCharacter* Other = FindActor(Def.RelativeTo)) { Base = Other->GetActorLocation(); BaseRot = FRotator(0, Other->GetActorRotation().Yaw, 0); } }
		FVector Location = Base + BaseRot.RotateVector(Def.Offset);

		// Put the capsule on the floor under that point.
		const ACharacter* Default = ABaseCharacter::StaticClass()->GetDefaultObject<ACharacter>();
		const float HalfHeight = Default && Default->GetCapsuleComponent() ? Default->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 88.0f;
		if (Def.RelativeTo == TEXT("world"))
		{
			// World placement gives the floor height itself; no trace, so a prop
			// standing on that spot can't lift the character onto its top.
			Location.Z += HalfHeight + 2.0f;
		}
		else
		{
			FHitResult Floor;
			FCollisionQueryParams Query(SCENE_QUERY_STAT(SequenceSpawn), false);
			if (Player) { Query.AddIgnoredActor(Player); }
			if (World->LineTraceSingleByChannel(Floor, Location + FVector(0, 0, 150), Location - FVector(0, 0, 400), ECC_Visibility, Query))
			{
				Location.Z = Floor.ImpactPoint.Z + HalfHeight + 2.0f;
			}
		}
		FRotator Rotation(0, BaseRot.Yaw + Def.Yaw, 0);
		if (Def.bFacePlayer && Player)
		{
			Rotation = (Player->GetActorLocation() - Location).GetSafeNormal2D().Rotation();
		}

		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
		ABaseCharacter* Spawned = World->SpawnActorDeferred<ABaseCharacter>(ABaseCharacter::StaticClass(), FTransform(Rotation, Location), nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);
		if (!Spawned) { continue; }
		Spawned->DefaultCharacterConfigName = Def.Character;
		Spawned->bRuntimeSpawned = true;
		Spawned->FinishSpawning(FTransform(Rotation, Location));
#if WITH_EDITOR
		Spawned->SetActorLabel(Def.Character);
#endif
		Actors.Add(Def.Id, Spawned);
	}
}

ABaseCharacter* USequenceDirector::FindActor(const FString& Id) const
{
	if (const TObjectPtr<ABaseCharacter>* Found = Actors.Find(Id)) { return Found->Get(); }
	return nullptr;
}

bool USequenceDirector::ResolveAnchor(const FString& Anchor, const FVector& Offset, FVector& Out) const
{
	if (Anchor == TEXT("world")) { Out = Offset; return true; }
	if (Anchor.StartsWith(TEXT("world:")))
	{
		TArray<FString> Parts;
		Anchor.Mid(6).ParseIntoArray(Parts, TEXT(","), true);
		if (Parts.Num() == 3) { Out = FVector(FCString::Atof(*Parts[0]), FCString::Atof(*Parts[1]), FCString::Atof(*Parts[2])) + Offset; return true; }
		return false;
	}
	FString Id = Anchor, Part;
	Anchor.Split(TEXT("."), &Id, &Part);
	AActor* Actor = nullptr;
	if (Id == TEXT("player")) { Actor = Controller->GetPawn(); }
	else { Actor = FindActor(Id); }
	if (!Actor) { return false; }

	FVector Origin = Actor->GetActorLocation();
	if (Part == TEXT("head"))
	{
		if (const ACharacter* C = Cast<ACharacter>(Actor))
		{
			if (C->GetMesh() && C->GetMesh()->DoesSocketExist(TEXT("head"))) { Origin = C->GetMesh()->GetSocketLocation(TEXT("head")); }
			else { Origin += FVector(0, 0, 70); }
		}
	}
	const FRotator Frame(0, Actor->GetActorRotation().Yaw, 0);
	Out = Origin + Frame.RotateVector(Offset);
	return true;
}

FString USequenceDirector::SpeakerName(const FString& Who) const
{
	if (Who == TEXT("player")) { return TEXT("You"); }
	if (const ABaseCharacter* C = FindActor(Who)) { return C->GetCharacterConfig().Name; }
	return Who;
}

void USequenceDirector::BeginStep(int32 Index)
{
	if (!bActive) { return; }
	if (!Sequence.Steps.IsValidIndex(Index)) { FinishSequence(1.0f); return; }
	StepIndex = Index;
	StepElapsed = 0.0f;
	StepDuration = 0.0f;
	const FSequenceStep& Step = Sequence.Steps[Index];
	APlayerCameraManager* Camera = Controller->PlayerCameraManager;
	if (bSkipping)
	{
		if (Index >= SkipTarget) { bSkipping = false; }
		else
		{
			// Fast-forward: timed and spoken steps are dropped, moves land at once, turns snap;
			// the state steps (blink, look, ambient, place, panel, remote) fall through and run.
			switch (Step.Type)
			{
			case ESequenceStepType::Wait: case ESequenceStepType::Say: case ESequenceStepType::Choice: case ESequenceStepType::Shock:
			case ESequenceStepType::Sound: case ESequenceStepType::WaitBlinks: case ESequenceStepType::Fade: case ESequenceStepType::Appearance:
				BeginStep(Index + 1); return;
			case ESequenceStepType::Move:
				bMoving = false; LookEye = Step.Position; SwayRoll = 0.0f;
				BeginStep(Index + 1); return;
			case ESequenceStepType::Turn:
				if (AActor* A = FindActor(Step.Who)) { if (const float* Yaw = Step.Numbers.Find(TEXT("yaw"))) { FRotator Rot = A->GetActorRotation(); Rot.Yaw = *Yaw; A->SetActorRotation(Rot); } }
				BeginStep(Index + 1); return;
			default: break;
			}
		}
	}

	switch (Step.Type)
	{
	case ESequenceStepType::Fade:
		if (Camera) { Camera->StartCameraFade(Step.FadeFrom, Step.FadeTo, Step.Seconds, FLinearColor::Black, false, Step.FadeTo >= 1.0f); }
		StepDuration = Step.Seconds;
		break;

	case ESequenceStepType::Camera:
	{
		LookStepIndex = INDEX_NONE;   // a fixed shot replaces any background look
		FVector Eye, LookAt;
		if (ResolveAnchor(Step.Shot.Anchor, Step.Shot.Offset, Eye) && ResolveAnchor(Step.Shot.LookAt, Step.Shot.LookOffset, LookAt))
		{
			Controller->SetCinematicCamera(Eye, LookAt, Step.Shot.Fov, Step.Shot.Blend);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("Sequence: step %d camera anchor '%s'/'%s' not found"), Index, *Step.Shot.Anchor, *Step.Shot.LookAt);
		}
		StepDuration = FMath::Max(0.01f, Step.Shot.Blend);
		break;
	}

	case ESequenceStepType::Say:
	{
		const bool bPlayer = Step.Who == TEXT("player");
		ABaseCharacter* Speaker = bPlayer ? nullptr : FindActor(Step.Who);
		Controller->ShowConversationPanel(SpeakerName(Step.Who));
		Controller->ConversationSay(SpeakerName(Step.Who), Step.Text.Replace(TEXT("{name}"), *Controller->GetPlayerDisplayName()), bPlayer);   // {name}: the chosen name
		// A "continue" button unless the next step is the player's reply.
		// (looking past labels and unconditional gotos, so a branch's reply is still on offer at once).
		int32 Next = Index + 1;
		for (int32 Guard = 0; Guard < 8 && Sequence.Steps.IsValidIndex(Next); ++Guard)
		{
			const FSequenceStep& N = Sequence.Steps[Next];
			if (N.Type == ESequenceStepType::Label) { ++Next; continue; }
			if (N.Type == ESequenceStepType::Goto && N.Condition.IsEmpty()) { const int32 T = Sequence.FindLabel(N.Label); if (T == INDEX_NONE) { break; } Next = T + 1; continue; }
			break;
		}
		const bool bReplyFollows = Sequence.Steps.IsValidIndex(Next) && Sequence.Steps[Next].Type == ESequenceStepType::Choice;
		Controller->SetConversationChoices(bReplyFollows ? TArray<FString>() : TArray<FString>{ TEXT("> continue") });
		float Voice = 0.0f;
		if (Speaker)
		{
			if (APawn* P = Controller->GetPawn()) { Speaker->SetLookAtTarget(P->GetActorLocation() + FVector(0, 0, 60.0f)); }
			Voice = Controller->PlayVoiceFile(Speaker, SequenceFile::GetVoicePath(Sequence.Name, Index));
		}
		if (bReplyFollows)
		{
			// The reply is on offer at once, while the line is still being
			// spoken; answering early cuts the voice (the next line stops it).
			BeginStep(Index + 1);
			return;
		}
		const float Reading = 1.2f + 0.045f * Step.Text.Len();
		StepDuration = Step.bWaitForInput ? 0.0f : (Step.Seconds > 0.0f ? Step.Seconds : (Voice > 0.0f ? Voice + 0.6f : Reading));
		break;
	}

	case ESequenceStepType::Choice:
	{
		TArray<FString> Texts;
		for (const FSequenceOption& O : Step.Options) { Texts.Add(O.Text); }
		Controller->ShowConversationPanel(FString());
		Controller->SetConversationChoices(Texts);
		StepDuration = 0.0f;   // waits for OnChoice
		break;
	}

	case ESequenceStepType::Wait:
		StepDuration = FMath::Max(0.01f, Step.Seconds);
		break;

	case ESequenceStepType::Label:
		BeginStep(Index + 1);
		return;

	case ESequenceStepType::Goto:
	{
		if (!ConditionHolds(Step.Condition)) { BeginStep(Index + 1); return; }
		const int32 Target = Sequence.FindLabel(Step.Label);
		if (Target == INDEX_NONE) { UE_LOG(LogTemp, Warning, TEXT("Sequence: label '%s' not found"), *Step.Label); }
		BeginStep(Target == INDEX_NONE ? Index + 1 : Target + 1);
		return;
	}

	case ESequenceStepType::Look:
	{
		const bool bContinuing = LookStepIndex != INDEX_NONE;   // keep the current gaze, retarget smoothly
		if (!ResolveAnchor(Step.Shot.Anchor, Step.Shot.Offset, LookEye)) { LookEye = Controller->GetPawn() ? Controller->GetPawn()->GetActorLocation() + FVector(0, 0, 60) : FVector::ZeroVector; }
		LookIndex = 0;
		FVector First = LookEye + FVector(100, 0, 0);
		if (Step.LookTargets.Num() > 0) { ResolveAnchor(Step.LookTargets[0], FVector::ZeroVector, First); }
		if (!bContinuing)
		{
			LookRotation = (First - LookEye).Rotation();
			Controller->SetCinematicCamera(LookEye, First, Step.Shot.Fov, 0.0f);
		}
		LookStepIndex = Index;
		LookElapsed = 0.0f;
		if (Step.Seconds <= 0.0f) { BeginStep(Index + 1); return; }   // background look
		StepDuration = Step.Seconds;
		break;
	}

	case ESequenceStepType::WaitBlinks:
		StepDuration = 0.0f;   // Tick releases it once enough blinks have happened
		break;

	case ESequenceStepType::Sound:
	{
		const float* Vol = Step.Numbers.Find(TEXT("volume"));
		const float* Pitch = Step.Numbers.Find(TEXT("pitch"));
		UAmbientPlayer::PlayOneShot(this, Controller->GetWorld(), Step.Text, Vol ? *Vol : 1.0f, Pitch ? *Pitch : 1.0f);
		BeginStep(Index + 1);
		return;
	}

	case ESequenceStepType::Move:
	{
		bMoving = true;
		MoveFrom = LookEye;
		MoveTo = Step.Position;
		MoveSeconds = FMath::Max(0.1f, Step.Seconds);
		MoveElapsed = 0.0f;
		const float* Sway = Step.Numbers.Find(TEXT("sway"));
		MoveSway = Sway ? *Sway : 1.0f;
		StepDuration = MoveSeconds;
		break;
	}

	case ESequenceStepType::Ambient:
	{
		const float* Vol = Step.Numbers.Find(TEXT("volume"));
		Controller->FadeAmbient(Vol ? *Vol : 1.0f, FMath::Max(0.05f, Step.Seconds));
		BeginStep(Index + 1);
		return;
	}

	case ESequenceStepType::Turn:
	{
		TurnActor = FindActor(Step.Who);
		if (TurnActor.IsValid())
		{
			TurnFromYaw = TurnActor->GetActorRotation().Yaw;
			const float* Yaw = Step.Numbers.Find(TEXT("yaw"));
			TurnToYaw = TurnFromYaw + FMath::FindDeltaAngleDegrees(TurnFromYaw, Yaw ? *Yaw : TurnFromYaw);
			TurnSeconds = FMath::Max(0.1f, Step.Seconds); TurnElapsed = 0.0f;
		}
		StepDuration = FMath::Max(0.1f, Step.Seconds);
		break;
	}

	case ESequenceStepType::Appearance:
	{
		Controller->BeginAppearance();
		StepDuration = 0.0f;   // Tick releases it once the likeness is accepted
		break;
	}

	case ESequenceStepType::Panel:
	{
		if (Step.Text == TEXT("hide")) { Controller->HideConversationPanel(); }
		BeginStep(Index + 1);
		return;
	}

	case ESequenceStepType::Remote:
	{
		if (Step.bFlag) { Controller->HideRemoteView(); }
		else if (ABaseCharacter* Who = FindActor(Step.Who)) { Controller->ShowRemoteView(Who); }
		else { Controller->ShowRemoteCall(Step.Who); }   // not one of the sequence's actors: a config name, called from elsewhere
		BeginStep(Index + 1);
		return;
	}

	case ESequenceStepType::Shock:
	{
		const float* Vol = Step.Numbers.Find(TEXT("volume"));
		const float* Pitch = Step.Numbers.Find(TEXT("pitch"));
		UAmbientPlayer::PlayOneShot(this, Controller->GetWorld(), Step.Text, Vol ? *Vol : 1.0f, Pitch ? *Pitch : 1.0f);
		StepDuration = FMath::Max(0.05f, Step.Seconds);   // the hum builds; the jolt fires as the step ends (Tick)
		break;
	}

	case ESequenceStepType::Blink:
	{
		if (Step.bFlag) { Controller->HideBlinkOverlay(); }
		else
		{
			FBlinkParams P;
			auto Get = [&Step](const TCHAR* Key, float Default) { const float* V = Step.Numbers.Find(Key); return V ? *V : Default; };
			P.OpenMin = Get(TEXT("openMin"), P.OpenMin);
			P.OpenMax = Get(TEXT("openMax"), P.OpenMax);
			P.ClosedSeconds = Get(TEXT("closedSeconds"), P.ClosedSeconds);
			P.OpenSeconds = Get(TEXT("openSeconds"), P.OpenSeconds);
			P.DarkStart = Get(TEXT("darkStart"), P.DarkStart);
			P.DarkEnd = Get(TEXT("darkEnd"), P.DarkEnd);
			P.DarkSeconds = Get(TEXT("darkSeconds"), P.DarkSeconds);
			P.LiftSeconds = Get(TEXT("lift"), P.LiftSeconds);
			Controller->ShowBlinkOverlay(P);
			// The eyelids now cover the screen, so any hold-black camera fade
			// (set when the level was entered) can go.
			if (Camera) { Camera->StopCameraFade(); }
		}
		BeginStep(Index + 1);
		return;
	}

	case ESequenceStepType::Place:
	{
		AActor* Target = Step.Who == TEXT("player") ? static_cast<AActor*>(Controller->GetPawn()) : static_cast<AActor*>(FindActor(Step.Who));
		if (Target)
		{
			if (Step.Who == TEXT("player") && !bPlayerMoved)
			{
				bPlayerMoved = true;
				PlayerOriginal = Target->GetActorTransform();
				bPlayerWasHidden = Target->IsHidden();
				bPlayerHadCollision = Target->GetActorEnableCollision();
			}
			const float* Yaw = Step.Numbers.Find(TEXT("yaw"));
			Target->SetActorEnableCollision(!Step.bFlag && (Step.Who != TEXT("player") || bPlayerHadCollision));
			Target->SetActorLocationAndRotation(Step.Position, FRotator(0, Yaw ? *Yaw : 0.0f, 0), false, nullptr, ETeleportType::TeleportPhysics);
			Target->SetActorHiddenInGame(Step.bFlag);
		}
		BeginStep(Index + 1);
		return;
	}

	case ESequenceStepType::Release:
	{
		// Stand the pawn where the look camera is, facing where it looks, and hand over.
		if (APawn* P = Controller->GetPawn())
		{
			FVector Feet = LookEye;
			Feet.Z = bPlayerMoved ? PlayerOriginal.GetLocation().Z : P->GetActorLocation().Z;
			P->SetActorHiddenInGame(false);
			P->SetActorEnableCollision(true);
			P->SetActorLocationAndRotation(Feet, FRotator(0.0f, LookRotation.Yaw, 0.0f), false, nullptr, ETeleportType::TeleportPhysics);
			if (ACharacter* C = Cast<ACharacter>(P))
			{
				// Hidden without collision it has been falling for the whole intro: kill that and stand it up.
				C->GetCharacterMovement()->StopMovementImmediately();
				C->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
			}
			Controller->SetControlRotation(FRotator(0.0f, LookRotation.Yaw, 0.0f));
			bPlayerMoved = false;   // and stays there: FinishSequence must not put it back
		}
		Controller->HideConversationPanel();
		bKeepBlink = true;
		Controller->BeginGroggy(Step.Position, Step.Numbers.FindRef(TEXT("radius")), Step.Numbers.FindRef(TEXT("speed")));
		FinishSequence(0.7f);
		return;
	}

	case ESequenceStepType::End:
		bEnding = true;
		Controller->HideConversationPanel();
		if (Step.Seconds > 0.0f && Camera) { Camera->StartCameraFade(0.0f, 1.0f, Step.Seconds, FLinearColor::Black, false, true); }
		StepDuration = FMath::Max(0.01f, Step.Seconds);
		break;
	}
}

void USequenceDirector::TickLook(float DeltaSeconds)
{
	if (!Sequence.Steps.IsValidIndex(LookStepIndex)) { return; }
	const FSequenceStep& Step = Sequence.Steps[LookStepIndex];
	ACameraActor* Camera = Controller->GetCinematicCamera();
	if (!Camera || Step.LookTargets.Num() == 0) { return; }
	LookElapsed += DeltaSeconds;
	// Which target now: one per period. Ease toward it with a slow, heavy
	// turn, plus a faint wander so the gaze never sits perfectly still.
	const int32 Want = FMath::FloorToInt(LookElapsed / FMath::Max(0.5f, Step.LookPeriod)) % Step.LookTargets.Num();
	LookIndex = Want;
	FVector Target;
	if (!ResolveAnchor(Step.LookTargets[LookIndex], FVector::ZeroVector, Target)) { return; }
	const float* Bias = Step.Numbers.Find(TEXT("yawBias"));
	const FRotator Desired = (Target - LookEye).Rotation() + FRotator(0, Bias ? *Bias : 0.0f, 0);
	const float* TurnSpeed = Step.Numbers.Find(TEXT("turnSpeed"));
	const float* WanderScale = Step.Numbers.Find(TEXT("wander"));
	LookRotation = FMath::RInterpTo(LookRotation, Desired, DeltaSeconds, TurnSpeed ? FMath::Max(0.05f, *TurnSpeed) : 1.1f);
	const float T = LookElapsed;
	const float Ws = WanderScale ? *WanderScale : 1.0f;
	const FRotator Wander(Ws * 1.2f * FMath::Sin(T * 0.7f), Ws * 1.6f * FMath::Sin(T * 0.53f + 1.0f), Ws * 0.8f * FMath::Sin(T * 0.31f) + SwayRoll);
	// The spasm: a head-thrown-back kick that rings down into a tremor.
	FRotator Shake = FRotator::ZeroRotator;
	FVector Jitter = FVector::ZeroVector;
	if (ShakeLeft > 0.0f && ShakeTotal > 0.0f)
	{
		const float Decay = ShakeLeft / ShakeTotal;
		const float D3 = Decay * Decay * Decay;
		const float S = ShakeClock;
		Shake.Pitch = 11.0f * D3 * FMath::Sin(S * 21.0f) + 5.0f * Decay * FMath::Sin(S * 47.0f + 0.7f);
		Shake.Yaw = 4.0f * Decay * FMath::Sin(S * 39.0f + 1.9f) + 2.0f * Decay * FMath::Sin(S * 71.0f);
		Shake.Roll = 6.0f * D3 * FMath::Sin(S * 29.0f + 2.4f);
		Jitter = FVector(2.0f * FMath::Sin(S * 53.0f), 3.0f * Decay * FMath::Sin(S * 61.0f), 4.0f * D3 * FMath::Sin(S * 44.0f));
	}
	Camera->SetActorLocationAndRotation(LookEye + Jitter, LookRotation + Wander + Shake);
}

void USequenceDirector::FireJolt(const FSequenceStep& Step)
{
	const float* Spasm = Step.Numbers.Find(TEXT("spasm"));
	ShakeTotal = ShakeLeft = Spasm ? *Spasm : 0.8f;
	ShakeClock = 0.0f;
	Controller->ShockBlinkOverlay(ShakeTotal);
}

void USequenceDirector::Tick(float DeltaSeconds)
{
	if (!bActive || !Sequence.Steps.IsValidIndex(StepIndex)) { return; }
	StepElapsed += DeltaSeconds;
	if (ShakeLeft > 0.0f) { ShakeLeft -= DeltaSeconds; ShakeClock += DeltaSeconds; }
	if (TurnActor.IsValid() && TurnElapsed < TurnSeconds)
	{
		TurnElapsed += DeltaSeconds;
		const float A = FMath::Clamp(TurnElapsed / TurnSeconds, 0.0f, 1.0f);
		const float E = A * A * (3.0f - 2.0f * A);
		FRotator R = TurnActor->GetActorRotation(); R.Yaw = FMath::Lerp(TurnFromYaw, TurnToYaw, E);
		TurnActor->SetActorRotation(R);
	}
	if (bMoving)
	{
		// Ease along the path; sway sideways, bob at a walking cadence, roll a
		// little: someone upright for the first time, not a dolly.
		MoveElapsed += DeltaSeconds;
		const float A = FMath::Clamp(MoveElapsed / MoveSeconds, 0.0f, 1.0f);
		const float E = A * A * (3.0f - 2.0f * A);
		const FVector Dir = (MoveTo - MoveFrom).GetSafeNormal2D();
		const FVector Side = FVector::CrossProduct(Dir, FVector::UpVector);
		const float Env = FMath::Sin(A * PI);   // sway strongest mid-move
		const float S = MoveElapsed;
		LookEye = FMath::Lerp(MoveFrom, MoveTo, E)
			+ Side * (7.0f * MoveSway * Env * FMath::Sin(S * 1.9f))
			+ FVector::UpVector * (3.5f * MoveSway * Env * FMath::Sin(S * 3.6f));
		SwayRoll = 3.0f * MoveSway * Env * FMath::Sin(S * 1.3f + 0.4f);
		if (A >= 1.0f) { bMoving = false; LookEye = MoveTo; SwayRoll = 0.0f; }
	}
	if (LookStepIndex != INDEX_NONE) { TickLook(DeltaSeconds); }
	if (Sequence.Steps[StepIndex].Type == ESequenceStepType::Appearance && !Controller->IsAppearanceOpen())
	{
		BeginStep(StepIndex + 1);
		return;
	}
	if (Sequence.Steps[StepIndex].Type == ESequenceStepType::WaitBlinks && Controller->GetBlinkCount() >= FMath::RoundToInt(Sequence.Steps[StepIndex].Seconds))
	{
		BeginStep(StepIndex + 1);
		return;
	}
	if (StepDuration > 0.0f && StepElapsed >= StepDuration)
	{
		if (bEnding) { FinishSequence(0.0f); return; }
		if (Sequence.Steps[StepIndex].Type == ESequenceStepType::Shock) { FireJolt(Sequence.Steps[StepIndex]); }
		BeginStep(StepIndex + 1);
	}
}

int32 USequenceDirector::NextSkipTarget() const
{
	int32 Best = INDEX_NONE;
	for (const FString& L : Sequence.SkipLabels)
	{
		const int32 At = Sequence.FindLabel(L);
		if (At > StepIndex && (Best == INDEX_NONE || At < Best)) { Best = At; }
	}
	return Best;
}

bool USequenceDirector::CanSkipAhead() const
{
	if (!bActive || bSkipping || !Sequence.Steps.IsValidIndex(StepIndex)) { return false; }
	const ESequenceStepType T = Sequence.Steps[StepIndex].Type;
	if (T == ESequenceStepType::Say || T == ESequenceStepType::Choice || T == ESequenceStepType::Appearance) { return false; }
	return NextSkipTarget() != INDEX_NONE;
}

void USequenceDirector::SkipAhead()
{
	if (!CanSkipAhead()) { return; }
	SkipTarget = NextSkipTarget();
	bSkipping = true;
	if (bMoving) { bMoving = false; LookEye = MoveTo; SwayRoll = 0.0f; }
	BeginStep(StepIndex + 1);
}

bool USequenceDirector::ConditionHolds(const FString& Condition) const
{
	FString C = Condition.TrimStartAndEnd();
	if (C.IsEmpty()) { return true; }
	bool bWant = true;
	if (C.StartsWith(TEXT("!"))) { bWant = false; C = C.Mid(1).TrimStartAndEnd(); }
	bool bHolds = false;
	FString Value;
	if (C.StartsWith(TEXT("name")) && C.Split(TEXT("="), nullptr, &Value))
	{
		const FString Name = Controller ? Controller->GetPlayerDisplayName() : FString();
		bHolds = Name.TrimStartAndEnd().Equals(Value.TrimStartAndEnd(), ESearchCase::IgnoreCase);
	}
	else { bHolds = Controller && Controller->ConversationFlags.Contains(C); }
	return bHolds == bWant;
}

void USequenceDirector::OnChoice(int32 Index)
{
	if (!bActive || !Sequence.Steps.IsValidIndex(StepIndex)) { return; }
	const FSequenceStep& Step = Sequence.Steps[StepIndex];
	if (Step.Type == ESequenceStepType::Say)
	{
		BeginStep(StepIndex + 1);
	}
	else if (Step.Type == ESequenceStepType::Choice && Step.Options.IsValidIndex(Index))
	{
		const FSequenceOption& O = Step.Options[Index];
		Controller->ConversationSay(TEXT("You"), O.Text, true);
		if (!O.SetFlag.IsEmpty()) { Controller->ConversationFlags.Add(O.SetFlag); }
		const int32 Target = O.Goto.IsEmpty() ? INDEX_NONE : Sequence.FindLabel(O.Goto);
		BeginStep(Target == INDEX_NONE ? StepIndex + 1 : Target + 1);
	}
}

void USequenceDirector::Advance()
{
	if (!bActive) { return; }
	if (bEnding) { FinishSequence(0.0f); return; }
	BeginStep(StepIndex + 1);
}

void USequenceDirector::Skip()
{
	if (!bActive) { return; }
	if (APlayerCameraManager* Camera = Controller->PlayerCameraManager) { Camera->StopCameraFade(); }
	FinishSequence(0.6f);
}

void USequenceDirector::FinishSequence(float CameraBlend)
{
	if (!bActive) { return; }
	bActive = false;
	LookStepIndex = INDEX_NONE;
	bMoving = false;
	SwayRoll = 0.0f;
	Controller->HideRemoteView();
	for (const TPair<FString, TObjectPtr<ABaseCharacter>>& Pair : Actors)
	{
		if (Pair.Value) { Pair.Value->ClearLookAtTarget(); }
	}
	if (!bKeepBlink) { Controller->HideBlinkOverlay(); }
	if (bPlayerMoved)
	{
		if (APawn* P = Controller->GetPawn())
		{
			P->SetActorHiddenInGame(bPlayerWasHidden);
			P->SetActorEnableCollision(bPlayerHadCollision);
			P->SetActorTransform(PlayerOriginal, false, nullptr, ETeleportType::TeleportPhysics);
		}
		bPlayerMoved = false;
	}
	Controller->EndCinematic(CameraBlend, bKeepBlink);
	bKeepBlink = false;
	if (bEnding)
	{
		// Faded to black at the end step: lift the black once control is back.
		if (APlayerCameraManager* Camera = Controller->PlayerCameraManager) { Camera->StartCameraFade(1.0f, 0.0f, 0.8f, FLinearColor::Black, false, false); }
	}
	UE_LOG(LogTemp, Log, TEXT("Sequence: '%s' finished"), *Sequence.Name);
}

FString USequenceDirector::Describe() const
{
	if (!bActive) { return TEXT("idle"); }
	const FSequenceStep* Step = Sequence.Steps.IsValidIndex(StepIndex) ? &Sequence.Steps[StepIndex] : nullptr;
	return FString::Printf(TEXT("%s step %d/%d %s"), *Sequence.Name, StepIndex, Sequence.Steps.Num(), Step ? *UEnum::GetValueAsString(Step->Type).RightChop(19) : TEXT("?"));
}
