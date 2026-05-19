#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Actor.h"
#include "Animation/AnimMontage.h"
#include "ThrustSystemNodes.generated.h"

// All IK goals written by this system are additive offsets (zero = animation pose).
// ABP variable names are stored as FNames and accessed via reflection so the system
// is decoupled from any concrete AnimInstance subclass.
USTRUCT(BlueprintType)
struct FThrustState
{
    GENERATED_BODY()

    // ── References ───────────────────────────────────────────────────────────

    /** Skeletal mesh of the attacking character. Set automatically by ThrustSetUp — do not assign manually. */
    UPROPERTY(BlueprintReadWrite, Category = "Thrust|Setup")
    USkeletalMeshComponent* AttackerMesh = nullptr;

    /** The actor that was hit. Used to track the target bone world position live each frame. */
    UPROPERTY(BlueprintReadWrite, Category = "Thrust|Setup")
    AActor* VictimActor = nullptr;

    // ── ABP goal variable names ───────────────────────────────────────────────

    /** ABP variable name (FVector) for the dominant hand position additive goal. Resolved via reflection each frame. */
    UPROPERTY(BlueprintReadWrite, Category = "Thrust|Goals")
    FName DomLocGoal;

    /** ABP variable name (FRotator) for the dominant hand rotation additive goal. Resolved via reflection each frame. */
    UPROPERTY(BlueprintReadWrite, Category = "Thrust|Goals")
    FName DomRotGoal;

    /** ABP variable name (FVector) for the slave hand position additive goal. Resolved via reflection each frame. */
    UPROPERTY(BlueprintReadWrite, Category = "Thrust|Goals")
    FName SlaveLocGoal;

    /** ABP variable name (FRotator) for the slave hand rotation additive goal. Resolved via reflection each frame. */
    UPROPERTY(BlueprintReadWrite, Category = "Thrust|Goals")
    FName SlaveRotGoal;

    // ── Rest pose ─────────────────────────────────────────────────────────────

    /** Dominant hand position in component space at setup time, captured from the pivot bone. Restored by ThrustEnd. */
    UPROPERTY(BlueprintReadWrite, Category = "Thrust|Rest")
    FVector DomRestPos = FVector::ZeroVector;

    /** Dominant hand rotation in component space at setup time. Restored by ThrustEnd. */
    UPROPERTY(BlueprintReadWrite, Category = "Thrust|Rest")
    FRotator DomRestRot = FRotator::ZeroRotator;

    /** Slave hand rest position (mirrors DomRestPos at setup). Restored by ThrustEnd. */
    UPROPERTY(BlueprintReadWrite, Category = "Thrust|Rest")
    FVector SlaveRestPos = FVector::ZeroVector;

    /** Slave hand rest rotation (mirrors DomRestRot at setup). Restored by ThrustEnd. */
    UPROPERTY(BlueprintReadWrite, Category = "Thrust|Rest")
    FRotator SlaveRestRot = FRotator::ZeroRotator;

    // ── IK solution ──────────────────────────────────────────────────────────

    /** Bone that the contact socket is attached to; used as the IK pivot for rotation computation. */
    UPROPERTY(BlueprintReadWrite, Category = "Thrust|IK")
    FName PivotBone;

    /** Bone on the victim mesh being targeted. Queried live each frame to track a moving victim. */
    UPROPERTY(BlueprintReadWrite, Category = "Thrust|IK")
    FName TargetBone;

    /** Fallback world position of the target, used when no victim mesh is available. */
    UPROPERTY(BlueprintReadWrite, Category = "Thrust|IK")
    FVector TargetBoneWorld = FVector::ZeroVector;

    /** Offset from the raw bone position to the trace-refined surface impact point + StabDepth. Added to the live bone position every frame. */
    UPROPERTY(BlueprintReadWrite, Category = "Thrust|IK")
    FVector TargetBoneOffset = FVector::ZeroVector;

    /** How far (cm) the impact point was pushed into the surface along the hit direction. Set by ThrustSetUp from HitImpactComponent.StabDepth. */
    UPROPERTY(BlueprintReadWrite, Category = "Thrust|IK")
    float StabDepth = 0.f;

