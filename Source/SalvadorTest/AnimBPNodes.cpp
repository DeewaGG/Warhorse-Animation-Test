#include "AnimBPNodes.h"
#include "Curves/CurveFloat.h"
#include "DrawDebugHelpers.h"
#include "Engine/SkeletalMeshSocket.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Character.h"

// ─────────────────────────────────────────────
// ANIM INSTANCE PROPERTY HELPERS
// ─────────────────────────────────────────────

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

static float RandFromRange(FVector2D Range)
{
    return FMath::RandRange(Range.X, Range.Y);
}

// ─────────────────────────────────────────────
// CURVE SAMPLER
// ─────────────────────────────────────────────

void UAnimBPNodes::SampleCurve(
    FCurveSamplerState& State,
    UCurveFloat* Curve,
    float Duration,
    float DeltaTime,
    bool bReset,
    float& OutValue,
    bool& bOutFinished)
{
    OutValue = 0.f;
    bOutFinished = false;

    if (bReset)
        State.Elapsed = 0.f;

    if (!Curve || Duration <= 0.f)
        return;

    if (State.Elapsed < Duration)
        State.Elapsed += DeltaTime;

    if (State.Elapsed >= Duration)
    {
        State.Elapsed = Duration;
        bOutFinished = true;
    }

    OutValue = Curve->GetFloatValue(
        FMath::Clamp(State.Elapsed / Duration, 0.f, 1.f));
}

// ─────────────────────────────────────────────
// THRUST SYSTEM
// ─────────────────────────────────────────────

