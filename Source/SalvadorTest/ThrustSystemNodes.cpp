#include "ThrustSystemNodes.h"
#include "Engine/SkeletalMeshSocket.h"
#include "GameFramework/Character.h"

// ── AnimInstance property accessors via reflection ────────────────────────────
// Goal variable names are stored as FNames in FThrustState so the thrust system
// is not coupled to any concrete AnimInstance subclass.

static void SetAnimVec(UAnimInstance* Inst, FName PropName, const FVector& Val)
{
    if (!Inst || PropName == NAME_None) return;
    if (FProperty* P = Inst->GetClass()->FindPropertyByName(PropName))
        if (FStructProperty* SP = CastField<FStructProperty>(P))
            if (SP->Struct == TBaseStructure<FVector>::Get())
                *SP->ContainerPtrToValuePtr<FVector>(Inst) = Val;
}

static void SetAnimRot(UAnimInstance* Inst, FName PropName, const FRotator& Val)
{
    if (!Inst || PropName == NAME_None) return;
    if (FProperty* P = Inst->GetClass()->FindPropertyByName(PropName))
        if (FStructProperty* SP = CastField<FStructProperty>(P))
            if (SP->Struct == TBaseStructure<FRotator>::Get())
                *SP->ContainerPtrToValuePtr<FRotator>(Inst) = Val;
}

static void SetAnimFloat(UAnimInstance* Inst, FName PropName, float Val)
{
    if (!Inst || PropName == NAME_None) return;
    if (FProperty* P = Inst->GetClass()->FindPropertyByName(PropName))
        if (FFloatProperty* FP = CastField<FFloatProperty>(P))
            FP->SetPropertyValue_InContainer(Inst, Val);
}

static FVector GetAnimVec(UAnimInstance* Inst, FName PropName)
{
    if (!Inst || PropName == NAME_None) return FVector::ZeroVector;
    if (FProperty* P = Inst->GetClass()->FindPropertyByName(PropName))
        if (FStructProperty* SP = CastField<FStructProperty>(P))
            if (SP->Struct == TBaseStructure<FVector>::Get())
                return *SP->ContainerPtrToValuePtr<FVector>(Inst);
    return FVector::ZeroVector;
}

static FRotator GetAnimRot(UAnimInstance* Inst, FName PropName)
{
    if (!Inst || PropName == NAME_None) return FRotator::ZeroRotator;
    if (FProperty* P = Inst->GetClass()->FindPropertyByName(PropName))
        if (FStructProperty* SP = CastField<FStructProperty>(P))
            if (SP->Struct == TBaseStructure<FRotator>::Get())
                return *SP->ContainerPtrToValuePtr<FRotator>(Inst);
    return FRotator::ZeroRotator;
}

static float GetAnimFloat(UAnimInstance* Inst, FName PropName)
{
    if (!Inst || PropName == NAME_None) return 0.f;
    if (FProperty* P = Inst->GetClass()->FindPropertyByName(PropName))
        if (FFloatProperty* FP = CastField<FFloatProperty>(P))
            return FP->GetPropertyValue_InContainer(Inst);
    return 0.f;
}


