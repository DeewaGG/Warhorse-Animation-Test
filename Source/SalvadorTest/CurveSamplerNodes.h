#pragma once
#include "CoreMinimal.h"
#include "Curves/CurveFloat.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CurveSamplerNodes.generated.h"

// Minimal state for SampleCurve; the ABP or component owns this struct.
USTRUCT(BlueprintType)
struct FCurveSamplerState
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, Category = "CurveSampler")
    float Elapsed = 0.f;
};

UCLASS()
class SALVADORTEST_API UCurveSamplerNodes : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:

    // Stateless time-based curve sampler. Pass bReset = true to restart from the beginning.
    // OutValue is 0 and bOutFinished is false while Elapsed < Duration; bOutFinished fires exactly
    // once on the frame Elapsed reaches Duration.
    UFUNCTION(BlueprintCallable, Category = "CurveSampler")
    static void SampleCurve(
        UPARAM(ref) FCurveSamplerState& State,
        UCurveFloat* Curve,
        float Duration,
        float DeltaTime,
        bool bReset,
        float& OutValue,
        bool& bOutFinished
    );
};
