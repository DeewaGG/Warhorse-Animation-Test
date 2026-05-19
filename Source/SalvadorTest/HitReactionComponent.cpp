#include "HitReactionComponent.h"
#include "AnimInstanceBase.h"
#include "FootIKNodes.h"
#include "CurveSamplerNodes.h"
#include "GameFramework/Character.h"
#include "GameFramework/Pawn.h"
#include "Components/CapsuleComponent.h"
#include "PhysicsEngine/PhysicalAnimationComponent.h"

UHitReactionComponent::UHitReactionComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    // Tick is gated: only enabled while a stunt or ragdoll is active to avoid per-frame cost at idle
    PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UHitReactionComponent::BeginPlay()
{
    Super::BeginPlay();
    SetupComponent();
}

// Caches references and creates the PhysicalAnimationComponent at runtime so it can
// bind to the character mesh after BeginPlay — not possible during construction
void UHitReactionComponent::SetupComponent()
{
    BPVictim = GetOwner();
    if (!BPVictim)
    {
        UE_LOG(LogTemp, Error, TEXT("HitReactionComponent: no owner actor"));
        return;
    }

    ACharacter* Character = Cast<ACharacter>(BPVictim);
    if (!Character)
    {
        UE_LOG(LogTemp, Error, TEXT("HitReactionComponent: owner is not a Character"));
        return;
    }

    Mesh = Character->GetMesh();
    if (!Mesh)
    {
        UE_LOG(LogTemp, Error, TEXT("HitReactionComponent: mesh not found"));
        return;
    }

    ABP = Cast<UAnimInstanceBase>(Mesh->GetAnimInstance());
    if (!ABP)
    {
        UE_LOG(LogTemp, Error, TEXT("HitReactionComponent: anim instance is not UAnimInstanceBase"));
        return;
    }

    CharacterMovement = Character->GetCharacterMovement();
    if (!CharacterMovement)
    {
        UE_LOG(LogTemp, Error, TEXT("HitReactionComponent: no CharacterMovementComponent"));
        return;
    }

    PhysicAnimComp = NewObject<UPhysicalAnimationComponent>(GetOwner());
    PhysicAnimComp->RegisterComponent();
    PhysicAnimComp->SetSkeletalMeshComponent(Mesh);
}

// Entry point called from BP on hit detection.
// HP is decremented first so ActivateRagdoll (triggered at ThrustRecover) sees the final value.
// Blacklisted bones still run the full stunt — they just skip the HP decrement.
void UHitReactionComponent::HitW_Physics(int32 InAttackSide, FName InBoneHit, FVector InHitDir, double InHitStrength)
{
    if (!BlacklistedHitBones.Contains(InBoneHit))
        CurrentHP = FMath::Max(0, CurrentHP - 1);

    AttackSide = InAttackSide;
    ProtectHit(InHitDir, InHitStrength);

    // Pelvis and root hits redirect to a stable fallback so physics motors have a valid anchor
    HitBone = (InBoneHit == PelvisBoneName || InBoneHit.IsNone()) ? FallbackHitBone : InBoneHit;

    // PhysicsBones drives which hierarchy gets simulation and impulses.
    // Top (0): upper spine + hit bone — full upper-body reaction.
    // Mid (1) / Bot (2): lower spine + hit bone — belly/leg reaction.
    PhysicsBones.Reset();
    switch (AttackSide)
    {
        case 0:  PhysicsBones = { UpperSimBone, HitBone }; break;
        case 1:  PhysicsBones = { MidSimBone,   HitBone }; break;
        case 2:  PhysicsBones = { MidSimBone,   HitBone }; break;
        default: PhysicsBones = { MidSimBone,   HitBone }; break;
    }

    // Same stunt runs on every hit including the death hit.
    // ActivateRagdoll (called from BP at ThrustRecover) overrides physics state cleanly.
    SetComponentTickEnabled(true);
    ActivateSimBones();
    SetupVarsForSim();
}

// Normalises the hit direction before it is stored.
// Bot hits (side 2) replace a zero InHitDir with the character's forward vector so PelvisMovement
// still has a valid axis; other sides preserve zero so their pelvis path is unaffected.
void UHitReactionComponent::ProtectHit(FVector InHitDir, double InHitStrength)
{
    FVector fallback = FVector::ZeroVector;
    if (APawn* Pawn = Cast<APawn>(BPVictim))
        fallback = Pawn->GetActorForwardVector() * 2.0;

    const FVector safeDir = InHitDir.IsZero() ? fallback : InHitDir;
    HitDir = (AttackSide == 2) ? safeDir : InHitDir;
    HitStrength = InHitStrength;
}