void UAnimBPNodes::ThrustSetUp(
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
    float RecoverDuration,
    bool bDebug)
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

    // Target world position
    const FVector TargetWorld = (VictimMesh && TargetBoneName != NAME_None)
        ? VictimMesh->GetBoneLocation(TargetBoneName)
        : HitLocation;

    // Closest contact socket to hit location
    FName   ClosestSocket      = ContactSockets[0];
    FVector ClosestSocketWorld = AttackerMesh->GetSocketLocation(ContactSockets[0]);
    float   MinDistSq          = FVector::DistSquared(HitLocation, ClosestSocketWorld);
    for (int32 i = 1; i < ContactSockets.Num(); ++i)
    {
        const FVector SW  = AttackerMesh->GetSocketLocation(ContactSockets[i]);
        const float   DSq = FVector::DistSquared(HitLocation, SW);
        if (DSq < MinDistSq) { MinDistSq = DSq; ClosestSocketWorld = SW; ClosestSocket = ContactSockets[i]; }
    }

    // Pivot bone = parent bone of closest socket
    const USkeletalMeshSocket* Sock      = AttackerMesh->GetSocketByName(ClosestSocket);
    const FName                PivotBone = Sock ? Sock->BoneName : NAME_None;
    const FVector              PivotWorld = (PivotBone != NAME_None)
        ? AttackerMesh->GetBoneLocation(PivotBone)
        : AttackerMesh->GetComponentLocation();

    // Read A-pose defaults from ABP — goals must be at rest when ThrustSetUp is called
    State.DomRestPos   = GetAnimVec(AnimInst, DomLocGoal);
    State.DomRestRot   = GetAnimRot(AnimInst, DomRotGoal);
    State.SlaveRestPos = GetAnimVec(AnimInst, SlaveLocGoal);
    State.SlaveRestRot = GetAnimRot(AnimInst, SlaveRotGoal);

    // Rotation delta: current socket direction → target direction (world space)
    const FVector CurrentDir = (ClosestSocketWorld - PivotWorld).GetSafeNormal();
    const FVector TargetDir  = (TargetWorld         - PivotWorld).GetSafeNormal();
    const FQuat   DeltaWorld = FQuat::FindBetweenVectors(CurrentDir, TargetDir);

    // World delta → component space
    const FQuat CompWorldQuat = AttackerMesh->GetComponentTransform().GetRotation();
    const FQuat DeltaCS       = CompWorldQuat.Inverse() * DeltaWorld * CompWorldQuat;

    // Start = hand_r's IK goal CS rotation (DomRestRot). Using weapon_r's bone CS rotation
    // here would be wrong — the IK goal drives hand_r, not weapon_r.
    const FQuat StartRotCS  = State.DomRestRot.Quaternion();
    const FQuat TargetRotCS = DeltaCS * StartRotCS;

    // Fill state
    State.AttackerMesh    = AttackerMesh;
    State.VictimActor     = VictimActor;
    State.DomLocGoal      = DomLocGoal;
    State.DomRotGoal      = DomRotGoal;
    State.SlaveLocGoal    = SlaveLocGoal;
    State.SlaveRotGoal    = SlaveRotGoal;
    State.PivotBone       = PivotBone;
    State.TargetBone      = TargetBoneName;
    State.TargetBoneWorld = TargetWorld;
    State.DomStartRotCS   = StartRotCS.Rotator();
    State.DomTargetRotCS  = TargetRotCS.Rotator();
    State.HitReachDelay   = HitReachDelay;
    State.PlantDuration   = PlantDuration;
    State.PlantElapsed    = 0.f;
    State.Montage         = Montage;
    State.MontagePos      = MontageCurrentPos;
    State.RecoverDuration = RecoverDuration;
    State.TotalFrames     = 0;
    State.FramesRemaining = 0;
    State.bActive         = true;
    State.bPlanted        = false;
    State.PlantedRotCS    = FRotator::ZeroRotator;
    State.bDebug          = bDebug;

    if (bDebug)
    {
        UWorld* World = AttackerMesh->GetWorld();
        if (World)
        {
            for (const FName& SocketName : ContactSockets)
            {
                const FVector SW     = AttackerMesh->GetSocketLocation(SocketName);
                const bool    bClose = SocketName == ClosestSocket;
                DrawDebugSphere(World, SW, 3.f, 8,
                    bClose ? FColor::Cyan : FColor::Red, false, 4.f);
            }
            DrawDebugSphere(World, PivotWorld,  5.f, 8,  FColor::Green, false, 4.f);
            DrawDebugSphere(World, TargetWorld, 5.f, 12, FColor::Black, false, 4.f);
            DrawDebugBox   (World, TargetWorld, FVector(4.f, 4.f, 50.f), FQuat::Identity, FColor::Black, false, 4.f);
            DrawDebugLine  (World, ClosestSocketWorld, TargetWorld,            FColor::Yellow, false, 4.f, 0, 0.5f);
            DrawDebugLine  (World, PivotWorld, PivotWorld + CurrentDir * 40.f, FColor::Red,    false, 4.f, 0, 0.5f);
            DrawDebugLine  (World, PivotWorld, PivotWorld + TargetDir  * 40.f, FColor::Green,  false, 4.f, 0, 0.5f);
        }
    }
}

