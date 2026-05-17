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
    SetComponentTickEnabled(true);

    AttackSide = InAttackSide;

    HitBone = (InBoneHit == TEXT("pelvis") || InBoneHit.IsNone()) ? FName(TEXT("spine_01")) : InBoneHit;

    PhysicsBones.Reset();
    switch (AttackSide)
    {
        case 0:  PhysicsBones = { TEXT("spine_03"), HitBone };                                          break;
        case 1:  PhysicsBones = { TEXT("spine_01"), HitBone };                                          break;
        case 2:  PhysicsBones = { TEXT("thigh_l"), TEXT("thigh_r"), HitBone };                          break;
        default: PhysicsBones = { TEXT("spine_01"), HitBone };                                          break;
    }

    ProtectHit(InHitDir, InHitStrength);
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
        Mesh->SetAllBodiesBelowSimulatePhysics(Bone, true, true);
        PhysicAnimComp->ApplyPhysicalAnimationProfileBelow(Bone, TEXT("HitReaction"), true, true);
    }

    for (const FName& Bone : PhysicsBones)
    {
        Mesh->AddImpulse(HitDir * HitStrength,         Bone, true);
        Mesh->AddImpulse(HitDir * HitStrength / 5.0,   Bone, true);
    }
}

void UHitReactionComponent::SetupVarsForSim()
{
    bResetHit = true;

    const FVector CapsuleLoc = BPVictim ? BPVictim->GetActorLocation() : FVector::ZeroVector;
    PushGoal = FVector2D(CapsuleLoc + HitDir * 200.0);

    if (ABP)
    {
        ABP->bIsStunned     = true;
        RightHandIKPosition = ABP->RightHandIKPosition;
        LeftHandIKPosition  = ABP->LeftHandIKPosition;
        StartPelvisPos      = ABP->PelvisGoalPosition;
        RestLeftFootPos    = ABP->LeftFootIKPosition;
        RestRightFootPos   = ABP->RightFootIKPosition;
        FrozenLeftFootIK   = ABP->LeftFootIKPosition;
        FrozenRightFootIK  = ABP->RightFootIKPosition;
        FrozenPelvisWorldL = Mesh->GetBoneLocation(TEXT("VB pelvis"));
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
        LFootState, Mesh, TEXT("VB foot_l"), TEXT("VB pelvis"),
        MeshLoc, LeftGoal,
        LStrideThreshold, LStrideDuration, LStrideHeight, LStrideCooldown, LStrideReach,
        31.f, 100.f);

    UFootIKNodes::SetupFootIK(
        RFootState, Mesh, TEXT("VB foot_r"), TEXT("VB pelvis"),
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
    for (const FName& Bone : PhysicsBones)
        Mesh->SetAllBodiesBelowPhysicsBlendWeight(Bone, (float)SimValue, false, true);
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
    const FVector    PelvisNow = Mesh->GetBoneLocation(TEXT("VB pelvis"));
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

void UHitReactionComponent::SimFinish()
{
    if (!Mesh) return;
    Mesh->SetAllBodiesBelowSimulatePhysics(TEXT("root"), false, true);
}

void UHitReactionComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    const bool bFinished = CurveTickValues(DeltaTime);

    if (bFinished)
    {
        if (!bSimFinishTriggered)
        {
            bSimFinishTriggered = true;

            SetupStrides(RestLeftFootPos, RestRightFootPos, true);
            if (ABP)
            {
                LFootState.StrideStartGoal = ABP->LeftFootIKPosition;
                RFootState.StrideStartGoal = ABP->RightFootIKPosition;
            }

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
                SetComponentTickEnabled(false);
                bSimFinishTriggered = false;
                bRepositioning      = false;
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
}