    /** Scale (0-1) applied to the slave hand position relative to the dominant hand. 0 = slave stays at rest, 1 = slave mirrors dom. Currently stored but not yet applied — set via HitImpactComponent.ArmReachPercent. */
    UPROPERTY(BlueprintReadWrite, Category = "Thrust|IK")
    float ArmReachPercent = 0.5f;

    /** Dominant hand rotation in component space at setup (the animation rest rotation). */
    UPROPERTY(BlueprintReadWrite, Category = "Thrust|IK")
    FRotator DomStartRotCS = FRotator::ZeroRotator;

    /** Target dominant hand rotation in component space, computed at setup from the direction toward the hit point. */
    UPROPERTY(BlueprintReadWrite, Category = "Thrust|IK")
    FRotator DomTargetRotCS = FRotator::ZeroRotator;

    // ── Approach phase (ThrustTick) ───────────────────────────────────────────

    /** Duration of the approach phase in seconds. Set from HitImpactComponent.HitReachDelay. */
    UPROPERTY(BlueprintReadWrite, Category = "Thrust|Lerp")
    float HitReachDelay = 0.1f;

    /** Total frame count for the approach phase, computed from HitReachDelay / DeltaTime on the first tick. */
    UPROPERTY(BlueprintReadWrite, Category = "Thrust|Lerp")
    int32 TotalFrames = 0;

    /** Frames remaining in the current phase. Counts down each tick. */
    UPROPERTY(BlueprintReadWrite, Category = "Thrust|Lerp")
    int32 FramesRemaining = 0;

    /** True while ThrustTick is running the approach phase. Set to false when the approach completes. */
    UPROPERTY(BlueprintReadWrite, Category = "Thrust|Lerp")
    bool bActive = false;

    // ── Plant phase (ThrustPlant) ─────────────────────────────────────────────

    /** True once the approach phase completes and ThrustPlant takes over. */
    UPROPERTY(BlueprintReadWrite, Category = "Thrust|Plant")
    bool bPlanted = false;

    /** The weapon socket chosen at setup (closest to the hit location). Read each frame for socket-feedback IK. */
    UPROPERTY(BlueprintReadWrite, Category = "Thrust|Plant")
    FName ContactSocket;

    /** Unused. Hand-to-socket offset in component space. */
    UPROPERTY(BlueprintReadWrite, Category = "Thrust|Plant")
    FVector SocketToHandOffsetCS = FVector::ZeroVector;

    /** Hand-to-socket offset in hand-local space. Constant after setup; used to reconstruct the world hand position from the anchored rotation without re-querying the skeleton. */
    UPROPERTY(BlueprintReadWrite, Category = "Thrust|Plant")
    FVector SocketRelativeLocation = FVector::ZeroVector;

    /** Dominant hand rotation in component space at the moment the plant begins. Used to anchor world rotation on the first plant frame. */
    UPROPERTY(BlueprintReadWrite, Category = "Thrust|Plant")
    FRotator PlantedRotCS = FRotator::ZeroRotator;

    /** How long in seconds to hold the plant before transitioning to recovery. Set from HitImpactComponent.PlantDuration. */
    UPROPERTY(BlueprintReadWrite, Category = "Thrust|Plant")
    float PlantDuration = 0.f;

    /** Time elapsed since the plant phase started. Compared against PlantDuration each frame. */
    UPROPERTY(BlueprintReadWrite, Category = "Thrust|Plant")
    float PlantElapsed = 0.f;

    /** Unused legacy field. */
    UPROPERTY(BlueprintReadWrite, Category = "Thrust|Plant")
    FVector PlantedDomHandWorld = FVector::ZeroVector;

    /** World-space rotation of the dominant hand, captured on the first plant frame. Re-projected to component space each frame so the hand stays world-anchored despite character rotation. */
    UPROPERTY(BlueprintReadWrite, Category = "Thrust|Plant")
    FRotator PlantedDomRotWorld = FRotator::ZeroRotator;