// Enables physical animation and simulation for each bone in PhysicsBones, then fires impulses.
void UHitReactionComponent::ActivateSimBones()
{
    if (!Mesh || !PhysicAnimComp) return;

    for (const FName& Bone : PhysicsBones)
    {
        // Profile applied before simulation so motor constraints exist on the first physics tick
        PhysicAnimComp->ApplyPhysicalAnimationProfileBelow(Bone, PhysicalAnimProfile, true, true);
        Mesh->SetAllBodiesBelowSimulatePhysics(Bone, true, true);
        // Weight starts at 0 so SimulationWeight() ramps up via the stunt curve — prevents a one-frame physics drop
        Mesh->SetAllBodiesBelowPhysicsBlendWeight(Bone, 0.f, false, true);
    }

    for (const FName& Bone : PhysicsBones)
    {
        Mesh->AddImpulse(HitDir * HitStrength,                                   Bone, true);
        Mesh->AddImpulse(HitDir * HitStrength / (double)SecondaryImpulseDivisor, Bone, true);
    }
}

// Snapshots ABP IK state at the hit frame so ReactiveSteps can track drift relative to that baseline.
void UHitReactionComponent::SetupVarsForSim()
{
    bBlendingOutPhysics = false;
    bResetHit           = true;

    const FVector CapsuleLoc = BPVictim ? BPVictim->GetActorLocation() : FVector::ZeroVector;
    PushGoal = FVector2D(CapsuleLoc + HitDir * PushGoalDistance);

    if (ABP && Mesh)
    {
        ABP->bIsStunned     = true;
        RightHandIKPosition = ABP->RightHandIKPosition;
        LeftHandIKPosition  = ABP->LeftHandIKPosition;
        StartPelvisPos      = ABP->PelvisGoalPosition;
        RestLeftFootPos    = ABP->LeftFootIKPosition;
        RestRightFootPos   = ABP->RightFootIKPosition;
        FrozenLeftFootIK   = ABP->LeftFootIKPosition;
        FrozenRightFootIK  = ABP->RightFootIKPosition;
        // Both feet share the same pelvis reference at hit start; each updates its own after a stride lands
        FrozenPelvisWorldL = Mesh->GetBoneLocation(VirtualPelvisBone);
        FrozenPelvisWorldR = FrozenPelvisWorldL;
        bWasLStriding      = false;
        bWasRStriding      = false;
    }

    SetupStrides(RestLeftFootPos, RestRightFootPos);

    OpenTickGate();
}

// Initialises foot IK state for both feet. bStriding is forced to false so no step fires on the
// first frame — the anti-slide WorldDelta activates immediately and steps trigger naturally when
// the pelvis exceeds the stride threshold under the physics push.
void UHitReactionComponent::SetupStrides(FVector LeftGoal, FVector RightGoal, bool bIsRepositioning)
{
    if (!Mesh) return;

    const FVector MeshLoc = Mesh->GetComponentLocation();

    UFootIKNodes::SetupFootIK(
        LFootState, Mesh, VirtualLeftFootBone, VirtualPelvisBone,
        MeshLoc, LeftGoal,
        LStrideThreshold, LStrideDuration, LStrideHeight, LStrideCooldown, LStrideReach,
        FootSize, FootPitchScale);

    UFootIKNodes::SetupFootIK(
        RFootState, Mesh, VirtualRightFootBone, VirtualPelvisBone,
        MeshLoc, RightGoal,
        RStrideThreshold, RStrideDuration, RStrideHeight, RStrideCooldown, RStrideReach,
        FootSize, FootPitchScale);

    if (!bIsRepositioning)
    {
        LFootState.bStriding = false;
        RFootState.bStriding = false;
    }
}

void UHitReactionComponent::OpenTickGate()
{
    bDoOnceFired = false;
}

