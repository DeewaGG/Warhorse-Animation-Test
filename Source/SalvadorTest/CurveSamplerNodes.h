#pragma once
#include "CoreMinimal.h"
#include "Curves/CurveFloat.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CurveSamplerNodes.generated.h"

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
