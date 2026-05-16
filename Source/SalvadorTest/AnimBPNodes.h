#pragma once
#include "CoreMinimal.h"
#include "Curves/CurveFloat.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Actor.h"
#include "Animation/AnimMontage.h"
#include "AnimBPNodes.generated.h"

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
// THRUST SYSTEM
// ─────────────────────────────────────────────

USTRUCT(BlueprintType)
struct FThrustState
{
    GENERATED_BODY()

    // ── References ───────────────────────────────────────────────────────────
    UPROPERTY(BlueprintReadWrite, Category = "Thrust|Setup")
    USkeletalMeshComponent* AttackerMesh = nullptr;

    UPROPERTY(BlueprintReadWrite, Category = "Thrust|Setup")
    AActor* VictimActor = nullptr;

    // ── ABP goal variable names ──────────────────────────────────────────────
    UPROPERTY(BlueprintReadWrite, Category = "Thrust|Goals")
    FName DomLocGoal;

    UPROPERTY(BlueprintReadWrite, Category = "Thrust|Goals")
    FName DomRotGoal;

    UPROPERTY(BlueprintReadWrite, Category = "Thrust|Goals")
    FName SlaveLocGoal;

    UPROPERTY(BlueprintReadWrite, Category = "Thrust|Goals")
    FName SlaveRotGoal;

    // ── Rest pose (A-pose defaults read from ABP at setup time) ─────────────
    UPROPERTY(BlueprintReadWrite, Category = "Thrust|Rest")
    FVector DomRestPos = FVector::ZeroVector;

    UPROPERTY(BlueprintReadWrite, Category = "Thrust|Rest")
    FRotator DomRestRot = FRotator::ZeroRotator;

    UPROPERTY(BlueprintReadWrite, Category = "Thrust|Rest")
    FVector SlaveRestPos = FVector::ZeroVector;

    UPROPERTY(BlueprintReadWrite, Category = "Thrust|Rest")
    FRotator SlaveRestRot = FRotator::ZeroRotator;

    // ── IK solution (absolute component space) ──────────────────────────────
    UPROPERTY(BlueprintReadWrite, Category = "Thrust|IK")
    FName PivotBone;

    UPROPERTY(BlueprintReadWrite, Category = "Thrust|IK")
    FName TargetBone;

    UPROPERTY(BlueprintReadWrite, Category = "Thrust|IK")
    FVector TargetBoneWorld = FVector::ZeroVector;

    UPROPERTY(BlueprintReadWrite, Category = "Thrust|IK")
    FVector TargetBoneOffset = FVector::ZeroVector;  // surface hit point minus bone world at setup

    UPROPERTY(BlueprintReadWrite, Category = "Thrust|IK")
    float StabDepth = 0.f;  // cm past the surface hit point along the trace direction

    UPROPERTY(BlueprintReadWrite, Category = "Thrust|IK")
    FRotator DomStartRotCS = FRotator::ZeroRotator;

    UPROPERTY(BlueprintReadWrite, Category = "Thrust|IK")
    FRotator DomTargetRotCS = FRotator::ZeroRotator;

    // ── Lerp state ───────────────────────────────────────────────────────────
    UPROPERTY(BlueprintReadWrite, Category = "Thrust|Lerp")
    float HitReachDelay = 0.1f;

    UPROPERTY(BlueprintReadWrite, Category = "Thrust|Lerp")
    int32 TotalFrames = 0;

    UPROPERTY(BlueprintReadWrite, Category = "Thrust|Lerp")
    int32 FramesRemaining = 0;

    UPROPERTY(BlueprintReadWrite, Category = "Thrust|Lerp")
    bool bActive = false;

    // ── Plant state ──────────────────────────────────────────────────────────
    UPROPERTY(BlueprintReadWrite, Category = "Thrust|Plant")
    bool bPlanted = false;

    UPROPERTY(BlueprintReadWrite, Category = "Thrust|Plant")
    FRotator PlantedRotCS = FRotator::ZeroRotator;

    UPROPERTY(BlueprintReadWrite, Category = "Thrust|Plant")
    float PlantDuration = 0.f;

    UPROPERTY(BlueprintReadWrite, Category = "Thrust|Plant")
    float PlantElapsed = 0.f;

    UPROPERTY(BlueprintReadWrite, Category = "Thrust|Plant")
    FVector PlantedDomHandWorld = FVector::ZeroVector;

    UPROPERTY(BlueprintReadWrite, Category = "Thrust|Plant")
    FRotator PlantedDomRotWorld = FRotator::ZeroRotator;

