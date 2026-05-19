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
    PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UHitReactionComponent::BeginPlay()
{
    Super::BeginPlay();
    SetupComponent();
}

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

void UHitReactionComponent::HitW_Physics(int32 InAttackSide, FName InBoneHit, FVector InHitDir, double InHitStrength)
{
    if (!BlacklistedHitBones.Contains(InBoneHit))
        CurrentHP = FMath::Max(0, CurrentHP - 1);

    AttackSide = InAttackSide;
    ProtectHit(InHitDir, InHitStrength);

    HitBone = (InBoneHit == PelvisBoneName || InBoneHit.IsNone()) ? FallbackHitBone : InBoneHit;

    PhysicsBones.Reset();
    switch (AttackSide)
    {
        case 0:  PhysicsBones = { UpperSimBone, HitBone };                                       break;
        case 1:  PhysicsBones = { MidSimBone, HitBone };                                         break;
        case 2:  PhysicsBones = { MidSimBone, HitBone };                                         break;
        default: PhysicsBones = { MidSimBone, HitBone };                                         break;
    }

    // Same behavior on every hit — stunt runs even on the death hit.
    // ActivateRagdoll (triggered at ThrustRecover) overrides physics cleanly.
    SetComponentTickEnabled(true);
    ActivateSimBones();
    SetupVarsForSim();
}

void UHitReactionComponent::ProtectHit(FVector InHitDir, double InHitStrength)
{
    FVector fallback = FVector::ZeroVector;
    if (APawn* Pawn = Cast<APawn>(BPVictim))
        fallback = Pawn->GetActorForwardVector() * 2.0;

    const FVector safeDir = InHitDir.IsZero() ? fallback : InHitDir;
    HitDir = (AttackSide == 2) ? safeDir : InHitDir;
    HitStrength = InHitStrength;
}

void UHitReactionComponent::ActivateSimBones()
{
    if (!Mesh || !PhysicAnimComp) return;

    for (const FName& Bone : PhysicsBones)
    {
        // Profile must be applied before enabling simulation so motor constraints exist on the first physics tick
        PhysicAnimComp->ApplyPhysicalAnimationProfileBelow(Bone, PhysicalAnimProfile, true, true);
        Mesh->SetAllBodiesBelowSimulatePhysics(Bone, true, true);
        // Start at weight 0 so SimulationWeight() ramps up via the curve — prevents one-frame physics drop
        Mesh->SetAllBodiesBelowPhysicsBlendWeight(Bone, 0.f, false, true);
    }

    for (const FName& Bone : PhysicsBones)
    {
        Mesh->AddImpulse(HitDir * HitStrength,         Bone, true);
        Mesh->AddImpulse(HitDir * HitStrength / 5.0,   Bone, true);
    }

}

void UHitReactionComponent::SetupVarsForSim()
{
    bBlendingOutPhysics = false;
    bResetHit           = true;

    const FVector CapsuleLoc = BPVictim ? BPVictim->GetActorLocation() : FVector::ZeroVector;
    PushGoal = FVector2D(CapsuleLoc + HitDir * 200.0);

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
        FrozenPelvisWorldL = Mesh->GetBoneLocation(VirtualPelvisBone);
        FrozenPelvisWorldR = FrozenPelvisWorldL;
        bWasLStriding      = false;
        bWasRStriding      = false;
    }

    SetupStrides(RestLeftFootPos, RestRightFootPos);

    OpenTickGate();
}

void UHitReactionComponent::SetupStrides(FVector LeftGoal, FVector RightGoal, bool bIsRepositioning)
{
    if (!Mesh) return;

    const FVector MeshLoc = Mesh->GetComponentLocation();

    UFootIKNodes::SetupFootIK(
        LFootState, Mesh, VirtualLeftFootBone, VirtualPelvisBone,
        MeshLoc, LeftGoal,
        LStrideThreshold, LStrideDuration, LStrideHeight, LStrideCooldown, LStrideReach,
        31.f, 100.f);

    UFootIKNodes::SetupFootIK(
        RFootState, Mesh, VirtualRightFootBone, VirtualPelvisBone,
        MeshLoc, RightGoal,
        RStrideThreshold, RStrideDuration, RStrideHeight, RStrideCooldown, RStrideReach,
        31.f, 100.f);

    if (!bIsRepositioning)
    {
        // Arrancar sin stride forzado: el anti-slide de WorldDelta actúa desde el frame 1
        // y los pasos se disparan cuando la cadera supera el umbral al ser empujado
        LFootState.bStriding = false;
        RFootState.bStriding = false;
    }
}

void UHitReactionComponent::OpenTickGate()
{
    bDoOnceFired = false;
}

