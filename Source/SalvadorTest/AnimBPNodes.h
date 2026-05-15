#pragma once
#include "CoreMinimal.h"
#include "Curves/CurveFloat.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Actor.h"
#include "AnimBPNodes.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogSwordIK, Log, All);

// ─────────────────────────────────────────────
// CURVE SAMPLER
// ─────────────────────────────────────────────

USTRUCT(BlueprintType)
struct FCurveSamplerState
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, Category = "CurveSampler")
    float Elapsed = 0.f;
};

// ─────────────────────────────────────────────
// FOOT IK
// ─────────────────────────────────────────────

USTRUCT(BlueprintType)
struct FFootIKState
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, Category = "FootIK|Setup")
    USkeletalMeshComponent* Mesh = nullptr;

    UPROPERTY(BlueprintReadWrite, Category = "FootIK|Setup")
    FName FootBone = NAME_None;

    UPROPERTY(BlueprintReadWrite, Category = "FootIK|Setup")
    FName HipBone = NAME_None;

    UPROPERTY(BlueprintReadWrite, Category = "FootIK|Setup")
    FVector NeutralGoal = FVector::ZeroVector;

    UPROPERTY(BlueprintReadWrite, Category = "FootIK|Setup")
    FVector2D StrideThreshold = FVector2D(40.f, 60.f);

    UPROPERTY(BlueprintReadWrite, Category = "FootIK|Setup")
    FVector2D StrideDuration = FVector2D(0.25f, 0.4f);

    UPROPERTY(BlueprintReadWrite, Category = "FootIK|Setup")
    FVector2D StrideHeight = FVector2D(8.f, 15.f);

    UPROPERTY(BlueprintReadWrite, Category = "FootIK|Setup")
    FVector2D StrideCooldown = FVector2D(0.1f, 0.2f);

    UPROPERTY(BlueprintReadWrite, Category = "FootIK|Setup")
    FVector2D StrideReach = FVector2D(0.f, 0.f);

    UPROPERTY(BlueprintReadWrite, Category = "FootIK|Runtime")
    FVector AnchorWorldPos = FVector::ZeroVector;

    UPROPERTY(BlueprintReadWrite, Category = "FootIK|Runtime")
    FVector AnchorGoal = FVector::ZeroVector;

    UPROPERTY(BlueprintReadWrite, Category = "FootIK|Runtime")
    bool bAnchored = false;

    UPROPERTY(BlueprintReadWrite, Category = "FootIK|Runtime")
    bool bStriding = false;

    UPROPERTY(BlueprintReadWrite, Category = "FootIK|Runtime")
    bool bForceStride = true;

    UPROPERTY(BlueprintReadWrite, Category = "FootIK|Runtime")
    FVector StrideStartGoal = FVector::ZeroVector;

    UPROPERTY(BlueprintReadWrite, Category = "FootIK|Runtime")
    float StrideElapsed = 0.f;

    UPROPERTY(BlueprintReadWrite, Category = "FootIK|Runtime")
    float CooldownTimer = 0.f;

    UPROPERTY(BlueprintReadWrite, Category = "FootIK|Runtime")
    float ActiveDuration = 0.3f;

    UPROPERTY(BlueprintReadWrite, Category = "FootIK|Runtime")
    float ActiveHeight = 10.f;

    UPROPERTY(BlueprintReadWrite, Category = "FootIK|Runtime")
    float ActiveReach = 0.f;

    UPROPERTY(BlueprintReadWrite, Category = "FootIK|Runtime")
    float ActiveCooldown = 0.15f;

    UPROPERTY(BlueprintReadWrite, Category = "FootIK|Runtime")
    float ActiveThreshold = 50.f;
};

// ─────────────────────────────────────────────
// HAND IK
// ─────────────────────────────────────────────