    UPROPERTY(BlueprintReadWrite, Category = "Thrust|Plant")
    FVector PlantedSlaveHandWorld = FVector::ZeroVector;

    UPROPERTY(BlueprintReadWrite, Category = "Thrust|Plant")
    FRotator PlantedSlaveRotWorld = FRotator::ZeroRotator;

    // Victim bone world position captured at plant start — tracking origin
    UPROPERTY(BlueprintReadWrite, Category = "Thrust|Plant")
    FVector PlantedTargetBoneWorld = FVector::ZeroVector;

    // ── Recover ──────────────────────────────────────────────────────────────
    UPROPERTY(BlueprintReadWrite, Category = "Thrust|Recover")
    UAnimMontage* Montage = nullptr;

    UPROPERTY(BlueprintReadWrite, Category = "Thrust|Recover")
    float MontagePos = 0.f;

    UPROPERTY(BlueprintReadWrite, Category = "Thrust|Recover")
    float RecoverDuration = 0.1f;

    UPROPERTY(BlueprintReadWrite, Category = "Thrust|Recover")
    bool bRecovering = false;

    UPROPERTY(BlueprintReadWrite, Category = "Thrust|Recover")
    bool bMontageReversing = false;

    UPROPERTY(BlueprintReadWrite, Category = "Thrust|Recover")
    int32 RecoverFramesTotal = 0;

    UPROPERTY(BlueprintReadWrite, Category = "Thrust|Recover")
    int32 RecoverFramesRemaining = 0;

    UPROPERTY(BlueprintReadWrite, Category = "Thrust|Recover")
    FVector RecoverDomStartPosCS = FVector::ZeroVector;

    UPROPERTY(BlueprintReadWrite, Category = "Thrust|Recover")
    FRotator RecoverDomStartRotCS = FRotator::ZeroRotator;

    UPROPERTY(BlueprintReadWrite, Category = "Thrust|Recover")
    FVector RecoverSlaveStartPosCS = FVector::ZeroVector;

    UPROPERTY(BlueprintReadWrite, Category = "Thrust|Recover")
    FRotator RecoverSlaveStartRotCS = FRotator::ZeroRotator;

    // ── Constraints ──────────────────────────────────────────────────────────
    UPROPERTY(BlueprintReadWrite, Category = "Thrust|Constraints")
    FName LimitBone = TEXT("spine_03");

    UPROPERTY(BlueprintReadWrite, Category = "Thrust|Constraints")
    float MaxDistFromBone = 0.f;  // 0 = unlimited

    // ── Debug ────────────────────────────────────────────────────────────────
    UPROPERTY(BlueprintReadWrite, Category = "Thrust|Debug")
    bool bDebug = false;
};

// ─────────────────────────────────────────────
// LIBRARY
// ─────────────────────────────────────────────

UCLASS()
class SALVADORTEST_API UAnimBPNodes : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:

    // ── Curve ────────────────────────────────────────────────────────────────
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

    // ── Thrust System ────────────────────────────────────────────────────────
    UFUNCTION(BlueprintCallable, Category = "AnimBPNodes|ThrustSystem")
    static void ThrustSetUp(
        UPARAM(ref) FThrustState& State,
        AActor* AttackerActor,
        AActor* VictimActor,
        FName DomLocGoal,
        FName DomRotGoal,
        FName SlaveLocGoal,
        FName SlaveRotGoal,
        const TArray<FName>& ContactSockets,
        FVector HitLocation,
        FName TargetBoneName,
        float HitReachDelay,
        float PlantDuration,
        UAnimMontage* Montage,
        float MontageCurrentPos,
        float RecoverDuration,
        FName LimitBone,
        float MaxDistFromBone,
        float StabDepth,
        bool bDebug
    );

    UFUNCTION(BlueprintCallable, Category = "AnimBPNodes|ThrustSystem")
    static void ThrustRecover(
        UPARAM(ref) FThrustState& State,
        float DeltaTime,
        bool& bOutComplete
    );

    UFUNCTION(BlueprintCallable, Category = "AnimBPNodes|ThrustSystem")
    static void ThrustTick(
        UPARAM(ref) FThrustState& State,
        float DeltaTime,
        bool& bOutComplete
    );

    UFUNCTION(BlueprintCallable, Category = "AnimBPNodes|ThrustSystem")
    static void ThrustPlant(
        UPARAM(ref) FThrustState& State,
        float DeltaTime,
        bool& bOutComplete
    );

    UFUNCTION(BlueprintCallable, Category = "AnimBPNodes|ThrustSystem")
    static void ThrustEnd(
        UPARAM(ref) FThrustState& State
    );

    // ── Foot IK ──────────────────────────────────────────────────────────────
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
