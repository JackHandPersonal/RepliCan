#include "Characters/WorkBot.h"
#include "World/AmbientPlayer.h"
#include "Core/BasePlayerController.h"
#include "Weapons/ShotReactions.h"
#include "Weapons/SparkFx.h"
#include "Characters/Vitality.h"
#include "Narrative/VoiceLines.h"
#include "Animation/AnimSequence.h"
#include "Components/AudioComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/PointLightComponent.h"
#include "Engine/PointLight.h"
#include "EngineUtils.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "HAL/FileManager.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/Paths.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundWave.h"
#include "TimerManager.h"

AWorkBot::AWorkBot()
{
	DefaultCharacterConfigName = TEXT("WorkBot");
	AIControllerClass = AWorkBotController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
	Alertness = CreateDefaultSubobject<UAlertnessComponent>(TEXT("Alertness"));
	Alertness->SightRange = 1300.0f; Alertness->SightFovDeg = 110.0f; Alertness->GainPerSecond = 0.8f;   // an old sensor
	Tags.AddUnique(TEXT("robot"));
}

void AWorkBot::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	// The zoom index the character starts on is first person, which hides a single-mesh head; a
	// machine is only ever seen from outside.
	if (GetMesh()) { GetMesh()->UnHideBoneByName(TEXT("head")); }
}

AWorkBotController::AWorkBotController()
{
	PrimaryActorTick.bCanEverTick = true;
}

void AWorkBotController::FindClearSpot()
{
	// The layout once put it inside a cage: every character has the same check now (ABaseCharacter::UnstickNow).
	if (AWorkBot* B = Bot()) { B->UnstickNow(); }
}

AWorkBot* AWorkBotController::Bot() const { return Cast<AWorkBot>(GetPawn()); }

void AWorkBotController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	AWorkBot* B = Cast<AWorkBot>(InPawn);
	if (!B) { return; }
	if (B->Alertness) { B->Alertness->OnStateChanged.AddUObject(this, &AWorkBotController::OnAlert); }
	// Its lines: whatever the bake left in the folder, sorted by the kind in the file name.
	const FString Dir = FPaths::Combine(FPaths::ProjectDir(), TEXT("Conversations"), TEXT("Voice"), B->BarkFolder);
	TArray<FString> Files;
	IFileManager::Get().FindFiles(Files, *FPaths::Combine(Dir, TEXT("*.wav")), true, false);
	for (const FString& F : Files)
	{
		const FString Full = FPaths::Combine(Dir, F);
		if (F.StartsWith(TEXT("status_"))) { StatusLines.Add(Full); }
		else if (F.StartsWith(TEXT("notice_"))) { NoticeLines.Add(Full); }
		else if (F.StartsWith(TEXT("alert_"))) { AlertLines.Add(Full); }
		else if (F.StartsWith(TEXT("resume_"))) { ResumeLines.Add(Full); }
	}
	UE_LOG(LogTemp, Log, TEXT("WorkBot: %d status, %d notice, %d alert, %d resume lines in %s"), StatusLines.Num(), NoticeLines.Num(), AlertLines.Num(), ResumeLines.Num(), *Dir);
	// A voice from a point in the room: full inside a couple of metres, gone fourteen out.
	Attenuation = NewObject<USoundAttenuation>(this);
	FSoundAttenuationSettings& A = Attenuation->Attenuation;
	A.bAttenuate = true; A.bSpatialize = true;
	A.AttenuationShape = EAttenuationShape::Sphere;
	A.AttenuationShapeExtents = FVector(180.0f);
	A.FalloffDistance = 1400.0f;
	NextBark = FMath::FRandRange(4.0f, 9.0f);
	B->SetWalkOnly(true);   // one gait: the walk, whatever the hurry
	FindClearSpot();
	PickPatrolGoal();
}