USTRUCT(BlueprintType)
struct FHandIKState
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, Category = "HandIK")
    USkeletalMeshComponent* Mesh = nullptr;

    UPROPERTY(BlueprintReadWrite, Category = "HandIK")
    AActor* AttackerActor = nullptr;

    UPROPERTY(BlueprintReadWrite, Category = "HandIK")
    FName HandBoneName;

    UPROPERTY(BlueprintReadWrite, Category = "HandIK")
    FName GoalPropertyName;

    UPROPERTY(BlueprintReadWrite, Category = "HandIK")
    FName RotationPropertyName;

    UPROPERTY(BlueprintReadWrite, Category = "HandIK")
    FVector StartHandGoalLocal = FVector::ZeroVector;

    UPROPERTY(BlueprintReadWrite, Category = "HandIK")
    FVector FinalHandGoalLocal = FVector::ZeroVector;

    UPROPERTY(BlueprintReadWrite, Category = "HandIK")
    FRotator StartHandRotLocal = FRotator::ZeroRotator;

    UPROPERTY(BlueprintReadWrite, Category = "HandIK")
    FRotator FinalHandRotLocal = FRotator::ZeroRotator;

    UPROPERTY(BlueprintReadWrite, Category = "HandIK")
    FVector LastPosDelta = FVector::ZeroVector;

    UPROPERTY(BlueprintReadWrite, Category = "HandIK")
    FRotator LastRotDelta = FRotator::ZeroRotator;

    UPROPERTY(BlueprintReadWrite, Category = "HandIK")
    float Duration = 0.1f;

    UPROPERTY(BlueprintReadWrite, Category = "HandIK")
    int32 TotalFrames = 0;

    UPROPERTY(BlueprintReadWrite, Category = "HandIK")
    int32 FramesRemaining = 0;

    UPROPERTY(BlueprintReadWrite, Category = "HandIK")
    bool bActive = false;

    UPROPERTY(BlueprintReadWrite, Category = "HandIK|Slave")
    bool bHasSlave = false;

    UPROPERTY(BlueprintReadWrite, Category = "HandIK|Slave")
    FName SlaveGoalPropertyName;

    UPROPERTY(BlueprintReadWrite, Category = "HandIK|Slave")
    FName SlaveRotationPropertyName;

    UPROPERTY(BlueprintReadWrite, Category = "HandIK|Position")
    AActor* VictimActor = nullptr;

    UPROPERTY(BlueprintReadWrite, Category = "HandIK|Position")
    FName VictimBoneName;

    // Posición world capturada al terminar TickHandIK — usada por TickHandIKFreeze
    UPROPERTY(BlueprintReadWrite, Category = "HandIK|Freeze")
    FVector FrozenHandWorldPos = FVector::ZeroVector;

    UPROPERTY(BlueprintReadWrite, Category = "HandIK|Freeze")
    bool bFreezeActive = false;
};

// ─────────────────────────────────────────────
// LIBRARY
// ─────────────────────────────────────────────

UCLASS()
class SALVADORTEST_API UAnimBPNodes : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:

    UFUNCTION(BlueprintCallable, Category = "AnimBPNodes|Curve")
    static void SampleCurve(
        UPARAM(ref) FCurveSamplerState& State,
        UCurveFloat* Curve,
        float Duration,
        float DeltaTime,
        bool bReset,
        float& OutValue,
        bool& bOutFinished
    );

    UFUNCTION(BlueprintCallable, Category = "AnimBPNodes|SwordContactIK")
    static void SolveHandIK(
        UPARAM(ref) FHandIKState& State,
        USkeletalMeshComponent* AttackerMesh,
        const TArray<FName>& ContactSockets,
        FVector HitLocation,
        AActor* VictimActor,
        FName TargetBoneName,
        FName DomIKLoc,
        FName DomIKRot,
        FName SlaveIKLoc,
        FName SlaveIKRot,
        float Duration
    );

    UFUNCTION(BlueprintCallable, Category = "AnimBPNodes|SwordContactIK")
    static void TickHandIK(
        UPARAM(ref) FHandIKState& State,
        float DeltaTime
    );

    UFUNCTION(BlueprintCallable, Category = "AnimBPNodes|FootIK")
    static void SetupFootIK(
        UPARAM(ref) FFootIKState& Foot,
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
        bool bForceFirstStride
    );

    UFUNCTION(BlueprintCallable, Category = "AnimBPNodes|FootIK")
    static void SolveFootIK(
        UPARAM(ref) FFootIKState& LeftFoot,
        UPARAM(ref) FFootIKState& RightFoot,
        FVector ActorWorldPos,
        FRotator ActorWorldRot,
        float DeltaTime,
        FVector& OutLeftGoal,
        FVector& OutRightGoal
    );

    UFUNCTION(BlueprintCallable, Category = "AnimBPNodes|FootIK")
    static bool AreFeetRepositioned(
        const FFootIKState& LeftFoot,
        const FFootIKState& RightFoot
    );

private:

    static void SolveFoot(
        FFootIKState& Foot,
        FVector ActorWorldPos,
        FRotator ActorWorldRot,
        FVector FootBoneWorld,
        FVector HipBoneWorld,
        float DeltaTime,
        bool bAnyFootBusy,
        FVector& OutGoal
    );
};
