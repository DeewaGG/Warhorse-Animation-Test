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
struct FPushState
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, Category = "Push")
    FVector TargetPosition = FVector::ZeroVector;

    UPROPERTY(BlueprintReadWrite, Category = "Push")
    bool bActive = false;
};

USTRUCT(BlueprintType)
struct FProceduralFootState
{
    GENERATED_BODY()

    // Estado plantado
    UPROPERTY(BlueprintReadWrite, Category = "Foot")
    FVector PlantedActorWorldPos = FVector::ZeroVector;

    UPROPERTY(BlueprintReadWrite, Category = "Foot")
    FVector PlantedIKGoal = FVector::ZeroVector;

    UPROPERTY(BlueprintReadWrite, Category = "Foot")
    bool bIsPlanted = false;

    // Posición natural del pie (= IK goal inicial), usada como destino del paso
    UPROPERTY(BlueprintReadWrite, Category = "Foot")
    FVector RestIKGoal = FVector::ZeroVector;

    // Animación del paso
    UPROPERTY(BlueprintReadWrite, Category = "Foot")
    bool bIsStepping = false;

    UPROPERTY(BlueprintReadWrite, Category = "Foot")
    FVector StepStartWorldXY = FVector::ZeroVector;

    UPROPERTY(BlueprintReadWrite, Category = "Foot")
    float StepElapsed = 0.f;

    UPROPERTY(BlueprintReadWrite, Category = "Foot")
    float StepHeight = 8.f;
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
    static void InitProceduralFoot(
        UPARAM(ref) FProceduralFootState& State,
        FVector ActorWorldPosition,
        FVector CurrentIKGoal,
        float StepHeight
    );

    UFUNCTION(BlueprintCallable, Category = "AnimBPNodes")
    static void UpdateProceduralFoot(
        UPARAM(ref) FProceduralFootState& State,
        FVector ActorWorldPosition,
        FRotator ActorWorldRotation,
        float StepTriggerDistance,
        float StepDuration,
        float DeltaTime,
        bool bOtherFootStepping,
        FVector& OutIKGoal,
        bool& bOutIsStepping
    );
};