// Drives a persistent low-level physics sim on MidSimBone after the character reaches 1 HP.
// Two sine waves at different frequencies produce an organic struggling feel that never exactly
// repeats — wave 1 mimics laboured weight-shifting, wave 2 adds involuntary muscle tremor.
void UHitReactionComponent::LowHealthTick(float DeltaTime)
{
    if (!bLowHealthActive || !Mesh || bBlendingOutPhysics) return;

    LowHealthElapsed += DeltaTime;

    // Safety timeout: stops the sim if the character is never finished off
    if (LowHealthTickTimeout > 0.f && LowHealthElapsed >= LowHealthTickTimeout)
    {
        Mesh->SetAllBodiesBelowPhysicsBlendWeight(MidSimBone, 0.f, false, true);
        Mesh->SetAllBodiesBelowSimulatePhysics(MidSimBone, false, true);
        bLowHealthActive = false;
        SetComponentTickEnabled(false);
        return;
    }

    const float BlendAlpha = (LowHealthTransitionTime > 0.f)
        ? FMath::Clamp(LowHealthElapsed / LowHealthTransitionTime, 0.f, 1.f)
        : 1.f;
    LowHealthBlend = LowHealthSimWeight * BlendAlpha;

    const float Wave1 = LowHealthOscAmplitude  * FMath::Sin(LowHealthOscFrequency  * LowHealthElapsed * 2.f * PI);
    const float Wave2 = LowHealthOscAmplitude2 * FMath::Sin(LowHealthOscFrequency2 * LowHealthElapsed * 2.f * PI);

    const float EffBlend = FMath::Clamp(LowHealthBlend + Wave1 + Wave2, 0.f, 1.f);
    Mesh->SetAllBodiesBelowPhysicsBlendWeight(MidSimBone, EffBlend, false, true);
}

// Advances the stunt curve sampler and returns true when the stunt duration has elapsed.
// AttackSide: 0=top, 1=mid, 2=bot. Top and bot extend their window via time multipliers
// to give upper and lower body hits a distinct feel.
bool UHitReactionComponent::CurveTickValues(float DeltaTime)
{
    float duration = 0.f;
    switch (AttackSide)
    {
        case 0:  duration = (float)(StuntTime * StuntTimeMultiplierTop); break;
        case 2:  duration = (float)(StuntTime * StuntTimeMultiplierBot); break;
        default: duration = (float)(StuntTime);                          break;
    }

    UCurveFloat* curve = nullptr;
    switch (AttackSide)
    {
        case 0:  curve = StuntCurveTop; break;
        case 2:  curve = StuntCurveBot; break;
        default: curve = StuntCurveMid; break;
    }

    float outValue  = 0.f;
    bool  bFinished = false;
    UCurveSamplerNodes::SampleCurve(StuntValueState, curve, duration, DeltaTime, bResetHit, outValue, bFinished);

    SimValue = FMath::Min((double)outValue, 1.0);
    return bFinished;
}

// Writes the physics blend weight for all sim bones, scaled by the current curve value.
// Bot hits (side 2) are capped at BotSideMaxBlendWeight to avoid a full ragdoll look on low-body strikes.
void UHitReactionComponent::SimulationWeight()
{
    if (!Mesh) return;
    const float MaxWeight = (AttackSide == 2) ? BotSideMaxBlendWeight : 1.f;
    for (const FName& Bone : PhysicsBones)
        Mesh->SetAllBodiesBelowPhysicsBlendWeight(Bone, (float)SimValue * MaxWeight, false, true);
}

// Moves the character capsule toward PushGoal while the stunt is active.
// Bot attacks (side 2) use scale=0 — low hits knock down, not back.
void UHitReactionComponent::PushVictim()
{
    if (!BPVictim) return;

    const FVector CapsuleLoc = BPVictim->GetActorLocation();
    const FVector2D VictimXY = FVector2D(CapsuleLoc);

    if (FVector2D::Distance(VictimXY, PushGoal) <= 0.1f) return;

    const float scale = (AttackSide == 2) ? 0.f : (float)(SimValue * (VictimPushForce / 10.0) + 0.1);

    if (APawn* Pawn = Cast<APawn>(BPVictim))
        Pawn->AddMovementInput(HitDir, scale);
}

