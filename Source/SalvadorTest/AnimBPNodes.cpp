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

void UAnimBPNodes::InitProceduralFoot(
    FProceduralFootState& State,
    FVector ActorWorldPosition,
    FVector CurrentIKGoal,
    float StepHeight)
{
    State.RestIKGoal = CurrentIKGoal;
    State.PlantedActorWorldPos = ActorWorldPosition;
    State.PlantedIKGoal = CurrentIKGoal;
    State.bIsPlanted = true;
    State.bIsStepping = false;
    State.StepElapsed = 0.f;
    State.StepHeight = StepHeight;
}

void UAnimBPNodes::UpdateProceduralFoot(
    FProceduralFootState& State,
    FVector ActorWorldPosition,
    FRotator ActorWorldRotation,
    float StepTriggerDistance,
    float StepDuration,
    float DeltaTime,
    bool bOtherFootStepping,
    FVector& OutIKGoal,
    bool& bOutIsStepping)
{
    if (State.bIsStepping)
    {
        State.StepElapsed += DeltaTime;
        float Alpha = FMath::Clamp(State.StepElapsed / StepDuration, 0.f, 1.f);

        // Target en world XY: posición natural relativa al actor actual
        FVector TargetWorldXY = ActorWorldPosition +
            ActorWorldRotation.RotateVector(
                FVector(State.RestIKGoal.X, State.RestIKGoal.Y, 0.f));

        // Lerp de la posición de inicio al target
        FVector FootWorldXY = FMath::Lerp(State.StepStartWorldXY, TargetWorldXY, Alpha);

        // Convierte world XY a IK goal
        FVector LocalOffset = ActorWorldRotation.UnrotateVector(
            FootWorldXY - ActorWorldPosition);

        OutIKGoal = FVector(
            LocalOffset.X,
            LocalOffset.Y,
            State.RestIKGoal.Z + State.StepHeight * FMath::Sin(Alpha * PI)
        );

        if (Alpha >= 1.f)
        {
            // Planta el pie en la nueva posición
            State.PlantedActorWorldPos = ActorWorldPosition;
            State.PlantedIKGoal = State.RestIKGoal;
            State.bIsPlanted = true;
            State.bIsStepping = false;
        }
    }
    else
    {
        // Fórmula de plantado normal
        FVector WorldDelta = ActorWorldPosition - State.PlantedActorWorldPos;
        FVector LocalDelta = ActorWorldRotation.UnrotateVector(WorldDelta);

        OutIKGoal = FVector(
            State.PlantedIKGoal.X - LocalDelta.X,
            State.PlantedIKGoal.Y - LocalDelta.Y,
            State.PlantedIKGoal.Z
        );

        // Trigger del paso: el actor se alejó demasiado del punto de plantado
        float DistFromPlant = FVector2D(WorldDelta.X, WorldDelta.Y).Size();
        if (DistFromPlant > StepTriggerDistance && !bOtherFootStepping)
        {
            // Captura la posición world actual del pie como inicio del paso
            State.StepStartWorldXY = State.PlantedActorWorldPos +
                ActorWorldRotation.RotateVector(
                    FVector(State.PlantedIKGoal.X, State.PlantedIKGoal.Y, 0.f));

            State.StepElapsed = 0.f;
            State.bIsStepping = true;
        }
    }

    bOutIsStepping = State.bIsStepping;
}
