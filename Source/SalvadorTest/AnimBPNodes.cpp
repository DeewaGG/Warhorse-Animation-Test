#include "AnimBPNodes.h"
#include "Curves/CurveFloat.h"
#include "DrawDebugHelpers.h"
#include "Engine/SkeletalMeshSocket.h"
#include "GameFramework/Actor.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

DEFINE_LOG_CATEGORY(LogSwordIK);

static void WriteIKLog(const FString& Text)
{
    const FString Path = FPaths::ProjectDir() / TEXT("SwordIKDebug.log");
    FFileHelper::SaveStringToFile(Text, *Path);
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
// SWORD CONTACT IK
// ─────────────────────────────────────────────

// Helper: escribe un FVector o FRotator en una variable del AnimBP por nombre
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

void UAnimBPNodes::SolveHandIK(
    FHandIKState& State,
    USkeletalMeshComponent* AttackerMesh,
    const TArray<FName>& ContactSockets,
    FVector HitLocation,
    AActor* VictimActor,
    FName TargetBoneName,
    FName DomIKLoc,
    FName DomIKRot,
    FName SlaveIKLoc,
    FName SlaveIKRot,
    float Duration)
{
    if (!AttackerMesh || ContactSockets.Num() == 0 || Duration <= 0.f)
        return;

    USkeletalMeshComponent* VictimMesh = VictimActor
        ? VictimActor->FindComponentByClass<USkeletalMeshComponent>()
        : nullptr;

    const FVector TargetBoneWorld = VictimMesh ? VictimMesh->GetBoneLocation(TargetBoneName) : HitLocation;

    // Socket más cercano al hit point
    FName   ClosestSocketName  = ContactSockets[0];
    FVector ClosestSocketWorld = AttackerMesh->GetSocketLocation(ContactSockets[0]);
    float   MinDistSq          = FVector::DistSquared(HitLocation, ClosestSocketWorld);
    for (int32 i = 1; i < ContactSockets.Num(); ++i)
    {
        const FVector SW  = AttackerMesh->GetSocketLocation(ContactSockets[i]);
        const float   DSq = FVector::DistSquared(HitLocation, SW);
        if (DSq < MinDistSq) { MinDistSq = DSq; ClosestSocketWorld = SW; ClosestSocketName = ContactSockets[i]; }
    }

    // Pivot: hueso padre del socket — mismo cálculo que HandBone pero sin parámetro
    const USkeletalMeshSocket* Sock       = AttackerMesh->GetSocketByName(ClosestSocketName);
    const FName                PivotBone  = Sock ? Sock->BoneName : NAME_None;
    const FVector              PivotWorld = (PivotBone != NAME_None)
        ? AttackerMesh->GetBoneLocation(PivotBone)
        : AttackerMesh->GetComponentLocation();

    const FVector CurrentSocketDir = (ClosestSocketWorld - PivotWorld).GetSafeNormal();
    const FVector DesiredDir       = (TargetBoneWorld    - PivotWorld).GetSafeNormal();
    const FQuat   DeltaQuat        = FQuat::FindBetweenVectors(CurrentSocketDir, DesiredDir);
    const FRotator FinalRotation   = DeltaQuat.Rotator();

    AActor*          AttackerOwner  = AttackerMesh->GetOwner();
    const FTransform PivotTransform = AttackerOwner
        ? AttackerOwner->GetActorTransform()
        : AttackerMesh->GetComponentTransform();
    const FQuat CompWorldQuat = AttackerMesh->GetComponentTransform().GetRotation();

    // Delta de rotación expresado en component space del mesh (lo que espera el AnimBP)
    const FQuat DeltaComp = CompWorldQuat.Inverse() * DeltaQuat * CompWorldQuat;

    State.Mesh                      = AttackerMesh;
    State.AttackerActor             = AttackerOwner;
    State.HandBoneName              = PivotBone;
    State.GoalPropertyName          = DomIKLoc;
    State.RotationPropertyName      = DomIKRot;
    State.StartHandGoalLocal        = PivotTransform.InverseTransformPosition(PivotWorld);
    State.FinalHandGoalLocal        = PivotTransform.InverseTransformPosition(PivotWorld);
    State.StartHandRotLocal         = FRotator::ZeroRotator;    // sin delta al inicio
    State.FinalHandRotLocal         = DeltaComp.Rotator();      // delta completo al final
    State.LastPosDelta              = FVector::ZeroVector;
    State.LastRotDelta              = FRotator::ZeroRotator;
    State.Duration                  = Duration;
    State.TotalFrames               = 0;
    State.FramesRemaining           = 0;
    State.bActive                   = true;
    State.bHasSlave                 = (SlaveIKLoc != NAME_None || SlaveIKRot != NAME_None);
    State.SlaveGoalPropertyName     = SlaveIKLoc;
    State.SlaveRotationPropertyName = SlaveIKRot;
    State.VictimActor               = VictimActor;
    State.VictimBoneName            = TargetBoneName;

#if !UE_BUILD_SHIPPING
    {
        UWorld* World = AttackerMesh->GetWorld();
        FString Log;
        Log += TEXT("======= SolveHandIK =======\n");
        Log += FString::Printf(TEXT("TargetBone  [%s]  (%.2f, %.2f, %.2f)\n"),
            *TargetBoneName.ToString(), TargetBoneWorld.X, TargetBoneWorld.Y, TargetBoneWorld.Z);
        for (int32 i = 0; i < ContactSockets.Num(); ++i)
        {
            const FVector SW = AttackerMesh->GetSocketLocation(ContactSockets[i]);
            const bool bClosest = SW.Equals(ClosestSocketWorld, 0.1f);
            Log += FString::Printf(TEXT("  Socket[%d] [%s]  dist=%.2f%s\n"),
                i, *ContactSockets[i].ToString(), FVector::Dist(TargetBoneWorld, SW),
                bClosest ? TEXT("  <-- CLOSEST") : TEXT(""));
            if (World)
                DrawDebugSphere(World, SW, 2.f, 8, bClosest ? FColor::Cyan : FColor::Red, false, 3.f);
        }
        Log += FString::Printf(TEXT("FinalRot  (P=%.2f Y=%.2f R=%.2f)  Duration=%.2fs  Slave=%s\n"),
            FinalRotation.Pitch, FinalRotation.Yaw, FinalRotation.Roll,
            Duration, State.bHasSlave ? TEXT("yes") : TEXT("no"));
        WriteIKLog(Log);
        if (World)
        {
            DrawDebugLine(World, ClosestSocketWorld, TargetBoneWorld, FColor::Yellow, false, 3.f, 0, 0.5f);
            if (VictimMesh)
            {
                DrawDebugSphere(World, TargetBoneWorld, 4.f, 12, FColor::Black, false, 3.f);
                DrawDebugBox   (World, TargetBoneWorld, FVector(5.f, 5.f, 100.f), FQuat::Identity, FColor::Black, false, 3.f);
            }
        }
    }
#endif
}

void UAnimBPNodes::TickHandIK(FHandIKState& State, float DeltaTime)
{
    if (!State.bActive || !State.Mesh)
        return;

    if (State.TotalFrames == 0)
    {
        State.TotalFrames     = FMath::Max(1, FMath::RoundToInt(State.Duration / DeltaTime));
        State.FramesRemaining = State.TotalFrames;
    }

    UAnimInstance* AnimInst = State.Mesh->GetAnimInstance();

    State.FramesRemaining--;
    if (State.FramesRemaining <= 0)
    {
        State.FramesRemaining = 0;
        State.bActive         = false;
    }

    const float Alpha = FMath::Clamp(
        1.f - (float)State.FramesRemaining / (float)State.TotalFrames, 0.f, 1.f);

    const FTransform PivotTransform = State.AttackerActor
        ? State.AttackerActor->GetActorTransform()
        : State.Mesh->GetComponentTransform();

    const FTransform CompToWorld = State.Mesh->GetComponentTransform();

    // ── POSICIÓN ──────────────────────────────────────────────────────────
    FVector TargetPosWorld;
    if (State.VictimActor)
    {
        USkeletalMeshComponent* VictimMesh = State.VictimActor->FindComponentByClass<USkeletalMeshComponent>();
        const FVector FinalPosWorld = (VictimMesh && State.VictimBoneName != NAME_None)
            ? VictimMesh->GetBoneLocation(State.VictimBoneName)
            : State.VictimActor->GetActorLocation();
        const FVector StartPosWorld = PivotTransform.TransformPosition(State.StartHandGoalLocal);
        TargetPosWorld = FMath::Lerp(StartPosWorld, FinalPosWorld, Alpha);
    }
    else
    {
        const FVector LocalGoal = FMath::Lerp(State.StartHandGoalLocal, State.FinalHandGoalLocal, Alpha);
        TargetPosWorld = PivotTransform.TransformPosition(LocalGoal);
    }

    const FVector CurBoneWorld    = State.Mesh->GetBoneLocation(State.HandBoneName);
    const FVector BaseAnimWorld   = CurBoneWorld - CompToWorld.TransformVector(State.LastPosDelta);
    const FVector NewPosDeltaComp = CompToWorld.InverseTransformVector(TargetPosWorld - BaseAnimWorld);
    State.LastPosDelta = NewPosDeltaComp;

    // ── ROTACIÓN ──────────────────────────────────────────────────────────
    const FQuat RotDelta = FQuat::Slerp(FQuat::Identity, FQuat(State.FinalHandRotLocal), Alpha);

    SetAnimVec(AnimInst, State.GoalPropertyName,     NewPosDeltaComp);
    SetAnimRot(AnimInst, State.RotationPropertyName, RotDelta.Rotator());

    if (State.bHasSlave)
    {
        SetAnimVec(AnimInst, State.SlaveGoalPropertyName,     NewPosDeltaComp);
        SetAnimRot(AnimInst, State.SlaveRotationPropertyName, RotDelta.Rotator());
    }
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

    Foot.ActiveDuration = RandFromRange(StrideDuration);
    Foot.ActiveHeight = RandFromRange(StrideHeight);
    Foot.ActiveReach = RandFromRange(StrideReach);
    Foot.ActiveCooldown = RandFromRange(StrideCooldown);
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

        FVector2D StartXY = FVector2D(Foot.StrideStartGoal.X, Foot.StrideStartGoal.Y);
        FVector2D NeutralXY = FVector2D(Foot.NeutralGoal.X, Foot.NeutralGoal.Y);
        FVector2D StrideDir = (NeutralXY - StartXY).GetSafeNormal();
        FVector2D TargetXY = NeutralXY + StrideDir * Foot.ActiveReach;
        FVector2D CurrentXY = FMath::Lerp(StartXY, TargetXY, Alpha);

        OutGoal = FVector(
            CurrentXY.X,
            CurrentXY.Y,
            Foot.NeutralGoal.Z + Foot.ActiveHeight * FMath::Sin(Alpha * PI)
        );

        if (Alpha >= 1.f)
        {
            Foot.AnchorWorldPos = ActorWorldPos;
            Foot.AnchorGoal = FVector(TargetXY.X, TargetXY.Y, Foot.NeutralGoal.Z);
            Foot.bAnchored = true;
            Foot.bStriding = false;
            Foot.CooldownTimer = Foot.ActiveCooldown;

            Foot.ActiveDuration = RandFromRange(Foot.StrideDuration);
            Foot.ActiveHeight = RandFromRange(Foot.StrideHeight);
            Foot.ActiveReach = RandFromRange(Foot.StrideReach);
            Foot.ActiveCooldown = RandFromRange(Foot.StrideCooldown);
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
                Foot.StrideElapsed = 0.f;
                Foot.bStriding = true;
                Foot.bForceStride = false;
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
    OutLeftGoal = LeftFoot.AnchorGoal;
    OutRightGoal = RightFoot.AnchorGoal;

    if (!LeftFoot.Mesh)
        return;

    FVector LeftBoneWorld = LeftFoot.Mesh->GetBoneLocation(LeftFoot.FootBone);
    FVector RightBoneWorld = RightFoot.Mesh->GetBoneLocation(RightFoot.FootBone);
    FVector HipBoneWorld = LeftFoot.Mesh->GetBoneLocation(LeftFoot.HipBone);

    bool bAnyBusy = LeftFoot.bStriding || RightFoot.bStriding
        || LeftFoot.CooldownTimer > 0.f
        || RightFoot.CooldownTimer > 0.f;

    float LeftDistToHip = FVector2D(
        LeftBoneWorld.X - HipBoneWorld.X,
        LeftBoneWorld.Y - HipBoneWorld.Y).Size();

    float RightDistToHip = FVector2D(
        RightBoneWorld.X - HipBoneWorld.X,
        RightBoneWorld.Y - HipBoneWorld.Y).Size();

    if (LeftDistToHip >= RightDistToHip)
    {
        SolveFoot(LeftFoot, ActorWorldPos, ActorWorldRot,
            LeftBoneWorld, HipBoneWorld, DeltaTime, bAnyBusy, OutLeftGoal);

        bAnyBusy = LeftFoot.bStriding || RightFoot.bStriding
            || LeftFoot.CooldownTimer > 0.f
            || RightFoot.CooldownTimer > 0.f;

        SolveFoot(RightFoot, ActorWorldPos, ActorWorldRot,
            RightBoneWorld, HipBoneWorld, DeltaTime, bAnyBusy, OutRightGoal);
    }
    else
    {
        SolveFoot(RightFoot, ActorWorldPos, ActorWorldRot,
            RightBoneWorld, HipBoneWorld, DeltaTime, bAnyBusy, OutRightGoal);

        bAnyBusy = LeftFoot.bStriding || RightFoot.bStriding
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
        && LeftFoot.CooldownTimer <= 0.f
        && RightFoot.CooldownTimer <= 0.f;
}