void UThrustSystemNodes::ThrustSetUp(
    FThrustState& State,
    AActor* AttackerActor,
    AActor* VictimActor,
    FName DomLocGoal,
    FName DomRotGoal,
    FName SlaveLocGoal,
    FName SlaveRotGoal,
    const TArray<FName>& ContactSockets,
    FVector HitLocation,
    FName TargetBoneName,
    float HitReachDelay,
    float PlantDuration,
    UAnimMontage* Montage,
    float MontageCurrentPos,
    float MontagePlayRate,
    float RecoverDuration,
    FName LimitBone,
    float MaxDistFromBone,
    float StabDepth,
    float ArmReachPercent,
    FName HipLocGoal,
    float HipFollowPercent,
    float ArmRecoverDuration,
    float HipRecoverDuration,
    bool bDebug,
    FName PelvisBoneName,
    FName SpineRotVarName,
    FName SpineAlphaVarName,
    TEnumAsByte<ECollisionChannel> StabTraceChannel)
{
    if (!AttackerActor || ContactSockets.Num() == 0 || HitReachDelay <= 0.f)
        return;

    USkeletalMeshComponent* AttackerMesh = nullptr;
    if (ACharacter* Char = Cast<ACharacter>(AttackerActor))
        AttackerMesh = Char->GetMesh();
    if (!AttackerMesh)
        AttackerMesh = AttackerActor->FindComponentByClass<USkeletalMeshComponent>();
    if (!AttackerMesh) return;

    UAnimInstance* AnimInst = AttackerMesh->GetAnimInstance();
    if (!AnimInst) return;

    USkeletalMeshComponent* VictimMesh = VictimActor
        ? VictimActor->FindComponentByClass<USkeletalMeshComponent>()
        : nullptr;

    // Prefer the live bone position over HitLocation when a victim mesh and bone name are available.
    const FVector RawTargetWorld = (VictimMesh && TargetBoneName != NAME_None)
        ? VictimMesh->GetSocketLocation(TargetBoneName)
        : HitLocation;

    // Pick the contact socket that is closest to the hit location (e.g., tip vs base of blade).
    FName   ClosestSocket      = ContactSockets[0];
    FVector ClosestSocketWorld = AttackerMesh->GetSocketLocation(ContactSockets[0]);
    float   MinDistSq          = FVector::DistSquared(HitLocation, ClosestSocketWorld);
    for (int32 i = 1; i < ContactSockets.Num(); ++i)
    {
        const FVector SW  = AttackerMesh->GetSocketLocation(ContactSockets[i]);
        const float   DSq = FVector::DistSquared(HitLocation, SW);
        if (DSq < MinDistSq) { MinDistSq = DSq; ClosestSocketWorld = SW; ClosestSocket = ContactSockets[i]; }
    }

    // Line-trace from the closest socket to the target to find the actual surface impact point,
    // then push StabDepth into the surface so the blade visually penetrates.
    FVector TargetWorld      = RawTargetWorld;
    FVector TargetBoneOffset = FVector::ZeroVector;
    {
        UWorld* TraceWorld = AttackerMesh->GetWorld();
        FHitResult Hit;
        FCollisionQueryParams Params(NAME_None, false, AttackerActor);
        if (TraceWorld && TraceWorld->LineTraceSingleByChannel(
                Hit, ClosestSocketWorld, RawTargetWorld, StabTraceChannel, Params))
        {
            const FVector TraceDir = (RawTargetWorld - ClosestSocketWorld).GetSafeNormal();
            TargetWorld      = Hit.ImpactPoint + TraceDir * StabDepth;
            TargetBoneOffset = TargetWorld - RawTargetWorld;
        }
    }

    const USkeletalMeshSocket* Sock      = AttackerMesh->GetSocketByName(ClosestSocket);
    const FName                PivotBone = Sock ? Sock->BoneName : NAME_None;
    const FVector              PivotWorld = (PivotBone != NAME_None)
        ? AttackerMesh->GetBoneLocation(PivotBone)
        : AttackerMesh->GetComponentLocation();

    const FTransform CompTW_S = AttackerMesh->GetComponentTransform();
    {
        const FTransform PivotBoneTM = (PivotBone != NAME_None)
            ? AttackerMesh->GetSocketTransform(PivotBone, RTS_World)
            : FTransform(CompTW_S);
        State.DomRestPos = CompTW_S.InverseTransformPosition(PivotBoneTM.GetLocation());
        State.DomRestRot = (CompTW_S.GetRotation().Inverse() * PivotBoneTM.GetRotation()).Rotator();
        // Socket offset in hand-local space; constant so it can reconstruct the world hand pos
        // from the rotation alone during ThrustPlant without re-querying the skeleton.
        State.SocketRelativeLocation = PivotBoneTM.GetRotation().Inverse().RotateVector(
            ClosestSocketWorld - PivotBoneTM.GetLocation());
    }
    State.SlaveRestPos = State.DomRestPos;
    State.SlaveRestRot = State.DomRestRot;

    State.PelvisBoneName   = PelvisBoneName;
    State.SpineRotVarName  = SpineRotVarName;
    State.SpineAlphaVarName = SpineAlphaVarName;
    State.HipRestPosCS     = (PelvisBoneName != NAME_None)
        ? CompTW_S.InverseTransformPosition(AttackerMesh->GetBoneLocation(PelvisBoneName))
        : FVector::ZeroVector;

    const FVector CurrentDir   = (ClosestSocketWorld - PivotWorld).GetSafeNormal();
    const FVector RawTargetDir = (TargetWorld        - PivotWorld).GetSafeNormal();

    FVector TargetDir = RawTargetDir;
    // If the target is behind the pivot, clamp to the tangent perpendicular to CurrentDir to
    // avoid flipping the rotation 180 degrees and producing an unnatural over-reach.
    if (FVector::DotProduct(CurrentDir, RawTargetDir) < 0.f)
    {
        const FVector Tangent = (RawTargetDir - FVector::DotProduct(RawTargetDir, CurrentDir) * CurrentDir).GetSafeNormal();
        TargetDir = Tangent.IsNearlyZero() ? CurrentDir : Tangent;
    }

    // Compute the rotation delta in component space: conjugate the world-space delta into CS
    // so subsequent slerp stays in the same coordinate frame as the additive IK goals.
    const FQuat DeltaWorld    = FQuat::FindBetweenVectors(CurrentDir, TargetDir);
    const FQuat CompWorldQuat = CompTW_S.GetRotation();
    const FQuat DeltaCS       = CompWorldQuat.Inverse() * DeltaWorld * CompWorldQuat;
    const FQuat StartRotCS    = State.DomRestRot.Quaternion();
    const FQuat TargetRotCS   = DeltaCS * StartRotCS;

    State.AttackerMesh     = AttackerMesh;
    State.VictimActor      = VictimActor;
    State.DomLocGoal       = DomLocGoal;
    State.DomRotGoal       = DomRotGoal;
    State.SlaveLocGoal     = SlaveLocGoal;
    State.SlaveRotGoal     = SlaveRotGoal;
    State.PivotBone        = PivotBone;
    State.ContactSocket    = ClosestSocket;
    State.TargetBone       = TargetBoneName;
    State.TargetBoneWorld  = TargetWorld;
    State.TargetBoneOffset = TargetBoneOffset;
    State.DomStartRotCS    = StartRotCS.Rotator();
    State.DomTargetRotCS   = TargetRotCS.Rotator();
    State.HitReachDelay    = HitReachDelay;
    State.PlantDuration    = PlantDuration;
    State.PlantElapsed     = 0.f;
    State.Montage          = Montage;
    State.MontagePos       = FMath::Max(0.f, MontageCurrentPos);
    State.MontageRate      = FMath::Abs(MontagePlayRate) > KINDA_SMALL_NUMBER ? MontagePlayRate : 1.f;
    State.RecoverDuration  = RecoverDuration;
    State.TotalFrames      = 0;
    State.FramesRemaining  = 0;
    State.bActive           = true;
    State.bPlanted          = false;
    State.PlantedRotCS      = FRotator::ZeroRotator;
    State.bRecovering       = false;
    State.bMontageReversing = false;
    State.LimitBone        = LimitBone;
    State.MaxDistFromBone  = MaxDistFromBone;
    State.StabDepth        = StabDepth;
    State.ArmReachPercent  = ArmReachPercent;
    State.HipLocGoal          = HipLocGoal;
    State.HipFollowPercent    = HipFollowPercent;
    State.ArmRecoverDuration  = ArmRecoverDuration > 0.f ? ArmRecoverDuration : RecoverDuration;
    State.HipRecoverDuration  = HipRecoverDuration > 0.f ? HipRecoverDuration : RecoverDuration;
    State.bDebug              = bDebug;
    State.StabTraceChannel    = StabTraceChannel;

    // Zero all goals so the ABP starts from a clean additive offset (zero = animation pose).
    SetAnimVec(AnimInst, DomLocGoal,   FVector::ZeroVector);
    SetAnimRot(AnimInst, DomRotGoal,   FRotator::ZeroRotator);
    SetAnimVec(AnimInst, SlaveLocGoal, FVector::ZeroVector);
    SetAnimRot(AnimInst, SlaveRotGoal, FRotator::ZeroRotator);
    if (HipLocGoal != NAME_None)
        SetAnimVec(AnimInst, HipLocGoal, FVector::ZeroVector);
    if (SpineAlphaVarName != NAME_None)
        SetAnimFloat(AnimInst, SpineAlphaVarName, 0.f);
}

