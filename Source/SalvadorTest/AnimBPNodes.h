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

USTRUCT(BlueprintType)
struct FReactiveStepsState
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, Category = "Steps")
    FVector LeftStart = FVector::ZeroVector;

    UPROPERTY(BlueprintReadWrite, Category = "Steps")
    FVector LeftEnd = FVector::ZeroVector;

    UPROPERTY(BlueprintReadWrite, Category = "Steps")
    float LeftElapsed = 0.f;

    UPROPERTY(BlueprintReadWrite, Category = "Steps")
    bool bLeftActive = false;

    UPROPERTY(BlueprintReadWrite, Category = "Steps")
    FVector RightStart = FVector::ZeroVector;

    UPROPERTY(BlueprintReadWrite, Category = "Steps")
    FVector RightEnd = FVector::ZeroVector;

    UPROPERTY(BlueprintReadWrite, Category = "Steps")
    float RightElapsed = 0.f;

    UPROPERTY(BlueprintReadWrite, Category = "Steps")
    bool bRightActive = false;

    UPROPERTY(BlueprintReadWrite, Category = "Steps")
    bool bLeftTurn = false;

    UPROPERTY(BlueprintReadWrite, Category = "Steps")
    int32 StepsRemaining = 0;
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

    UFUNCTION(BlueprintCallable, Category = "AnimBPNodes")
    static void TriggerReactiveSteps(
        UPARAM(ref) FReactiveStepsState& State,
        FVector LeftIKGoal,
        FVector RightIKGoal,
        FVector PelvisIKGoal,
        FVector ImpactDirection,
        float StepDistance,
        float ArcHeight,
        int32 NumSteps
    );

    UFUNCTION(BlueprintCallable, Category = "AnimBPNodes")
    static void TickReactiveSteps(
        UPARAM(ref) FReactiveStepsState& State,
        float StepDuration,
        float DeltaTime,
        FVector& OutLeftIKGoal,
        FVector& OutRightIKGoal,
        bool& bAnyStepActive,
        bool& bStepJustFinished
    );
};