void UHitReactionComponent::LowHealthTick(float DeltaTime)
{
    if (!bLowHealthActive || !Mesh || bBlendingOutPhysics) return;

    LowHealthElapsed += DeltaTime;

    if (LowHealthTickTimeout > 0.f && LowHealthElapsed >= LowHealthTickTimeout)
    {
        Mesh->SetAllBodiesBelowPhysicsBlendWeight(MidSimBone, 0.f, false, true);
        Mesh->SetAllBodiesBelowSimulatePhysics(MidSimBone, false, true);
        bLowHealthActive = false;
        SetComponentTickEnabled(false);
        return;
    }

    // Blend-in: ramp the base weight from 0 to LowHealthSimWeight
    const float BlendAlpha = (LowHealthTransitionTime > 0.f)
        ? FMath::Clamp(LowHealthElapsed / LowHealthTransitionTime, 0.f, 1.f)
        : 1.f;
    LowHealthBlend = LowHealthSimWeight * BlendAlpha;

    // Two sine waves at different frequencies — their combined pattern never exactly
    // repeats, producing an organic struggling feel driven purely by physics weight.
    // Wave 1: slow, like laboured breathing/shifting weight.
    // Wave 2: faster, like involuntary muscle tremor layered on top.
    const float Wave1 = LowHealthOscAmplitude  * FMath::Sin(LowHealthOscFrequency  * LowHealthElapsed * 2.f * PI);
    const float Wave2 = LowHealthOscAmplitude2 * FMath::Sin(LowHealthOscFrequency2 * LowHealthElapsed * 2.f * PI);

    const float EffBlend = FMath::Clamp(LowHealthBlend + Wave1 + Wave2, 0.f, 1.f);
    Mesh->SetAllBodiesBelowPhysicsBlendWeight(MidSimBone, EffBlend, false, true);
}

bool UHitReactionComponent::CurveTickValues(float DeltaTime)
{
    float duration = 0.f;
    switch (AttackSide)
    {
        case 0:  duration = (float)(StuntTime * 1.1); break;
        case 2:  duration = (float)(StuntTime * 1.4); break;
        default: duration = (float)(StuntTime);       break;
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

void UHitReactionComponent::SimulationWeight()
{
    if (!Mesh) return;
    const float MaxWeight = (AttackSide == 2) ? 0.5f : 1.f;
    for (const FName& Bone : PhysicsBones)
        Mesh->SetAllBodiesBelowPhysicsBlendWeight(Bone, (float)SimValue * MaxWeight, false, true);
}

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

void UHitReactionComponent::PelvisMovement()
{
    if (!ABP) return;

    const double alpha = FMath::Abs(SimValue * 2.0 - 1.0);
    const FVector displaceVec = HitDir * PelvisDisruption * (SimValue * 2.0 - 1.0);
    const FVector lerpedVec   = FMath::Lerp(displaceVec, StartPelvisPos, (float)alpha);
    const FVector pelvisXY    = (AttackSide == 2) ? lerpedVec : StartPelvisPos;

    const double pelvisZ = FMath::Lerp(
        FMath::Clamp(HitStrength * -0.07, -70.0, 20.0),
        StartPelvisPos.Z,
        (float)alpha);

    ABP->PelvisGoalPosition = FVector(pelvisXY.X, pelvisXY.Y, pelvisZ);
}


void UHitReactionComponent::ReactiveSteps(float DeltaTime)
{
    if (!Mesh || !ABP) return;

    const FTransform MeshTW    = Mesh->GetComponentTransform();
    const FVector    PelvisNow = Mesh->GetBoneLocation(VirtualPelvisBone);
    const FVector    MeshLoc   = Mesh->GetComponentLocation();
    const FRotator   MeshRot   = Mesh->GetComponentRotation();

    // Para pies no striding: alinear AnchorGoal con el freeze por pelvis.
    // Así SolveFootIK usa el offset correcto como StrideStartGoal cuando dispara un paso.
    auto AlignFoot = [&](FFootIKState& State, const FVector& FrozenIK, const FVector& FrozenPelvisW)
    {
        if (State.bStriding) return;
        const FVector DeltaCS  = MeshTW.InverseTransformVector(PelvisNow - FrozenPelvisW);
        State.AnchorGoal       = FrozenIK - DeltaCS;
        State.AnchorWorldPos   = MeshLoc;   // WorldDelta = 0 en SolveFoot este frame
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

    // Al aterrizar un stride, actualizar la referencia de congelado para el siguiente ciclo
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

void UHitReactionComponent::BlendOutPhysics(float Duration)
{
    if (!bLowHealthActive || !Mesh) return;
    bBlendingOutPhysics = true;
    BlendOutDuration    = FMath::Max(Duration, 0.f);
    BlendOutElapsed     = 0.f;
    BlendOutStartWeight = LowHealthBlend;
    SetComponentTickEnabled(true);
}

void UHitReactionComponent::StopLowHealthSim()
{
    if (!bLowHealthActive || !Mesh) return;
    Mesh->SetAllBodiesBelowPhysicsBlendWeight(MidSimBone, 0.f, false, true);
    Mesh->SetAllBodiesBelowSimulatePhysics(MidSimBone, false, true);
    bLowHealthActive = false;
}

void UHitReactionComponent::SetDeathPlantBlend(float Blend)
{
    if (!Mesh) return;
    for (const FName& Bone : PhysicsBones)
        Mesh->SetAllBodiesBelowPhysicsBlendWeight(Bone, Blend, false, true);
}

void UHitReactionComponent::ActivateRagdoll()
{
    if (!Mesh || !PhysicAnimComp) return;

    // Stop stunt and low-health sim: physics, tick, and ABP stun flag
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
            SetComponentTickEnabled(false);
        return;
    }

    const bool bFinished = CurveTickValues(DeltaTime);

    if (bFinished)
    {
        if (!bSimFinishTriggered)
        {
            bSimFinishTriggered = true;

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
                SimFinish();
                bSimFinishTriggered = false;
                bRepositioning      = false;

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

        if (!bDoOnceFired)
        {
            bDoOnceFired = true;
            bResetHit    = false;
        }
    }

    LowHealthTick(DeltaTime);

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