// Drives PelvisGoalPosition based on SimValue.
// alpha = |SimValue*2 - 1| forms a V-curve: the pelvis dips deepest at peak simulation
// (SimValue ≈ 0.5) and smoothly returns to its rest position at the start and end of the stunt.
void UHitReactionComponent::PelvisMovement()
{
    if (!ABP) return;

    const double alpha = FMath::Abs(SimValue * 2.0 - 1.0);
    const FVector displaceVec = HitDir * PelvisDisruption * (SimValue * 2.0 - 1.0);
    const FVector lerpedVec   = FMath::Lerp(displaceVec, StartPelvisPos, (float)alpha);
    const FVector pelvisXY    = (AttackSide == 2) ? lerpedVec : StartPelvisPos;

    const double pelvisZ = FMath::Lerp(
        FMath::Clamp(HitStrength * (double)PelvisZHitStrengthScale, (double)PelvisZClampRange.X, (double)PelvisZClampRange.Y),
        StartPelvisPos.Z,
        (float)alpha);

    ABP->PelvisGoalPosition = FVector(pelvisXY.X, pelvisXY.Y, pelvisZ);
}

// Runs foot IK each frame during and after the stunt, keeping feet planted while the pelvis moves.
// AlignFoot: for a non-striding foot, shifts its AnchorGoal by the current pelvis drift in component
// space so SolveFootIK always has an accurate StrideStartGoal when a step is triggered.
// FrozenPelvis references are updated on stride land so subsequent steps have a valid baseline.
void UHitReactionComponent::ReactiveSteps(float DeltaTime)
{
    if (!Mesh || !ABP) return;

    const FTransform MeshTW    = Mesh->GetComponentTransform();
    const FVector    PelvisNow = Mesh->GetBoneLocation(VirtualPelvisBone);
    const FVector    MeshLoc   = Mesh->GetComponentLocation();
    const FRotator   MeshRot   = Mesh->GetComponentRotation();

    auto AlignFoot = [&](FFootIKState& State, const FVector& FrozenIK, const FVector& FrozenPelvisW)
    {
        if (State.bStriding) return;
        const FVector DeltaCS  = MeshTW.InverseTransformVector(PelvisNow - FrozenPelvisW);
        State.AnchorGoal       = FrozenIK - DeltaCS;
        State.AnchorWorldPos   = MeshLoc;
    };

    AlignFoot(LFootState, FrozenLeftFootIK,  FrozenPelvisWorldL);
    AlignFoot(RFootState, FrozenRightFootIK, FrozenPelvisWorldR);

    FVector   outL, outR;
    FRotator  outLR, outRR;
    UFootIKNodes::SolveFootIK(LFootState, RFootState, MeshLoc, MeshRot,
        DeltaTime, outL, outR, outLR, outRR);

    ABP->LeftFootIKPosition  = outL;
    ABP->RightFootIKPosition = outR;
    ABP->LeftFootRot         = outLR;
    ABP->RightFootRot        = outRR;

    // Update frozen reference when a stride lands so the next step starts from the correct world position
    if (bWasLStriding && !LFootState.bStriding)
    {
        FrozenLeftFootIK   = LFootState.AnchorGoal;
        FrozenPelvisWorldL = PelvisNow;
    }
    if (bWasRStriding && !RFootState.bStriding)
    {
        FrozenRightFootIK  = RFootState.AnchorGoal;
        FrozenPelvisWorldR = PelvisNow;
    }

    bWasLStriding = LFootState.bStriding;
    bWasRStriding = RFootState.bStriding;
}

// Smoothly ramps the low-health physics blend to zero over Duration seconds.
// Only has effect while bLowHealthActive — a no-op otherwise.
void UHitReactionComponent::BlendOutPhysics(float Duration)
{
    if (!bLowHealthActive || !Mesh) return;
    bBlendingOutPhysics = true;
    BlendOutDuration    = FMath::Max(Duration, 0.f);
    BlendOutElapsed     = 0.f;
    BlendOutStartWeight = LowHealthBlend;
    SetComponentTickEnabled(true);
}

// Hard-stops the low-health sim with no blend. Use BlendOutPhysics() for a graceful transition.
void UHitReactionComponent::StopLowHealthSim()
{
    if (!bLowHealthActive || !Mesh) return;
    Mesh->SetAllBodiesBelowPhysicsBlendWeight(MidSimBone, 0.f, false, true);
    Mesh->SetAllBodiesBelowSimulatePhysics(MidSimBone, false, true);
    bLowHealthActive = false;
}