void UThrustSystemNodes::ThrustTick(
    FThrustState& State,
    float DeltaTime,
    bool& bOutComplete)
{
    bOutComplete = false;

    if (!State.bActive || !State.AttackerMesh) return;

    UAnimInstance* AnimInst = State.AttackerMesh->GetAnimInstance();
    if (!AnimInst) return;

    if (State.TotalFrames == 0)
    {
        State.TotalFrames     = FMath::Max(1, FMath::RoundToInt(State.HitReachDelay / DeltaTime));
        State.FramesRemaining = State.TotalFrames;
    }

    const int32 CurrentFrame = State.TotalFrames - State.FramesRemaining + 1;
    const float Alpha        = FMath::Clamp(float(CurrentFrame) / float(State.TotalFrames), 0.f, 1.f);

    const FQuat RestRotCS   = State.DomRestRot.Quaternion().GetNormalized();
    const FQuat TargetRotCS = State.DomTargetRotCS.Quaternion().GetNormalized();
    const FQuat RotAdditive = FQuat::Slerp(FQuat::Identity, (RestRotCS.Inverse() * TargetRotCS).GetNormalized(), Alpha).GetNormalized();

    // Re-query victim bone each frame so the approach tracks a moving target without a pop at
    // handoff to ThrustPlant (which also reads the live bone on its first frame).
    USkeletalMeshComponent* VictimMesh = State.VictimActor
        ? State.VictimActor->FindComponentByClass<USkeletalMeshComponent>()
        : nullptr;
    const FVector LiveTargetWorld = (VictimMesh && State.TargetBone != NAME_None)
        ? VictimMesh->GetSocketLocation(State.TargetBone) + State.TargetBoneOffset
        : State.TargetBoneWorld;

    // Socket-feedback accumulator scaled by Alpha: same convergence pattern as ThrustPlant so
    // the goal at Alpha=1 is identical to what ThrustPlant frame 1 would write (no pop at handoff).
    const FTransform CompTW         = State.AttackerMesh->GetComponentTransform();
    const FVector    SocketWorldCur = State.AttackerMesh->GetSocketLocation(State.ContactSocket);
    const FVector    SocketErrorCS  = CompTW.InverseTransformVector(LiveTargetWorld - SocketWorldCur);
    const FVector    PrevAdditive   = GetAnimVec(AnimInst, State.DomLocGoal);
    const FVector    DomAdditive    = (PrevAdditive + SocketErrorCS) * Alpha;

    SetAnimVec(AnimInst, State.DomLocGoal,   DomAdditive);
    SetAnimRot(AnimInst, State.DomRotGoal,   RotAdditive.Rotator());
    SetAnimVec(AnimInst, State.SlaveLocGoal, DomAdditive);
    SetAnimRot(AnimInst, State.SlaveRotGoal, FRotator::ZeroRotator);

    if (State.HipLocGoal != NAME_None && State.HipFollowPercent > 0.f)
        SetAnimVec(AnimInst, State.HipLocGoal, DomAdditive * State.HipFollowPercent);

    --State.FramesRemaining;
    if (State.FramesRemaining <= 0)
    {
        State.FramesRemaining = 0;
        bOutComplete          = true;
        State.bActive         = false;
        State.bPlanted        = true;
        State.PlantedRotCS    = State.DomTargetRotCS;
    }
}