void AWorkBotController::PickPatrolGoal()
{
	AWorkBot* B = Bot(); if (!B) { return; }
	// Somewhere else in the box, far enough to be a walk: a few tries, then whatever came up.
	const FVector Here = B->GetActorLocation();
	for (int32 k = 0; k < 8; ++k)
	{
		Goal = FVector(FMath::FRandRange(B->PatrolMin.X, B->PatrolMax.X), FMath::FRandRange(B->PatrolMin.Y, B->PatrolMax.Y), Here.Z);
		if (FVector::Dist2D(Goal, Here) > 250.0f) { break; }
	}
	GoalTimeout = 8.0f + FVector::Dist2D(Goal, Here) / 60.0f;
	ModeClock = 0.0f; StuckClock = 0.0f; SteerDir = FVector::ZeroVector; DirectClearFor = 0.0f;   // a new goal starts from the heading it is given
}

void AWorkBotController::Steer(float DeltaSeconds, const FVector& To, float Speed)
{
	AWorkBot* B = Bot(); UWorld* World = GetWorld();
	if (!B || !World) { return; }
	const float Wanted = World->GetTimeSeconds() < StutterUntil ? Speed * 0.25f : Speed;   // the half-stop
	if (!FMath::IsNearlyEqual(CurrentSpeed, Wanted)) { CurrentSpeed = Wanted; B->SetSpeedMultiplier(Wanted); }
	const FVector Dir = (To - B->GetActorLocation()).GetSafeNormal2D();
	if (Dir.IsNearlyZero()) { return; }
	// A wall or a crate ahead: the way round, thirty degrees at a time, either side. A person in
	// the way is not an obstacle -- it is where it is going.
	const float R = B->GetCapsuleComponent() ? B->GetCapsuleComponent()->GetScaledCapsuleRadius() : 34.0f;
	const FVector Start = B->GetActorLocation() + FVector(0.0f, 0.0f, -20.0f);
	FCollisionQueryParams Q(SCENE_QUERY_STAT(WorkBotSteer), false, B);
	TArray<AActor*> Attached; B->GetAttachedActors(Attached, true, true); Q.AddIgnoredActors(Attached);
	auto Clear = [&](const FVector& D)
	{
		FHitResult H;
		if (World->SweepSingleByChannel(H, Start, Start + D * (R + 70.0f), FQuat::Identity, ECC_Visibility, FCollisionShape::MakeSphere(R * 0.8f), Q)) { return Cast<APawn>(H.GetActor()) != nullptr; }
		return true;
	};
	FVector Go = Dir; bool bFound = Clear(Dir);
	DetourHold = FMath::Max(0.0f, DetourHold - DeltaSeconds);
	// THE BUZZ. A sweep that clears one frame and catches the next flips the heading between
	// straight-on and thirty degrees off at tick rate, which reads as the bot vibrating on the
	// spot. Two things stop it. First: a detour is only abandoned once the way ahead has been
	// clear for a moment, not for a single frame.
	DirectClearFor = bFound ? DirectClearFor + DeltaSeconds : 0.0f;
	if (bFound && DetourHold <= 0.0f && DirectClearFor > 0.45f) { DetourSide = 0; }
	else if (bFound && DetourSide != 0) { bFound = false; }   // still going round the thing it went round
	// A side once chosen is kept for a second and a half, so the way round a crate is one way
	// round it; and the side tried first alternates by memory, not by the tick.
	for (int32 k = 1; !bFound && k <= 5; ++k)
	{
		const FVector L = Dir.RotateAngleAxis(30.0f * k, FVector::UpVector), Rr = Dir.RotateAngleAxis(-30.0f * k, FVector::UpVector);
		const bool bLeftFirst = DetourSide >= 0;
		const FVector& First = bLeftFirst ? L : Rr; const FVector& Second = bLeftFirst ? Rr : L;
		if (Clear(First)) { Go = First; bFound = true; if (DetourSide == 0) { DetourSide = bLeftFirst ? 1 : -1; DetourHold = 1.5f; } }
		else if (Clear(Second)) { Go = Second; bFound = true; if (DetourSide == 0) { DetourSide = bLeftFirst ? -1 : 1; DetourHold = 1.5f; } }
	}
	// Going nowhere for two seconds with a clear-looking way ahead: hung on a corner. Back off
	// and try the other side, and let the stuck clock change the goal after that.
	if (FVector::Dist2D(B->GetActorLocation(), ProgressFrom) > 25.0f) { ProgressFrom = B->GetActorLocation(); NoProgress = 0.0f; }
	else { NoProgress += DeltaSeconds; }
	if (NoProgress > 2.0f)
	{
		NoProgress = 0.0f; DetourSide = -DetourSide; DetourHold = 1.5f;
		B->AddMovementInput(-Dir, 1.0f); StuckClock += 1.0f;
		return;
	}
	if (!bFound)
	{
		// Boxed in on every side: push toward the goal anyway (the capsule slides along whatever it
		// is against, and if it is INSIDE something the movement resolves out of it) and let the
		// stuck clock change the goal.
		B->AddMovementInput(Dir, 1.0f);
		StuckClock += DeltaSeconds;
		return;
	}
	if (B->GetVelocity().Size2D() < 8.0f) { StuckClock += DeltaSeconds; } else { StuckClock = 0.0f; }
	// Second: the heading turns at a rate instead of snapping. Even correct angles, swapped every
	// frame, look like a shiver; a bot that can only turn so fast per second cannot shiver.
	if (SteerDir.IsNearlyZero()) { SteerDir = Go; }
	SteerDir = FMath::VInterpNormalRotationTo(SteerDir, Go, DeltaSeconds, 420.0f).GetSafeNormal2D();
	if (SteerDir.IsNearlyZero()) { SteerDir = Go; }
	B->AddMovementInput(SteerDir, 1.0f);
}

