#include "AnimBPNodes.h"

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