void UThrustSystemNodes::ThrustPlant(
    FThrustState& State,
    float DeltaTime,
    bool& bOutComplete)
{
    bOutComplete = false;
    if (!State.AttackerMesh) return;

    UAnimInstance* AnimInst = State.AttackerMesh->GetAnimInstance();
    if (!AnimInst) return;

    const FTransform CompTW = State.AttackerMesh->GetComponentTransform();
    const FQuat      CompQ  = CompTW.GetRotation();

    USkeletalMeshComponent* VictimMesh = State.VictimActor
        ? State.VictimActor->FindComponentByClass<USkeletalMeshComponent>()
        : nullptr;

    if (State.FramesRemaining == 0)
    {
        // Capture world-space anchor on the first plant frame.
        State.PlantedTargetBoneWorld = (VictimMesh && State.TargetBone != NAME_None)
            ? VictimMesh->GetSocketLocation(State.TargetBone) + State.TargetBoneOffset
            : State.TargetBoneWorld;

        // World-anchored hand rotation: must be computed before PlantedHandWorldPos because
        // PlantedHandWorldPos is derived from this rotation.
        State.PlantedDomRotWorld = (CompQ * State.PlantedRotCS.Quaternion()).Rotator();

        // Reconstruct the world-space hand position from the rotation using the pre-computed
        // SocketRelativeLocation (hand-to-socket offset in hand-local space). Negating gives
        // socket-to-hand. This is stable under spine/camera rotation changes.
        State.PlantedHandWorldPos = -(State.PlantedDomRotWorld.Quaternion().RotateVector(State.SocketRelativeLocation));

        State.FramesRemaining = -1;
    }

    const FVector CurrentTargetWorld = (VictimMesh && State.TargetBone != NAME_None)
        ? VictimMesh->GetSocketLocation(State.TargetBone) + State.TargetBoneOffset
        : State.TargetBoneWorld;

    // Socket-feedback accumulator: read actual socket position (reflects last frame's IK result),
    // measure world-space error to the impact point, convert to CS additive. Converges in 1 frame
    // regardless of hand rotation or spine drift.
    const FVector SocketWorldCurrent = State.AttackerMesh->GetSocketLocation(State.ContactSocket);
    const FVector SocketErrorCS      = CompTW.InverseTransformVector(CurrentTargetWorld - SocketWorldCurrent);
    const FVector DomAdditive        = GetAnimVec(AnimInst, State.DomLocGoal) + SocketErrorCS;

    // Re-derive rotation additive in current CS each frame so the hand stays world-anchored
    // even as the character rotates (camera yaw, root motion, etc.).
    const FQuat DesiredHandRotCS = CompQ.Inverse() * State.PlantedDomRotWorld.Quaternion();
    const FQuat RotAdditive      = State.DomRestRot.Quaternion().Inverse() * DesiredHandRotCS;

    if (State.HipLocGoal != NAME_None && State.HipFollowPercent > 0.f)
        SetAnimVec(AnimInst, State.HipLocGoal, DomAdditive * State.HipFollowPercent);

    SetAnimVec(AnimInst, State.DomLocGoal,   DomAdditive);
    SetAnimRot(AnimInst, State.DomRotGoal,   RotAdditive.Rotator());
    SetAnimVec(AnimInst, State.SlaveLocGoal, DomAdditive);
    SetAnimRot(AnimInst, State.SlaveRotGoal, RotAdditive.Rotator());

    if (State.PlantDuration > 0.f)
    {
        State.PlantElapsed += DeltaTime;
        if (State.PlantElapsed >= State.PlantDuration)
            bOutComplete = true;
    }
    // Spine alpha ramps 0->1 over PlantDuration so the look-at fades in during the plant hold.
    const float PlantAlpha = (State.PlantDuration > 0.f)
        ? FMath::Clamp(State.PlantElapsed / State.PlantDuration, 0.f, 1.f)
        : 1.f;
    if (State.SpineAlphaVarName != NAME_None)
        SetAnimFloat(AnimInst, State.SpineAlphaVarName, PlantAlpha);
}