void AWorkBotController::Look(float DeltaSeconds)
{
	AWorkBot* B = Bot(); if (!B) { return; }
	const FRotator R = B->GetActorRotation();
	B->SetActorRotation(FRotator(0.0f, FMath::FixedTurn(R.Yaw, LookYawTarget, 45.0f * DeltaSeconds), 0.0f));
}

void AWorkBotController::FaceToward(const FVector& Point, float DeltaSeconds)
{
	AWorkBot* B = Bot(); if (!B) { return; }
	const FVector D = (Point - B->GetActorLocation()).GetSafeNormal2D();
	if (D.IsNearlyZero()) { return; }
	B->SetActorRotation(FRotator(0.0f, FMath::FixedTurn(B->GetActorRotation().Yaw, D.Rotation().Yaw, 120.0f * DeltaSeconds), 0.0f));
}

void AWorkBotController::Say(const TArray<FString>& Lines, float Volume)
{
	AWorkBot* B = Bot();
	if (!B || Lines.Num() == 0 || !B->GetMesh()) { return; }
	if (Voice && Voice->IsPlaying()) { return; }   // one thing at a time
	const FString& Path = Lines[FMath::RandRange(0, Lines.Num() - 1)];
	float Seconds = 0.0f;
	USoundWave* Sound = VoiceLines::LoadWav(this, Path, Seconds);
	if (!Sound) { return; }
	// From its face: the head bone, wherever the head is.
	Voice = UGameplayStatics::SpawnSoundAttached(Sound, B->GetMesh(), TEXT("head"), FVector::ZeroVector, EAttachLocation::SnapToTarget, true, Volume, 1.0f, 0.0f, Attenuation, nullptr, true);
}

void AWorkBotController::PlayAt(const FString& File, float Volume, const FVector* Where)
{
	AWorkBot* B = Bot(); if (!B || !GetWorld()) { return; }
	const FVector Loc = Where ? *Where : ((B->GetMesh() && B->GetMesh()->DoesSocketExist(TEXT("head"))) ? B->GetMesh()->GetSocketLocation(TEXT("head")) : B->GetActorLocation());
	// THE RECORDING FIRST. A workbot's step is a struck steel plate; the synthesised one was a
	// tone, and it is what sounded wrong. An imported sound also knows its own length and ends by
	// itself, so this branch needs no timer at all -- unlike the loose-file fallback below.
	if (USoundBase* Sample = UAmbientPlayer::SampleFor(File))
	{
		UGameplayStatics::SpawnSoundAtLocation(GetWorld(), Sample, Loc, FRotator::ZeroRotator, Volume, 1.0f, 0.0f, Attenuation);
		return;
	}
	float Seconds = 0.0f;
	USoundWave* S = VoiceLines::LoadWav(this, FPaths::Combine(UAmbientPlayer::RawAudioDir(), File), Seconds);
	if (!S) { return; }
	UAudioComponent* Comp = UGameplayStatics::SpawnSoundAtLocation(GetWorld(), S, Loc, FRotator::ZeroRotator, Volume, 1.0f, 0.0f, Attenuation);
	if (!Comp) { return; }
	// AND STOP IT WHEN THE CLIP IS OVER. A procedural wave never reports itself finished, so a
	// component left on one sits there starved, and a starved procedural wave clicks -- quietly,
	// forever, once per sound ever played. The death cry is four seconds long, which makes this
	// the difference between a room and an insect.
	FTimerHandle Handle;
	TWeakObjectPtr<UAudioComponent> WeakComp = Comp;
	GetWorld()->GetTimerManager().SetTimer(Handle, FTimerDelegate::CreateLambda([WeakComp]()
	{
		if (UAudioComponent* C = WeakComp.Get()) { C->Stop(); C->DestroyComponent(); }
	}), Seconds + 0.15f, false);
}

