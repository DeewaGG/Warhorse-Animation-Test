#include "AnimBPNodes.h"
#include "Curves/CurveFloat.h"

static float RandFromRange(FVector2D Range)
{
    return FMath::RandRange(Range.X, Range.Y);
}

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

    OutValue = Curve->GetFloatValue(FMath::Clamp(State.Elapsed / Duration, 0.f, 1.f));
}

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

        // Todo en IK goal space: interpola desde el goal actual hasta NeutralGoal
        FVector2D StartXY = FVector2D(Foot.StrideStartGoal.X, Foot.StrideStartGoal.Y);
        FVector2D NeutralXY = FVector2D(Foot.NeutralGoal.X, Foot.NeutralGoal.Y);

        // StrideReach: sobrepasa el neutral en la dirección del paso
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