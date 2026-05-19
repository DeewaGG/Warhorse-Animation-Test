#include "CurveSamplerNodes.h"

void UCurveSamplerNodes::SampleCurve(
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

    // Normalize elapsed to [0,1] so the curve asset is independent of Duration.
    OutValue = Curve->GetFloatValue(
        FMath::Clamp(State.Elapsed / Duration, 0.f, 1.f));
}