void AWorkBotController::OnAlert(EAlertState Old, EAlertState New)
{
	AWorkBot* B = Bot(); if (!B) { return; }
	if (New == EAlertState::Suspicious) { Say(NoticeLines, 0.5f); }
	else if (New == EAlertState::Alert) { if (Voice) { Voice->Stop(); } Say(AlertLines, 0.7f); }
	else if (New == EAlertState::Calm && Old != EAlertState::Suspicious) { Say(ResumeLines, 0.4f); }
	Mode = EMode::Patrol; ModeClock = 0.0f;   // the mode is re-picked from the state on the next tick
	// The foundation's readout: what it is doing, on the player's note line.
	if (ABasePlayerController* PC = Cast<ABasePlayerController>(UGameplayStatics::GetPlayerController(GetWorld(), 0)))
	{
		PC->SetDiagNoteTimed(FString::Printf(TEXT("%s: %s"), *B->GetCharacterConfig().Name, UAlertnessComponent::StateName(New)), 3.0f);
	}
}

void AWorkBotController::Strike(AActor* Target)
{
	AWorkBot* B = Bot(); if (!B || !Target) { return; }
	// THE SWING: the sword pack's light attack on the mannequin rig, whole body for its length (the
	// user asked to see the blow, 2026-09-17: walk-only is the gait, not the arm). The servo whine
	// says the arm is coming; the thump says it arrived.
	B->CombatAttack(TEXT("LightCombo01A"));
	PlayAt(TEXT("robot_strike.wav"), 0.8f);
	TWeakObjectPtr<AWorkBotController> Weak(this); TWeakObjectPtr<AActor> WT(Target);
	FTimerHandle H;
	GetWorldTimerManager().SetTimer(H, FTimerDelegate::CreateLambda([Weak, WT]() { if (Weak.IsValid() && WT.IsValid()) { Weak->Land(WT.Get()); } }), 0.35f, false);
}

void AWorkBotController::Land(AActor* Target)
{
	AWorkBot* B = Bot(); if (!B || !Target || B->IsDead() || !GetWorld()) { return; }
	if (FVector::Dist2D(B->GetActorLocation(), Target->GetActorLocation()) > B->StrikeReach * 1.15f) { return; }   // stepped back in time
	// The blow lands on the chest as a blunt hit, through the same path as every other hit: armour,
	// vitality, the flinch, the fall.
	ACharacter* C = Cast<ACharacter>(Target);
	FHitResult Hit;
	Hit.bBlockingHit = true;
	Hit.HitObjectHandle = FActorInstanceHandle(Target);
	Hit.Component = C ? C->GetMesh() : Cast<UPrimitiveComponent>(Target->GetRootComponent());
	Hit.BoneName = TEXT("spine_02");
	Hit.ImpactPoint = Hit.Location = (C && C->GetMesh() && C->GetMesh()->DoesSocketExist(TEXT("spine_02"))) ? C->GetMesh()->GetSocketLocation(TEXT("spine_02")) : Target->GetActorLocation();
	const FVector Dir = (Target->GetActorLocation() - B->GetActorLocation()).GetSafeNormal2D();
	Hit.ImpactNormal = Hit.Normal = -Dir;
	ShotReactions::React(GetWorld(), Hit, Dir, B, B->StrikeDamage, /*bBlunt=*/true);
	PlayAt(TEXT("robot_thump.wav"), 1.0f, &Hit.ImpactPoint);   // a solid thump where it landed
	if (ABasePlayerController* PC = Cast<ABasePlayerController>(UGameplayStatics::GetPlayerController(GetWorld(), 0)))
	{
		if (PC->GetPawn() == Target) { PC->SetDiagNoteTimed(FString::Printf(TEXT("%s hits you (%s)"), *B->GetCharacterConfig().Name, *ShotReactions::LastRegion()), 3.0f); }
	}
}

