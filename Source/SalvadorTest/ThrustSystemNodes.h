#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Actor.h"
#include "Animation/AnimMontage.h"
#include "ThrustSystemNodes.generated.h"

USTRUCT(BlueprintType)
struct FThrustState
{
    GENERATED_BODY()

    // ── References ───────────────────────────────────────────────────────────
    UPROPERTY(BlueprintReadWrite, Category = "Thrust|Setup")
    USkeletalMeshComponent* AttackerMesh = nullptr;

    UPROPERTY(BlueprintReadWrite, Category = "Thrust|Setup")
    AActor* VictimActor = nullptr;

    UPROPERTY(BlueprintReadWrite, Category = "Thrust|Setup")
    TArray<FName> SkipPlantBones;

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
    FVector TargetBoneOffset = FVector::ZeroVector;

    UPROPERTY(BlueprintReadWrite, Category = "Thrust|IK")
    float StabDepth = 0.f;

    UPROPERTY(BlueprintReadWrite, Category = "Thrust|IK")
    float ArmReachPercent = 0.5f;

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
    bool bSkipPlant = false;

    UPROPERTY(BlueprintReadWrite, Category = "Thrust|Plant")
    FName ContactSocket;

    UPROPERTY(BlueprintReadWrite, Category = "Thrust|Plant")
    FVector SocketToHandOffsetCS = FVector::ZeroVector;

    UPROPERTY(BlueprintReadWrite, Category = "Thrust|Plant")
    FVector SocketRelativeLocation = FVector::ZeroVector;

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

    UPROPERTY(BlueprintReadWrite, Category = "Thrust|Plant")
    FVector SlaveInDomHandOffsetLocal = FVector::ZeroVector;

    UPROPERTY(BlueprintReadWrite, Category = "Thrust|Plant")
    FRotator SlaveRotInDomHandLocal = FRotator::ZeroRotator;

    UPROPERTY(BlueprintReadWrite, Category = "Thrust|Plant")
    FVector PlantedTargetBoneWorld = FVector::ZeroVector;

    UPROPERTY(BlueprintReadWrite, Category = "Thrust|Plant")
    FVector PlantedHandWorldPos = FVector::ZeroVector;

    // ── Hip follow ────────────────────────────────────────────────────────────
    UPROPERTY(BlueprintReadWrite, Category = "Thrust|Hip")
    FName HipLocGoal;

    UPROPERTY(BlueprintReadWrite, Category = "Thrust|Hip")
    float HipFollowPercent = 0.f;

    UPROPERTY(BlueprintReadWrite, Category = "Thrust|Hip")
    FVector HipRestPosCS = FVector::ZeroVector;

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

    UPROPERTY(BlueprintReadWrite, Category = "Thrust|Recover")
    FVector RecoverHipStartPosCS = FVector::ZeroVector;

    UPROPERTY(BlueprintReadWrite, Category = "Thrust|Recover")
    float RecoverArmElapsed = 0.f;

    UPROPERTY(BlueprintReadWrite, Category = "Thrust|Recover")
    float ArmRecoverDuration = 0.3f;

    UPROPERTY(BlueprintReadWrite, Category = "Thrust|Recover")
    float RecoverHipElapsed = 0.f;

    UPROPERTY(BlueprintReadWrite, Category = "Thrust|Recover")
    float HipRecoverDuration = 0.3f;

    // ── Constraints ──────────────────────────────────────────────────────────
    UPROPERTY(BlueprintReadWrite, Category = "Thrust|Constraints")
    FName LimitBone = TEXT("spine_03");

    UPROPERTY(BlueprintReadWrite, Category = "Thrust|Constraints")
    float MaxDistFromBone = 0.f;

    // ── Additive goals context ────────────────────────────────────────────────
    UPROPERTY(BlueprintReadWrite, Category = "Thrust|Additive")
    FName PelvisBoneName = TEXT("pelvis");

    // FRotator variable name on the AnimInstance holding the spine additive rotation
    UPROPERTY(BlueprintReadWrite, Category = "Thrust|Additive")
    FName SpineRotVarName;

    // float variable name on the AnimInstance holding the spine look-at alpha
    UPROPERTY(BlueprintReadWrite, Category = "Thrust|Additive")
    FName SpineAlphaVarName;

    // ── Debug ────────────────────────────────────────────────────────────────
    UPROPERTY(BlueprintReadWrite, Category = "Thrust|Debug")
    bool bDebug = false;
};

UCLASS()
class SALVADORTEST_API UThrustSystemNodes : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:

    UFUNCTION(BlueprintCallable, Category = "ThrustSystem")
    static void ThrustSetUp(
        UPARAM(ref) FThrustState& State,
        AActor* AttackerActor,
        AActor* VictimActor,
        FName DomLocGoal,
        FName DomRotGoal,
        FName SlaveLocGoal,
        FName SlaveRotGoal,
        const TArray<FName>& ContactSockets,
        const TArray<FName>& SkipPlantBones,
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
        float ArmReachPercent,
        FName HipLocGoal,
        float HipFollowPercent,
        float ArmRecoverDuration,
        float HipRecoverDuration,
        bool bDebug,
        FName PelvisBoneName = TEXT("pelvis"),
        FName SpineRotVarName = NAME_None,
        FName SpineAlphaVarName = NAME_None
    );

    UFUNCTION(BlueprintCallable, Category = "ThrustSystem")
    static void ThrustTick(
        UPARAM(ref) FThrustState& State,
        float DeltaTime,
        bool& bOutComplete
    );

    UFUNCTION(BlueprintCallable, Category = "ThrustSystem")
    static void ThrustPlant(
        UPARAM(ref) FThrustState& State,
        float DeltaTime,
        bool& bOutBlacklisted,
        bool& bOutComplete
    );

    UFUNCTION(BlueprintCallable, Category = "ThrustSystem")
    static void ThrustRecover(
        UPARAM(ref) FThrustState& State,
        float DeltaTime,
        bool& bOutComplete
    );

    UFUNCTION(BlueprintCallable, Category = "ThrustSystem")
    static void ThrustEnd(
        UPARAM(ref) FThrustState& State
    );
};