    /** Unused legacy field. */
    UPROPERTY(BlueprintReadWrite, Category = "Thrust|Plant")
    FVector PlantedSlaveHandWorld = FVector::ZeroVector;

    /** Unused legacy field. */
    UPROPERTY(BlueprintReadWrite, Category = "Thrust|Plant")
    FRotator PlantedSlaveRotWorld = FRotator::ZeroRotator;

    /** Unused legacy field. */
    UPROPERTY(BlueprintReadWrite, Category = "Thrust|Plant")
    FVector SlaveInDomHandOffsetLocal = FVector::ZeroVector;

    /** Unused legacy field. */
    UPROPERTY(BlueprintReadWrite, Category = "Thrust|Plant")
    FRotator SlaveRotInDomHandLocal = FRotator::ZeroRotator;

    /** World position of the target bone captured on the first plant frame. Used as a fixed reference if victim tracking is not needed. */
    UPROPERTY(BlueprintReadWrite, Category = "Thrust|Plant")
    FVector PlantedTargetBoneWorld = FVector::ZeroVector;

    /** World-space hand position derived from PlantedDomRotWorld and SocketRelativeLocation on the first plant frame. Re-projected to component space every frame. */
    UPROPERTY(BlueprintReadWrite, Category = "Thrust|Plant")
    FVector PlantedHandWorldPos = FVector::ZeroVector;

    // ── Hip follow ────────────────────────────────────────────────────────────

    /** ABP variable name (FVector) for the hip IK position additive goal. Empty = hip follow disabled. */
    UPROPERTY(BlueprintReadWrite, Category = "Thrust|Hip")
    FName HipLocGoal;

    /** Scale (0-1) applied to the hip goal relative to the dominant hand additive offset. 0 = no hip movement. */
    UPROPERTY(BlueprintReadWrite, Category = "Thrust|Hip")
    float HipFollowPercent = 0.f;

    /** Pelvis bone position in component space at setup. Used as the hip rest reference for recovery. */
    UPROPERTY(BlueprintReadWrite, Category = "Thrust|Hip")
    FVector HipRestPosCS = FVector::ZeroVector;

    // ── Recovery phase (ThrustRecover) ───────────────────────────────────────

    /** Attack montage to play in reverse during recovery. Set from the montage passed to HitImpact(). */
    UPROPERTY(BlueprintReadWrite, Category = "Thrust|Recover")
    UAnimMontage* Montage = nullptr;

    /** Montage position (seconds) captured at setup; clamped >= 0. Used as the reverse playback start point. */
    UPROPERTY(BlueprintReadWrite, Category = "Thrust|Recover")
    float MontagePos = 0.f;

    /** Forward montage play rate captured at setup. Negated and scaled by ReverseRateMultiplier for reverse playback. */
    UPROPERTY(BlueprintReadWrite, Category = "Thrust|Recover")
    float MontageRate = 1.f;

    /** Fallback recover duration in seconds, used when no montage is assigned. */
    UPROPERTY(BlueprintReadWrite, Category = "Thrust|Recover")
    float RecoverDuration = 0.1f;

    /** True once ThrustRecover has been entered. Prevents re-initialising the recover state each frame. */
    UPROPERTY(BlueprintReadWrite, Category = "Thrust|Recover")
    bool bRecovering = false;

    /** True when recovery is driving the montage in reverse. False when using the frame-count fallback. */
    UPROPERTY(BlueprintReadWrite, Category = "Thrust|Recover")
    bool bMontageReversing = false;

    /** Total frame count for the fallback (non-montage) recovery path. */
    UPROPERTY(BlueprintReadWrite, Category = "Thrust|Recover")
    int32 RecoverFramesTotal = 0;

    /** Frames remaining in the fallback recovery. Counts down to zero. */
    UPROPERTY(BlueprintReadWrite, Category = "Thrust|Recover")
    int32 RecoverFramesRemaining = 0;

    /** Dominant hand position at the moment recovery starts. Lerped to zero over ArmRecoverDuration. */
    UPROPERTY(BlueprintReadWrite, Category = "Thrust|Recover")
    FVector RecoverDomStartPosCS = FVector::ZeroVector;