void AWorkBotController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	AWorkBot* B = Bot(); UWorld* World = GetWorld();
	if (!B || !World) { return; }
	if (B->IsDead())
	{
		if (!bDead)
		{
			bDead = true; Mode = EMode::Dead;
			if (Voice) { Voice->Stop(); }
			// IT COMES APART, AND THEN IT SPINS DOWN. One mixed file per variant, built by the robot
			// death tool from CC0 recordings: a low explosion for the weight, a crunch for the casing,
			// and under both of them the pack's own motor read back slower and slower, which is what a
			// rotor does when its supply is cut. What this replaces was a synthesised falling whistle,
			// and a falling whistle is what a cartoon does when it goes off a cliff.
			PlayAt(FString::Printf(TEXT("robot_death_%d.wav"), FMath::RandRange(1, 2)), 1.0f);
			const FVector Chest = (B->GetMesh() && B->GetMesh()->DoesSocketExist(TEXT("spine_02"))) ? B->GetMesh()->GetSocketLocation(TEXT("spine_02")) : B->GetActorLocation();
			// THE GOUT. One burst up out of the chest and three more off the shoulders and head in
			// the same frame, so it reads as the thing bursting rather than as one effect playing.
			SparkFx::Burst(World, Chest, FVector::UpVector, 300, 3.2f);
			for (const TCHAR* Spot : { TEXT("head"), TEXT("upperarm_l"), TEXT("upperarm_r") })
			{
				const FName Bone(Spot);
				const FVector At = (B->GetMesh() && B->GetMesh()->DoesSocketExist(Bone)) ? B->GetMesh()->GetSocketLocation(Bone) : Chest;
				SparkFx::Burst(World, At, (FMath::VRand() + FVector::UpVector).GetSafeNormal(), 120, 2.4f);
			}
			DeathSparksLeft = DeathSparkTotal; DeathSparkClock = 0.08f;   // and it goes on arcing for the better part of ten seconds
			if (B->GetCharacterMovement()) { B->GetCharacterMovement()->StopMovementImmediately(); }
		}
		if (DeathSparksLeft > 0)
		{
			DeathSparkClock -= DeltaSeconds;
			if (DeathSparkClock <= 0.0f)
			{
				--DeathSparksLeft;
				// TAPERING, not stopping. Left runs 1 down to 0 across the whole sequence, and it
				// drives all three of the things that should fade together: the gap between arcs
				// stretches, each burst throws fewer and smaller motes, and the crackle both quietens
				// and comes less often. A wreck that simply stopped sparking on a count read as a
				// timer running out, which is exactly what it was.
				const float Left = DeathSparksLeft / static_cast<float>(DeathSparkTotal);
				DeathSparkClock = FMath::FRandRange(0.10f, 0.26f) + (1.0f - Left) * 0.55f;
				static const TCHAR* Spots[] = { TEXT("spine_02"), TEXT("head"), TEXT("upperarm_l"), TEXT("upperarm_r"), TEXT("thigh_l"), TEXT("thigh_r"), TEXT("spine_01"), TEXT("lowerarm_l"), TEXT("lowerarm_r"), TEXT("calf_r") };
				const FName Spot(Spots[FMath::RandRange(0, 9)]);
				const FVector At = (B->GetMesh() && B->GetMesh()->DoesSocketExist(Spot)) ? B->GetMesh()->GetSocketLocation(Spot) : B->GetActorLocation();
				SparkFx::Burst(World, At, FMath::VRand().GetSafeNormal() + FVector::UpVector, FMath::RoundToInt(FMath::Lerp(14.0f, 130.0f, Left)), FMath::Lerp(0.7f, 2.4f, Left) * FMath::FRandRange(0.85f, 1.2f));
				// A real arc rather than the synthesised crackle, and not on every burst near the end.
				if (FMath::FRand() < 0.35f + Left * 0.5f)
				{
					PlayAt(FString::Printf(TEXT("robot_arc_%d.wav"), FMath::RandRange(1, 3)), FMath::Lerp(0.22f, 0.75f, Left), &At);
				}
			}
		}
		return;
	}
	// Under half: it arcs on its own every couple of seconds, somewhere on the body.
	if (const UVitalityComponent* V = B->FindComponentByClass<UVitalityComponent>())
	{
		if (V->Vitality < V->MaxVitality * 0.5f)
		{
			NextSpark -= DeltaSeconds;
			if (NextSpark <= 0.0f)
			{
				NextSpark = FMath::FRandRange(1.4f, 3.2f);
				static const TCHAR* Spots[] = { TEXT("spine_02"), TEXT("head"), TEXT("upperarm_l"), TEXT("thigh_r"), TEXT("spine_01"), TEXT("lowerarm_r") };
				const TCHAR* Spot = Spots[FMath::RandRange(0, 5)];
				const FVector Where = (B->GetMesh() && B->GetMesh()->DoesSocketExist(Spot)) ? B->GetMesh()->GetSocketLocation(Spot) : B->GetActorLocation();
				SparkFx::Burst(World, Where, FMath::VRand(), 26, 1.1f);   // a wounded machine arcs where you can see it
				PlayAt(FString::Printf(TEXT("spark_crackle_%02d.wav"), FMath::RandRange(1, 3)), 0.5f, &Where);
			}
		}
	}
	ModeClock += DeltaSeconds;
	// JERKY, UNSTEADY: every so often a kick of yaw and, a third of the time, a step that half
	// stops for a fraction of a second. The gait's own stumble and limp (Characters/WorkBot.json)
	// do the rest.
	// CLANK: a footfall while it moves, quicker with it, from the feet -- heard round a corner
	// before it is seen.
	const float Speed2D = (float)B->GetVelocity().Size2D();
	if (Speed2D > 15.0f)
	{
		StepClock -= DeltaSeconds * (0.5f + Speed2D / 140.0f);
		if (StepClock <= 0.0f)
		{
			StepClock = 0.62f;
			const FVector Feet = B->GetActorLocation() - FVector(0.0f, 0.0f, 80.0f);
			PlayAt(FString::Printf(TEXT("robot_step_%02d.wav"), FMath::RandRange(1, 3)), 0.6f, &Feet);
		}
	}
	TickAlarm(DeltaSeconds);
	TwitchClock -= DeltaSeconds;
	if (TwitchClock <= 0.0f)
	{
		TwitchClock = FMath::FRandRange(0.4f, 1.6f);
		B->AddActorWorldRotation(FRotator(0.0f, FMath::FRandRange(-4.0f, 4.0f), 0.0f));
		if (FMath::FRand() < 0.35f) { StutterUntil = World->GetTimeSeconds() + FMath::FRandRange(0.12f, 0.3f); }
	}
	const EAlertState S = B->Alertness ? B->Alertness->GetState() : EAlertState::Calm;
	APawn* Player = UGameplayStatics::GetPlayerPawn(World, 0);

	// ALERT: it has someone. Close on where they are (or were last seen); in reach, face them and strike on the clock.
	if (S == EAlertState::Alert && Player && B->Alertness)
	{
		AActor* T = B->Alertness->GetTarget() ? B->Alertness->GetTarget() : Player;
		const bool bSee = B->Alertness->CanSee(T);
		const FVector Aim = bSee ? T->GetActorLocation() : B->Alertness->GetLastKnown();
		if (Mode != EMode::Hunt) { Mode = EMode::Hunt; ModeClock = 0.0f; StuckClock = 0.0f; SetAlarm(true); }
		NextStrike -= DeltaSeconds;
		if (bSee && FVector::Dist2D(B->GetActorLocation(), T->GetActorLocation()) <= B->StrikeReach)
		{
			FaceToward(T->GetActorLocation(), DeltaSeconds);
			if (NextStrike <= 0.0f) { NextStrike = B->StrikeEvery; Strike(T); }
		}
		else { Steer(DeltaSeconds, Aim, B->HuntSpeed); }
		return;
	}
	// SUSPICIOUS or SEARCHING: go to where the trouble was, look about, go again.
	if ((S == EAlertState::Suspicious || S == EAlertState::Searching) && B->Alertness && B->Alertness->HasLastKnown())
	{
		SetAlarm(false);
		if (Mode != EMode::Investigate && Mode != EMode::Look) { Mode = EMode::Investigate; ModeClock = 0.0f; StuckClock = 0.0f; Goal = B->Alertness->GetLastKnown(); GoalTimeout = 14.0f; }
		if (Mode == EMode::Investigate)
		{
			if (FVector::Dist2D(B->GetActorLocation(), Goal) < 90.0f || ModeClock > GoalTimeout || StuckClock > 2.5f)
			{
				Mode = EMode::Look; ModeClock = 0.0f; StuckClock = 0.0f; LookSeconds = 4.0f;
				LookYawTarget = B->GetActorRotation().Yaw + FMath::FRandRange(-140.0f, 140.0f);
			}
			else { Steer(DeltaSeconds, Goal, B->PatrolSpeed * 1.4f); }
		}
		else
		{
			Look(DeltaSeconds);
			if (ModeClock > LookSeconds) { Mode = EMode::Investigate; ModeClock = 0.0f; Goal = B->Alertness->GetLastKnown() + FVector(FMath::FRandRange(-150.0f, 150.0f), FMath::FRandRange(-150.0f, 150.0f), 0.0f); GoalTimeout = 10.0f; }
		}
		return;
	}
	// CALM: the round, with a pause to look at nothing between legs, and a word to itself now and then.
	SetAlarm(false);
	if (Mode != EMode::Patrol && Mode != EMode::Look) { Mode = EMode::Patrol; PickPatrolGoal(); }
	NextBark -= DeltaSeconds;
	if (NextBark <= 0.0f) { NextBark = FMath::FRandRange(11.0f, 24.0f); Say(StatusLines, 0.38f); }
	if (Mode == EMode::Patrol)
	{
		if (FVector::Dist2D(B->GetActorLocation(), Goal) < 60.0f || ModeClock > GoalTimeout || StuckClock > 2.5f)
		{
			Mode = EMode::Look; ModeClock = 0.0f; StuckClock = 0.0f; LookSeconds = FMath::FRandRange(2.0f, 5.0f);
			LookYawTarget = B->GetActorRotation().Yaw + FMath::FRandRange(-90.0f, 90.0f);
		}
		else { Steer(DeltaSeconds, Goal, B->PatrolSpeed); }
	}
	else
	{
		Look(DeltaSeconds);
		if (ModeClock > LookSeconds) { Mode = EMode::Patrol; PickPatrolGoal(); }
	}
}