void UThrustSystemNodes::ThrustRecover(
    FThrustState& State,
    float DeltaTime,
    bool& bOutComplete)
{
    bOutComplete = false;

    if (!State.AttackerMesh) return;

    UAnimInstance* AnimInst = State.AttackerMesh->GetAnimInstance();
    if (!AnimInst) return;

    const bool bAlreadyRecovering = State.bRecovering;

    if (!State.bRecovering)
    {
        State.bRecovering            = true;
        State.RecoverDomStartPosCS   = GetAnimVec(AnimInst, State.DomLocGoal);
        State.RecoverDomStartRotCS   = GetAnimRot(AnimInst, State.DomRotGoal);
        State.RecoverSlaveStartPosCS = GetAnimVec(AnimInst, State.SlaveLocGoal);
        State.RecoverSlaveStartRotCS = GetAnimRot(AnimInst, State.SlaveRotGoal);
        State.RecoverHipStartPosCS   = GetAnimVec(AnimInst, State.HipLocGoal);
        State.RecoverArmElapsed      = 0.f;
        State.RecoverHipElapsed      = 0.f;

        if (State.Montage)
        {
            const float StartPos = State.MontagePos > KINDA_SMALL_NUMBER
                ? State.MontagePos
                : State.Montage->GetPlayLength();

            if (StartPos > KINDA_SMALL_NUMBER)
            {
                const float ReverseRate  = -FMath::Abs(State.MontageRate);
                const float RealDuration = StartPos / FMath::Abs(ReverseRate);
                State.MontagePos         = StartPos;
                State.bMontageReversing  = true;
                // Clamp arm/hip durations to the actual clip length so they don't outlast the montage.
                State.ArmRecoverDuration = FMath::Min(State.ArmRecoverDuration, RealDuration);
                State.HipRecoverDuration = FMath::Min(State.HipRecoverDuration, RealDuration);
                AnimInst->Montage_Play(State.Montage, ReverseRate,
                    EMontagePlayReturnType::MontageLength, StartPos, true);
            }
        }

        if (!State.bMontageReversing)
        {
            State.RecoverFramesTotal     = FMath::Max(1, FMath::RoundToInt(State.RecoverDuration / DeltaTime));
            State.RecoverFramesRemaining = State.RecoverFramesTotal;
        }
    }

    bool bDone = false;

    // Skip the completion check on the init frame: Montage_Play was just called and the animation
    // system has not evaluated yet, so Montage_GetPosition still returns the frozen position and
    // would falsely trigger "reached end," re-freezing the montage at position 0.
    if (bAlreadyRecovering)
    {
        if (State.bMontageReversing)
        {
            const float CurPos = AnimInst->Montage_GetPosition(State.Montage);
            if (CurPos <= KINDA_SMALL_NUMBER)
            {
                AnimInst->Montage_SetPlayRate(State.Montage, 0.f);
                bDone = true;
            }
        }
        else
        {
            --State.RecoverFramesRemaining;
            if (State.RecoverFramesRemaining <= 0)
                bDone = true;
        }
    }

    // Arms: lerp additive goals back to zero over ArmRecoverDuration.
    {
        State.RecoverArmElapsed += DeltaTime;
        const float ArmAlpha = FMath::Clamp(State.RecoverArmElapsed / State.ArmRecoverDuration, 0.f, 1.f);

        SetAnimVec(AnimInst, State.DomLocGoal,
            FMath::Lerp(State.RecoverDomStartPosCS, FVector::ZeroVector, ArmAlpha));
        SetAnimRot(AnimInst, State.DomRotGoal,
            FQuat::Slerp(State.RecoverDomStartRotCS.Quaternion().GetNormalized(), FQuat::Identity, ArmAlpha).GetNormalized().Rotator());
        SetAnimVec(AnimInst, State.SlaveLocGoal,
            FMath::Lerp(State.RecoverSlaveStartPosCS, FVector::ZeroVector, ArmAlpha));
        SetAnimRot(AnimInst, State.SlaveRotGoal,
            FQuat::Slerp(State.RecoverSlaveStartRotCS.Quaternion().GetNormalized(), FQuat::Identity, ArmAlpha).GetNormalized().Rotator());

        // Spine un-rotates in sync with arm recovery so head/spine arrive at rest together.
        if (State.SpineAlphaVarName != NAME_None)
            SetAnimFloat(AnimInst, State.SpineAlphaVarName, 1.f - ArmAlpha);
    }

    // Hip: independent duration so it can trail the arm recovery for added weight.
    if (State.HipLocGoal != NAME_None)
    {
        State.RecoverHipElapsed += DeltaTime;
        const float   HipAlpha   = FMath::Clamp(State.RecoverHipElapsed / State.HipRecoverDuration, 0.f, 1.f);
        SetAnimVec(AnimInst, State.HipLocGoal,
            FMath::Lerp(State.RecoverHipStartPosCS, FVector::ZeroVector, HipAlpha));
    }

    if (bDone)
    {
        // Guarantee all goals land at zero regardless of lerp precision.
        SetAnimVec(AnimInst, State.DomLocGoal,   FVector::ZeroVector);
        SetAnimRot(AnimInst, State.DomRotGoal,   FRotator::ZeroRotator);
        SetAnimVec(AnimInst, State.SlaveLocGoal, FVector::ZeroVector);
        SetAnimRot(AnimInst, State.SlaveRotGoal, FRotator::ZeroRotator);
        if (State.HipLocGoal != NAME_None)
            SetAnimVec(AnimInst, State.HipLocGoal, FVector::ZeroVector);
        if (State.SpineAlphaVarName != NAME_None)
            SetAnimFloat(AnimInst, State.SpineAlphaVarName, 0.f);
        bOutComplete = true;
    }
}

void UThrustSystemNodes::ThrustEnd(FThrustState& State)
{
    if (!State.AttackerMesh) return;

    UAnimInstance* AnimInst = State.AttackerMesh->GetAnimInstance();
    if (AnimInst)
    {
        SetAnimVec(AnimInst, State.DomLocGoal,   FVector::ZeroVector);
        SetAnimRot(AnimInst, State.DomRotGoal,   FRotator::ZeroRotator);
        SetAnimVec(AnimInst, State.SlaveLocGoal, FVector::ZeroVector);
        SetAnimRot(AnimInst, State.SlaveRotGoal, FRotator::ZeroRotator);
        if (State.HipLocGoal != NAME_None)
            SetAnimVec(AnimInst, State.HipLocGoal, FVector::ZeroVector);
        if (State.SpineAlphaVarName != NAME_None)
            SetAnimFloat(AnimInst, State.SpineAlphaVarName, 0.f);
    }

    State = FThrustState();
}