    /** Dominant hand rotation at the moment recovery starts. Slerped to identity over ArmRecoverDuration. */
    UPROPERTY(BlueprintReadWrite, Category = "Thrust|Recover")
    FRotator RecoverDomStartRotCS = FRotator::ZeroRotator;

    /** Slave hand position at the moment recovery starts. Lerped to zero over ArmRecoverDuration. */
    UPROPERTY(BlueprintReadWrite, Category = "Thrust|Recover")
    FVector RecoverSlaveStartPosCS = FVector::ZeroVector;

    /** Slave hand rotation at the moment recovery starts. Slerped to identity over ArmRecoverDuration. */
    UPROPERTY(BlueprintReadWrite, Category = "Thrust|Recover")
    FRotator RecoverSlaveStartRotCS = FRotator::ZeroRotator;

    /** Hip position at the moment recovery starts. Lerped to zero over HipRecoverDuration. */
    UPROPERTY(BlueprintReadWrite, Category = "Thrust|Recover")
    FVector RecoverHipStartPosCS = FVector::ZeroVector;

    /** Time elapsed in the arm IK recovery lerp. */
    UPROPERTY(BlueprintReadWrite, Category = "Thrust|Recover")
    float RecoverArmElapsed = 0.f;

    /** Time in seconds for the arm IK goals to lerp back to rest. Clamped to the actual reverse clip length if shorter. */
    UPROPERTY(BlueprintReadWrite, Category = "Thrust|Recover")
    float ArmRecoverDuration = 0.3f;

    /** Time elapsed in the hip IK recovery lerp. */
    UPROPERTY(BlueprintReadWrite, Category = "Thrust|Recover")
    float RecoverHipElapsed = 0.f;

    /** Time in seconds for the hip IK goal to lerp back to rest. Independent of ArmRecoverDuration. */
    UPROPERTY(BlueprintReadWrite, Category = "Thrust|Recover")
    float HipRecoverDuration = 0.3f;

    // ── Constraints ──────────────────────────────────────────────────────────

    /** Bone used as the origin for the reach distance check. Dominant hand exits plant early if it exceeds MaxDistFromBone from this bone. */
    UPROPERTY(BlueprintReadWrite, Category = "Thrust|Constraints")
    FName LimitBone = TEXT("spine_03");

    /** Maximum distance (cm) from LimitBone before ThrustPlant triggers an early exit to recovery. 0 = no limit. */
    UPROPERTY(BlueprintReadWrite, Category = "Thrust|Constraints")
    float MaxDistFromBone = 0.f;

    // ── Additive context ──────────────────────────────────────────────────────

    /** Pelvis bone name used to capture the hip rest position at setup. */
    UPROPERTY(BlueprintReadWrite, Category = "Thrust|Additive")
    FName PelvisBoneName = TEXT("pelvis");

    /** ABP variable name (FRotator) for the spine additive rotation driven by the look-at system. */
    UPROPERTY(BlueprintReadWrite, Category = "Thrust|Additive")
    FName SpineRotVarName;

    /** ABP variable name (float) for the spine look-at alpha. Ramped 0 to 1 over PlantDuration, back to 0 during recovery. */
    UPROPERTY(BlueprintReadWrite, Category = "Thrust|Additive")
    FName SpineAlphaVarName;

    /** Collision channel for the stab surface trace. Should match the channel used in HitImpactComponent. */
    UPROPERTY(BlueprintReadWrite, Category = "Thrust|Trace")
    TEnumAsByte<ECollisionChannel> StabTraceChannel = ECC_Visibility;

    /** Draws debug lines for the stab trace and target positions. Disable in shipping. */
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
        FVector HitLocation,
        FName TargetBoneName,
        float HitReachDelay,
        float PlantDuration,
        UAnimMontage* Montage,
        float MontageCurrentPos,
        float MontagePlayRate,
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
        FName SpineAlphaVarName = NAME_None,
        TEnumAsByte<ECollisionChannel> StabTraceChannel = ECC_Visibility
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
