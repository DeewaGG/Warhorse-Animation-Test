#pragma once

#include "CoreMinimal.h"
#include "Curves/CurveFloat.h"
#include "Curves/CurveVector.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "AnimBPNodes.generated.h"

USTRUCT(BlueprintType)
struct FCurvePlayerState
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, Category = "Curve")
    float ElapsedTime = 0.f;
};

UCLASS()
class SALVADORTEST_API UAnimBPNodes : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "AnimBPNodes")
    static void SampleCurve(
        UPARAM(ref) FCurvePlayerState& State,
        UCurveFloat* Curve,
        float Duration,
        float DeltaTime,
        bool bReset,
        float& Value,
        bool& bFinished
    );

    UFUNCTION(BlueprintCallable, Category = "AnimBPNodes")
    static void SampleVectorCurve(
        UPARAM(ref) FCurvePlayerState& State,
        UCurveVector* Curve,
        float Duration,
        float DeltaTime,
        bool bReset,
        FVector& Value,
        bool& bFinished
    );
};