void AWorkBotController::SetAlarm(bool bOn)
{
	if (bOn == bAlarm) { return; }
	bAlarm = bOn;
	if (bOn)
	{
		AlarmLights.Reset(); AlarmBase.Reset(); AlarmClock = 0.0f;
		for (TActorIterator<APointLight> It(GetWorld()); It; ++It)
		{
			if (!It->ActorHasTag(TEXT("alarm")) || !It->PointLightComponent) { continue; }
			AlarmLights.Add(It->PointLightComponent); AlarmBase.Add(It->PointLightComponent->Intensity);
		}
		return;
	}
	for (int32 i = 0; i < AlarmLights.Num(); ++i) { if (UPointLightComponent* L = AlarmLights[i].Get()) { L->SetIntensity(AlarmBase[i]); } }
	AlarmLights.Reset(); AlarmBase.Reset();
}

void AWorkBotController::TickAlarm(float DeltaSeconds)
{
	if (!bAlarm) { return; }
	AlarmClock += DeltaSeconds;
	// A slow throb with a sharper beat on top: a warning lamp, not a strobe.
	const float Pulse = 0.35f + 0.65f * FMath::Pow(0.5f + 0.5f * FMath::Sin(AlarmClock * 5.0f), 2.0f);
	for (int32 i = 0; i < AlarmLights.Num(); ++i) { if (UPointLightComponent* L = AlarmLights[i].Get()) { L->SetIntensity(AlarmBase[i] * (1.2f + 2.4f * Pulse)); } }
}
