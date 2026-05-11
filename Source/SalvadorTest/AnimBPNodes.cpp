#include "AnimBPNodes.h"
#include "Curves/CurveVector.h"

void UAnimBPNodes::SampleCurve(
    FCurvePlayerState& State,
    UCurveFloat* Curve,
    float Duration,
    float DeltaTime,
    bool bReset,
    float& Value,
    bool& bFinished)
{
    Value = 0.f;
    bFinished = false;
    if (bReset)
        State.ElapsedTime = 0.f;
    if (!Curve || Duration <= 0.f)
        return;
    if (State.ElapsedTime < Duration)
        State.ElapsedTime += DeltaTime;
    if (State.ElapsedTime >= Duration)
    {
        State.ElapsedTime = Duration;
        bFinished = true;
    }
    float NormalizedTime = FMath::Clamp(State.ElapsedTime / Duration, 0.f, 1.f);
    Value = Curve->GetFloatValue(NormalizedTime);
}

void UAnimBPNodes::SampleVectorCurve(
    FCurvePlayerState& State,
    UCurveVector* Curve,
    float Duration,
    float DeltaTime,
    bool bReset,
    FVector& Value,
    bool& bFinished)
{
    Value = FVector::ZeroVector;
    bFinished = false;
    if (bReset)
        State.ElapsedTime = 0.f;
    if (!Curve || Duration <= 0.f)
        return;
    if (State.ElapsedTime < Duration)
        State.ElapsedTime += DeltaTime;
    if (State.ElapsedTime >= Duration)
    {
        State.ElapsedTime = Duration;
        bFinished = true;
    }
    float NormalizedTime = FMath::Clamp(State.ElapsedTime / Duration, 0.f, 1.f);
    Value = Curve->GetVectorValue(NormalizedTime);
}

void UAnimBPNodes::TriggerReactiveSteps(
    FReactiveStepsState& State,
    FVector LeftIKGoal,
    FVector RightIKGoal,
    FVector PelvisIKGoal,
    FVector ImpactDirection,
    float StepDistance,
    float ArcHeight,
    int32 NumSteps)
{
    FVector LeftOffset = LeftIKGoal - PelvisIKGoal;
    FVector RightOffset = RightIKGoal - PelvisIKGoal;

    LeftOffset.Z = FMath::Abs(LeftOffset.Z);
    RightOffset.Z = FMath::Abs(RightOffset.Z);

    State.LeftStart = LeftOffset;
    State.RightStart = RightOffset;

    FVector Delta = FVector(ImpactDirection.X, ImpactDirection.Y, 0.f)
        .GetSafeNormal() * StepDistance;

    State.LeftEnd = LeftOffset + Delta;
    State.RightEnd = RightOffset + Delta;


    State.LeftElapsed = 0.f;
    State.RightElapsed = 0.f;
    State.bLeftActive = false;
    State.bRightActive = false;
    State.StepsRemaining = FMath::Clamp(NumSteps, 1, 2);

    State.bLeftTurn = false;
    State.bRightActive = true;
    State.StepsRemaining--;
}

void UAnimBPNodes::TickReactiveSteps(
    FReactiveStepsState& State,
    float StepDuration,
    float DeltaTime,
    FVector& OutLeftIKGoal,
    FVector& OutRightIKGoal,
    bool& bAnyStepActive,
    bool& bStepJustFinished)
{
    bStepJustFinished = false;
    bAnyStepActive = State.bLeftActive || State.bRightActive
        || State.StepsRemaining > 0;

    if (State.bRightActive)
    {
        State.RightElapsed += DeltaTime;
        float Alpha = FMath::Clamp(State.RightElapsed / StepDuration, 0.f, 1.f);

        OutRightIKGoal = FMath::Lerp(State.RightStart, State.RightEnd, Alpha);
        OutRightIKGoal.Z += 12.f * FMath::Sin(Alpha * PI);

        if (Alpha >= 1.f)
        {
            State.bRightActive = false;
            State.RightStart = State.RightEnd;
            bStepJustFinished = true;

            if (State.StepsRemaining > 0)
            {
                State.bLeftActive = true;
                State.LeftElapsed = 0.f;
                State.StepsRemaining--;
            }
        }
    }
    else
    {
        OutRightIKGoal = State.RightStart;
    }

    if (State.bLeftActive)
    {
        State.LeftElapsed += DeltaTime;
        float Alpha = FMath::Clamp(State.LeftElapsed / StepDuration, 0.f, 1.f);

        OutLeftIKGoal = FMath::Lerp(State.LeftStart, State.LeftEnd, Alpha);
        OutLeftIKGoal.Z += 12.f * FMath::Sin(Alpha * PI);

        if (Alpha >= 1.f)
        {
            State.bLeftActive = false;
            State.LeftStart = State.LeftEnd;
            bStepJustFinished = true;

            if (State.StepsRemaining > 0)
            {
                State.bRightActive = true;
                State.RightElapsed = 0.f;
                State.StepsRemaining--;
            }
        }
    }
    else
    {
        OutLeftIKGoal = State.LeftStart;
    }

    bAnyStepActive = State.bLeftActive || State.bRightActive
        || State.StepsRemaining > 0;
}