void UAnimBPNodes::ThrustTick(
    FThrustState& State,
    float DeltaTime,
    bool& bOutComplete)
{
    bOutComplete = false;

    if (!State.bActive || !State.AttackerMesh) return;

    UAnimInstance* AnimInst = State.AttackerMesh->GetAnimInstance();
    if (!AnimInst) return;

    // Initialize frame count on first tick
    if (State.TotalFrames == 0)
    {
        State.TotalFrames     = FMath::Max(1, FMath::RoundToInt(State.HitReachDelay / DeltaTime));
        State.FramesRemaining = State.TotalFrames;
    }

    const int32 CurrentFrame = State.TotalFrames - State.FramesRemaining + 1;
    const float Alpha        = FMath::Clamp(float(CurrentFrame) / float(State.TotalFrames), 0.f, 1.f);

    const FQuat    StartQ    = State.DomStartRotCS.Quaternion();
    const FQuat    TargetQ   = State.DomTargetRotCS.Quaternion();
    const FRotator LerpedRot = FQuat::Slerp(StartQ, TargetQ, Alpha).Rotator();

    SetAnimVec(AnimInst, State.DomLocGoal,   State.DomRestPos);
    SetAnimRot(AnimInst, State.DomRotGoal,   LerpedRot);
    SetAnimVec(AnimInst, State.SlaveLocGoal, State.SlaveRestPos);
    SetAnimRot(AnimInst, State.SlaveRotGoal, State.SlaveRestRot);

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

void UAnimBPNodes::ThrustPlant(
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

    // First frame: lock all values into world space and capture victim bone state
    if (State.FramesRemaining == 0)
    {
        // CS → World:  W = CompQ * CS
        State.PlantedDomHandWorld   = CompTW.TransformPosition(GetAnimVec(AnimInst, State.DomLocGoal));
        State.PlantedDomRotWorld    = (CompQ * GetAnimRot(AnimInst, State.DomRotGoal).Quaternion()).Rotator();
        State.PlantedSlaveHandWorld = CompTW.TransformPosition(GetAnimVec(AnimInst, State.SlaveLocGoal));
        State.PlantedSlaveRotWorld  = (CompQ * GetAnimRot(AnimInst, State.SlaveRotGoal).Quaternion()).Rotator();

        // Capture victim bone world state as the tracking origin
        State.PlantedTargetBoneWorld = (VictimMesh && State.TargetBone != NAME_None)
            ? VictimMesh->GetBoneLocation(State.TargetBone)
            : State.PlantedDomHandWorld;

        State.FramesRemaining = -1;
    }

    // Compute current hand world position and rotation, tracking the victim bone
    FVector CurrentDomHandWorld  = State.PlantedDomHandWorld;
    FQuat   CurrentDomRotWorld   = State.PlantedDomRotWorld.Quaternion();

    if (VictimMesh && State.TargetBone != NAME_None)
    {
        const FVector CurrentBoneWorld = VictimMesh->GetBoneLocation(State.TargetBone);

        // Position: only 10% of the victim's movement reaches the hand
        CurrentDomHandWorld = State.PlantedDomHandWorld
            + (CurrentBoneWorld - State.PlantedTargetBoneWorld) * 0.1f;

        // Rotation: recompute full blade direction each frame so the remaining
        // 90% of movement is absorbed by arm/wrist rotation
        const FVector PlantedBladeDir = (State.PlantedTargetBoneWorld - State.PlantedDomHandWorld).GetSafeNormal();
        const FVector CurrentBladeDir = (CurrentBoneWorld             - CurrentDomHandWorld).GetSafeNormal();
        const FQuat   BladeRotDelta   = FQuat::FindBetweenVectors(PlantedBladeDir, CurrentBladeDir);
        CurrentDomRotWorld            = BladeRotDelta * State.PlantedDomRotWorld.Quaternion();
    }

    // World → CS:  CS = CompQ.Inv * W
    SetAnimVec(AnimInst, State.DomLocGoal,
        CompTW.InverseTransformPosition(CurrentDomHandWorld));
    SetAnimRot(AnimInst, State.DomRotGoal,
        (CompQ.Inverse() * CurrentDomRotWorld).Rotator());
    SetAnimVec(AnimInst, State.SlaveLocGoal,
        CompTW.InverseTransformPosition(State.PlantedSlaveHandWorld));
    SetAnimRot(AnimInst, State.SlaveRotGoal,
        (CompQ.Inverse() * State.PlantedSlaveRotWorld.Quaternion()).Rotator());

    // PlantDuration timer — 0 means infinite
    if (State.PlantDuration > 0.f)
    {
        State.PlantElapsed += DeltaTime;
        if (State.PlantElapsed >= State.PlantDuration)
            bOutComplete = true;
    }
}

void UAnimBPNodes::ThrustRecover(
    FThrustState& State,
    float DeltaTime,
    bool& bOutComplete)
{
    bOutComplete = false;
    if (!State.AttackerMesh) return;

    UAnimInstance* AnimInst = State.AttackerMesh->GetAnimInstance();
    if (!AnimInst) return;

    // ── Phase 2: montage reversing ───────────────────────────────────────────
    if (State.bMontageReversing)
    {
        const float Pos = State.Montage
            ? AnimInst->Montage_GetPosition(State.Montage)
            : 0.f;

        if (Pos <= KINDA_SMALL_NUMBER)
        {
            if (State.Montage)
                AnimInst->Montage_SetPlayRate(State.Montage, 0.f);
            bOutComplete = true;
        }
        return;
    }

    // ── Phase 1: IK lerp back to rest ────────────────────────────────────────
    if (!State.bRecovering)
    {
        State.bRecovering            = true;
        State.RecoverDomStartRotCS   = GetAnimRot(AnimInst, State.DomRotGoal);
        State.RecoverSlaveStartRotCS = GetAnimRot(AnimInst, State.SlaveRotGoal);
        State.RecoverFramesTotal     = FMath::Max(1, FMath::RoundToInt(State.RecoverDuration / DeltaTime));
        State.RecoverFramesRemaining = State.RecoverFramesTotal;
    }

    const int32 CurrentFrame = State.RecoverFramesTotal - State.RecoverFramesRemaining + 1;
    const float Alpha = FMath::Clamp(float(CurrentFrame) / float(State.RecoverFramesTotal), 0.f, 1.f);

    const FRotator DomRotCS   = FQuat::Slerp(State.RecoverDomStartRotCS.Quaternion(),   State.DomRestRot.Quaternion(),   Alpha).Rotator();
    const FRotator SlaveRotCS = FQuat::Slerp(State.RecoverSlaveStartRotCS.Quaternion(), State.SlaveRestRot.Quaternion(), Alpha).Rotator();

    SetAnimVec(AnimInst, State.DomLocGoal,   State.DomRestPos);
    SetAnimRot(AnimInst, State.DomRotGoal,   DomRotCS);
    SetAnimVec(AnimInst, State.SlaveLocGoal, State.SlaveRestPos);
    SetAnimRot(AnimInst, State.SlaveRotGoal, SlaveRotCS);

    --State.RecoverFramesRemaining;
    if (State.RecoverFramesRemaining <= 0)
    {
        State.RecoverFramesRemaining = 0;

        if (State.Montage)
        {
            const float StartPos = State.MontagePos > KINDA_SMALL_NUMBER
                ? State.MontagePos
                : State.Montage->GetPlayLength();

            if (StartPos > KINDA_SMALL_NUMBER)
            {
                State.MontagePos        = StartPos;
                State.bMontageReversing = true;
                AnimInst->Montage_Play(State.Montage, -1.f,
                    EMontagePlayReturnType::MontageLength, StartPos, true);
            }
            else
            {
                bOutComplete = true;
            }
        }
        else
        {
            bOutComplete = true;
        }
    }
}

void UAnimBPNodes::ThrustEnd(FThrustState& State)
{
    if (!State.AttackerMesh) return;

    UAnimInstance* AnimInst = State.AttackerMesh->GetAnimInstance();
    if (AnimInst)
    {
        SetAnimVec(AnimInst, State.DomLocGoal,   State.DomRestPos);
        SetAnimRot(AnimInst, State.DomRotGoal,   State.DomRestRot);
        SetAnimVec(AnimInst, State.SlaveLocGoal, State.SlaveRestPos);
        SetAnimRot(AnimInst, State.SlaveRotGoal, State.SlaveRestRot);
    }

    State = FThrustState();
}

// ─────────────────────────────────────────────
// FOOT IK
// ─────────────────────────────────────────────

void UAnimBPNodes::SetupFootIK(
    FFootIKState& Foot,
    USkeletalMeshComponent* Mesh,
    FName FootBone,
    FName HipBone,
    FVector ActorWorldPos,
    FVector CurrentGoal,
    FVector2D StrideThreshold,
    FVector2D StrideDuration,
    FVector2D StrideHeight,
    FVector2D StrideCooldown,
    FVector2D StrideReach,
    bool bForceFirstStride)
{
    Foot.Mesh = Mesh;
    Foot.FootBone = FootBone;
    Foot.HipBone = HipBone;
    Foot.NeutralGoal = CurrentGoal;
    Foot.StrideThreshold = StrideThreshold;
    Foot.StrideDuration = StrideDuration;
    Foot.StrideHeight = StrideHeight;
    Foot.StrideCooldown = StrideCooldown;
    Foot.StrideReach = StrideReach;

    Foot.AnchorWorldPos = ActorWorldPos;
    Foot.AnchorGoal = CurrentGoal;
    Foot.bAnchored = true;
    Foot.bStriding = false;
    Foot.bForceStride = bForceFirstStride;
    Foot.StrideElapsed = 0.f;
    Foot.CooldownTimer = 0.f;

    Foot.ActiveDuration  = RandFromRange(StrideDuration);
    Foot.ActiveHeight    = RandFromRange(StrideHeight);
    Foot.ActiveReach     = RandFromRange(StrideReach);
    Foot.ActiveCooldown  = RandFromRange(StrideCooldown);
    Foot.ActiveThreshold = RandFromRange(StrideThreshold);
}

void UAnimBPNodes::SolveFoot(
    FFootIKState& Foot,
    FVector ActorWorldPos,
    FRotator ActorWorldRot,
    FVector FootBoneWorld,
    FVector HipBoneWorld,
    float DeltaTime,
    bool bAnyFootBusy,
    FVector& OutGoal)
{
    OutGoal = Foot.AnchorGoal;

    if (Foot.bStriding)
    {
        Foot.StrideElapsed += DeltaTime;
        float SafeDuration = FMath::Max(Foot.ActiveDuration, 0.01f);
        float Alpha = FMath::Clamp(Foot.StrideElapsed / SafeDuration, 0.f, 1.f);

        FVector2D StartXY   = FVector2D(Foot.StrideStartGoal.X, Foot.StrideStartGoal.Y);
        FVector2D NeutralXY = FVector2D(Foot.NeutralGoal.X,     Foot.NeutralGoal.Y);
        FVector2D StrideDir = (NeutralXY - StartXY).GetSafeNormal();
        FVector2D TargetXY  = NeutralXY + StrideDir * Foot.ActiveReach;
        FVector2D CurrentXY = FMath::Lerp(StartXY, TargetXY, Alpha);

        OutGoal = FVector(
            CurrentXY.X,
            CurrentXY.Y,
            Foot.NeutralGoal.Z + Foot.ActiveHeight * FMath::Sin(Alpha * PI)
        );

        if (Alpha >= 1.f)
        {
            Foot.AnchorWorldPos = ActorWorldPos;
            Foot.AnchorGoal     = FVector(TargetXY.X, TargetXY.Y, Foot.NeutralGoal.Z);
            Foot.bAnchored      = true;
            Foot.bStriding      = false;
            Foot.CooldownTimer  = Foot.ActiveCooldown;

            Foot.ActiveDuration  = RandFromRange(Foot.StrideDuration);
            Foot.ActiveHeight    = RandFromRange(Foot.StrideHeight);
            Foot.ActiveReach     = RandFromRange(Foot.StrideReach);
            Foot.ActiveCooldown  = RandFromRange(Foot.StrideCooldown);
            Foot.ActiveThreshold = RandFromRange(Foot.StrideThreshold);
        }
    }
    else
    {
        if (Foot.CooldownTimer > 0.f)
        {
            Foot.CooldownTimer -= DeltaTime;
            if (Foot.CooldownTimer < 0.f)
                Foot.CooldownTimer = 0.f;
        }

        FVector WorldDelta = ActorWorldPos - Foot.AnchorWorldPos;
        FVector LocalDelta = ActorWorldRot.UnrotateVector(WorldDelta);

        OutGoal = FVector(
            Foot.AnchorGoal.X - LocalDelta.X,
            Foot.AnchorGoal.Y - LocalDelta.Y,
            Foot.AnchorGoal.Z
        );

        if (!bAnyFootBusy)
        {
            float DistToHip = FVector2D(
                FootBoneWorld.X - HipBoneWorld.X,
                FootBoneWorld.Y - HipBoneWorld.Y).Size();

            if (Foot.bForceStride || DistToHip > Foot.ActiveThreshold)
            {
                Foot.StrideStartGoal = OutGoal;
                Foot.StrideElapsed   = 0.f;
                Foot.bStriding       = true;
                Foot.bForceStride    = false;
            }
        }
    }
}

void UAnimBPNodes::SolveFootIK(
    FFootIKState& LeftFoot,
    FFootIKState& RightFoot,
    FVector ActorWorldPos,
    FRotator ActorWorldRot,
    float DeltaTime,
    FVector& OutLeftGoal,
    FVector& OutRightGoal)
{
    OutLeftGoal  = LeftFoot.AnchorGoal;
    OutRightGoal = RightFoot.AnchorGoal;

    if (!LeftFoot.Mesh)
        return;

    FVector LeftBoneWorld  = LeftFoot.Mesh->GetBoneLocation(LeftFoot.FootBone);
    FVector RightBoneWorld = RightFoot.Mesh->GetBoneLocation(RightFoot.FootBone);
    FVector HipBoneWorld   = LeftFoot.Mesh->GetBoneLocation(LeftFoot.HipBone);

    bool bAnyBusy = LeftFoot.bStriding  || RightFoot.bStriding
                 || LeftFoot.CooldownTimer > 0.f
                 || RightFoot.CooldownTimer > 0.f;

    float LeftDistToHip  = FVector2D(LeftBoneWorld.X  - HipBoneWorld.X, LeftBoneWorld.Y  - HipBoneWorld.Y).Size();
    float RightDistToHip = FVector2D(RightBoneWorld.X - HipBoneWorld.X, RightBoneWorld.Y - HipBoneWorld.Y).Size();

    if (LeftDistToHip >= RightDistToHip)
    {
        SolveFoot(LeftFoot, ActorWorldPos, ActorWorldRot,
            LeftBoneWorld, HipBoneWorld, DeltaTime, bAnyBusy, OutLeftGoal);

        bAnyBusy = LeftFoot.bStriding  || RightFoot.bStriding
                || LeftFoot.CooldownTimer > 0.f
                || RightFoot.CooldownTimer > 0.f;

        SolveFoot(RightFoot, ActorWorldPos, ActorWorldRot,
            RightBoneWorld, HipBoneWorld, DeltaTime, bAnyBusy, OutRightGoal);
    }
    else
    {
        SolveFoot(RightFoot, ActorWorldPos, ActorWorldRot,
            RightBoneWorld, HipBoneWorld, DeltaTime, bAnyBusy, OutRightGoal);

        bAnyBusy = LeftFoot.bStriding  || RightFoot.bStriding
                || LeftFoot.CooldownTimer > 0.f
                || RightFoot.CooldownTimer > 0.f;

        SolveFoot(LeftFoot, ActorWorldPos, ActorWorldRot,
            LeftBoneWorld, HipBoneWorld, DeltaTime, bAnyBusy, OutLeftGoal);
    }
}

bool UAnimBPNodes::AreFeetRepositioned(
    const FFootIKState& LeftFoot,
    const FFootIKState& RightFoot)
{
    return !LeftFoot.bStriding
        && !RightFoot.bStriding
        && LeftFoot.CooldownTimer  <= 0.f
        && RightFoot.CooldownTimer <= 0.f;
}