// Writes the physics blend weight directly for all PhysicsBones — called from BP during a
// death-plant montage to drive the transition weight from the animation side.
void UHitReactionComponent::SetDeathPlantBlend(float Blend)
{
    if (!Mesh) return;
    for (const FName& Bone : PhysicsBones)
        Mesh->SetAllBodiesBelowPhysicsBlendWeight(Bone, Blend, false, true);
}

// Transitions the character from stunt physics to a full ragdoll.
// The "Ragdoll" string must match both a Physical Animation profile and a Constraint profile
// set up in the Physics Asset — both are applied here before simulation is re-enabled.
// Blend weight starts at 0 and ramps to 1 over RagdollTransitionTime in TickComponent.
void UHitReactionComponent::ActivateRagdoll()
{
    if (!Mesh || !PhysicAnimComp) return;

    // Clear stunt state so TickComponent routes into the ragdoll branch exclusively
    bLowHealthActive    = false;
    Mesh->SetAllBodiesBelowSimulatePhysics(RootSimBone, false, true);
    if (ABP) ABP->bIsStunned = false;
    bSimFinishTriggered = false;
    bRepositioning      = false;

    if (CharacterMovement)
        CharacterMovement->DisableMovement();

    PhysicAnimComp->ApplyPhysicalAnimationProfileBelow(RootSimBone, TEXT("Ragdoll"), true, true);
    Mesh->SetConstraintProfileForAll(TEXT("Ragdoll"), true);
    Mesh->SetAllBodiesBelowSimulatePhysics(PelvisBoneName, true, true);
    Mesh->SetAllBodiesBelowPhysicsBlendWeight(PelvisBoneName, 0.f, false, true);

    if (RagdollImpulse > 0.f && !HitDir.IsZero())
        Mesh->AddImpulse(-HitDir.GetSafeNormal() * RagdollImpulse, PelvisBoneName, true);

    bIsRagdoll     = true;
    RagdollElapsed = 0.f;
    SetComponentTickEnabled(true);
}

// Called twice per stunt cycle:
//   1. At curve end — disables stunt simulation on all bones below root.
//   2. After foot repositioning completes — optionally re-enables MidSimBone for low-health.
// Re-enabling MidSimBone at the current LowHealthBlend avoids a weight snap mid low-health cycle.
void UHitReactionComponent::SimFinish()
{
    if (!Mesh) return;
    Mesh->SetAllBodiesBelowSimulatePhysics(RootSimBone, false, true);

    if (bLowHealthActive && PhysicAnimComp)
    {
        PhysicAnimComp->ApplyPhysicalAnimationProfileBelow(MidSimBone, PhysicalAnimProfile, true, true);
        Mesh->SetAllBodiesBelowSimulatePhysics(MidSimBone, true, true);
        Mesh->SetAllBodiesBelowPhysicsBlendWeight(MidSimBone, LowHealthBlend, false, true);
    }
}

void UHitReactionComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (DeltaTime <= 0.f) return;

    // ── Ragdoll branch ────────────────────────────────────────────────────────
    // Runs exclusively while bIsRagdoll; returns early to skip all stunt logic.
    if (bIsRagdoll)
    {
        RagdollElapsed += DeltaTime;
        const float T = (RagdollTransitionTime > 0.f)
            ? FMath::Clamp(RagdollElapsed / RagdollTransitionTime, 0.f, 1.f)
            : 1.f;
        const float blend = RagdollTransitionCurve
            ? RagdollTransitionCurve->GetFloatValue(T) : T;
        Mesh->SetAllBodiesBelowPhysicsBlendWeight(PelvisBoneName, blend, false, true);

        if (T >= 1.f)
        {
            // PostBlendElapsed counts only after the blend-in is complete, so RagdollLifetime
            // measures from full-ragdoll, not from the moment ActivateRagdoll was called.
            const float PostBlendElapsed = RagdollElapsed - RagdollTransitionTime;
            if (RagdollLifetime > 0.f && PostBlendElapsed >= RagdollLifetime)
            {
                // Freeze: simulation stays enabled so the physics pose is preserved.
                // Disabling simulation would snap the mesh back to the animation pose.
                for (FBodyInstance* Body : Mesh->Bodies)
                {
                    if (Body)
                    {
                        Body->SetLinearVelocity(FVector::ZeroVector, false);
                        Body->SetAngularVelocityInRadians(FVector::ZeroVector, false);
                        Body->PutInstanceToSleep();
                    }
                }
                Mesh->SetEnableGravity(false);
                bIsRagdoll = false;

                // Stop all actor and component ticks so nothing nudges the sleeping bodies.
                // Remove all collisions so the frozen corpse doesn't block movement or attacks.
                AActor* Owner = GetOwner();
                if (Owner)
                {
                    Owner->SetActorTickEnabled(false);
                    for (UActorComponent* Comp : Owner->GetComponents())
                    {
                        if (Comp) Comp->SetComponentTickEnabled(false);

                        UPrimitiveComponent* Prim = Cast<UPrimitiveComponent>(Comp);
                        if (Prim) Prim->SetCollisionEnabled(ECollisionEnabled::NoCollision);
                    }
                }
            }
            else if (RagdollLifetime <= 0.f)
            {
                SetComponentTickEnabled(false);
            }
        }
        return;
    }

    // ── Stunt branch ──────────────────────────────────────────────────────────
    const bool bFinished = CurveTickValues(DeltaTime);

    if (bFinished)
    {
        // bSimFinishTriggered gates the foot snapshot and first SimFinish to fire exactly once
        // at stunt end, regardless of how many frames bFinished remains true.
        if (!bSimFinishTriggered)
        {
            bSimFinishTriggered = true;

            // Capture foot IK state at the stunt end frame as the baseline for repositioning
            if (ABP && Mesh)
            {
                FrozenLeftFootIK   = ABP->LeftFootIKPosition;
                FrozenRightFootIK  = ABP->RightFootIKPosition;
                FrozenPelvisWorldL = Mesh->GetBoneLocation(VirtualPelvisBone);
                FrozenPelvisWorldR = FrozenPelvisWorldL;
                bWasLStriding      = false;
                bWasRStriding      = false;
            }

            SetupStrides(RestLeftFootPos, RestRightFootPos, false);

            bRepositioning = true;
            SimFinish();
        }
        else if (bRepositioning)
        {
            ReactiveSteps(DeltaTime);

            if (UFootIKNodes::AreFeetRepositioned(LFootState, RFootState))
            {
                if (ABP) ABP->bIsStunned = false;
                SimFinish();    // Second SimFinish: re-enables MidSimBone for low-health if active
                bSimFinishTriggered = false;
                bRepositioning      = false;

                // Activate low-health sim on the hit that brings HP to exactly 1
                if (CurrentHP == 1 && !bLowHealthActive && PhysicAnimComp)
                {
                    bLowHealthActive = true;
                    LowHealthElapsed = 0.f;
                    LowHealthBlend   = 0.f;
                    PhysicAnimComp->ApplyPhysicalAnimationProfileBelow(MidSimBone, PhysicalAnimProfile, true, true);
                    Mesh->SetAllBodiesBelowSimulatePhysics(MidSimBone, true, true);
                    Mesh->SetAllBodiesBelowPhysicsBlendWeight(MidSimBone, 0.f, false, true);
                }

                if (!bLowHealthActive)
                    SetComponentTickEnabled(false);
            }
        }
    }
    else
    {
        SimulationWeight();
        PushVictim();
        PelvisMovement();
        ReactiveSteps(DeltaTime);

        // Advance bResetHit to false after the first live tick so the curve sampler
        // doesn't reinitialise on the second frame
        if (!bDoOnceFired)
        {
            bDoOnceFired = true;
            bResetHit    = false;
        }
    }

    LowHealthTick(DeltaTime);

    // ── Low-health blend-out ──────────────────────────────────────────────────
    if (bBlendingOutPhysics && Mesh)
    {
        BlendOutElapsed += DeltaTime;
        const float T = (BlendOutDuration > 0.f)
            ? FMath::Clamp(BlendOutElapsed / BlendOutDuration, 0.f, 1.f) : 1.f;
        Mesh->SetAllBodiesBelowPhysicsBlendWeight(MidSimBone, FMath::Lerp(BlendOutStartWeight, 0.f, T), false, true);
        if (T >= 1.f)
        {
            Mesh->SetAllBodiesBelowSimulatePhysics(MidSimBone, false, true);
            bLowHealthActive    = false;
            bBlendingOutPhysics = false;
            if (!bIsRagdoll)
                SetComponentTickEnabled(false);
        }
    }
}
