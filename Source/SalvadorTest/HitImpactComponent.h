#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ThrustSystemNodes.h"
#include "HitImpactComponent.generated.h"

class ACharacter;
class UAnimInstanceBase;
class UAnimMontage;

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class SALVADORTEST_API UHitImpactComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UHitImpactComponent();

    // Entry point for a weapon contact event. Freezes the attack montage, captures montage
    // position/rate for reverse playback, and hands off to the ThrustSystem state machine.
    UFUNCTION(BlueprintCallable, Category = "HitImpact")
    void HitImpact(AActor* HitActor, FVector HitLocation, FName HitBone, UAnimMontage* Montage);

    // Call at attack start to blend out physics on nearby wounded victims so weapon traces
    // can register hits during the swing without rigid-body interference.
    UFUNCTION(BlueprintCallable, Category = "HitImpact")
    void DisableNearbyVictimsPhysics();

    UFUNCTION(BlueprintCallable, Category = "HitImpact")
    void PhysicRecovery(FName BoneName, FVector ImpulseDirection, float ImpulseMagnitude);

    bool IsThrusting() const { return bThrustActive; }

protected:
    virtual void BeginPlay() override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType,
                               FActorComponentTickFunction* ThisTickFunction) override;

public:
    // ── ABP goal variable names ───────────────────────────────────────────────

    /** ABP variable name (FVector) for the dominant hand IK position additive goal. Must match the variable name in the Animation Blueprint. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ThrustSystem|Goals")
    FName DomLocGoal;

    /** ABP variable name (FRotator) for the dominant hand IK rotation additive goal. Must match the variable name in the Animation Blueprint. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ThrustSystem|Goals")
    FName DomRotGoal;

    /** ABP variable name (FVector) for the slave hand IK position additive goal. Must match the variable name in the Animation Blueprint. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ThrustSystem|Goals")
    FName SlaveLocGoal;

    /** ABP variable name (FRotator) for the slave hand IK rotation additive goal. Must match the variable name in the Animation Blueprint. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ThrustSystem|Goals")
    FName SlaveRotGoal;

    /** ABP variable name (FVector) for the hip IK position additive goal. Leave empty to disable hip follow entirely. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ThrustSystem|Goals")
    FName HipLocGoal;

    /** Bone used as the origin for the MaxDistFromBone reach check. Typically spine_03 — the dominant hand exits plant early if it drifts further than MaxDistFromBone from this bone. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ThrustSystem|Constraints")
    FName LimitBone;

    /** Collision channel for the line trace that refines the surface impact point and applies StabDepth. Must match the channel your weapon/victim meshes block. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ThrustSystem|Setup")
    TEnumAsByte<ECollisionChannel> StabTraceChannel = ECC_Visibility;

    /** Socket names on the weapon mesh used as blade contact points. The socket closest to the hit location is selected each hit. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ThrustSystem|Setup")
    TArray<FName> ContactSockets;

    // ── Timing ────────────────────────────────────────────────────────────────

    /** Time in seconds for the approach phase (ThrustTick) to move the hand from rest to the target. Longer = slower, less jarring reach. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ThrustSystem|Timing",
              meta = (ClampMin = "0.0"))
    float HitReachDelay = 0.1f;

    /** How long in seconds the hand holds at the impact point before ThrustRecover begins. 0 = exit immediately after approach. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ThrustSystem|Timing",
              meta = (ClampMin = "0.0"))
    float PlantDuration = 0.2f;

    /** Plant duration used when the hit bone is in the victim's BlacklistedHitBones. Near-zero so the plant exits immediately but recovery still runs. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ThrustSystem|Timing",
              meta = (ClampMin = "0.0"))
    float BlacklistedPlantDuration = 0.01f;

    /** Fallback recover duration in seconds used when no montage is assigned. Ignored when a montage is set — the clip length drives recovery instead. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ThrustSystem|Timing",
              meta = (ClampMin = "0.0"))
    float RecoverDuration = 0.3f;

    /** Time in seconds for the arm IK goals to lerp back to rest during recovery. Clamped to the actual reverse clip length if shorter. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ThrustSystem|Timing",
              meta = (ClampMin = "0.0"))
    float ArmRecoverDuration = 0.3f;

    /** Time in seconds for the hip IK goal to lerp back to rest during recovery. Can be longer than ArmRecoverDuration to add trailing weight. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ThrustSystem|Timing",
              meta = (ClampMin = "0.0"))
    float HipRecoverDuration = 0.3f;

    /** Offset added to the captured montage position before reverse playback starts. Positive values start further into the clip (later in the attack animation). */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ThrustSystem|Timing")
    float ReverseStartOffset = 0.0f;

    /** Multiplies the negated forward montage rate to set the reverse playback speed. 1.0 = same speed as forward, 2.0 = twice as fast in reverse. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ThrustSystem|Timing",
              meta = (ClampMin = "0.01"))
    float ReverseRateMultiplier = 1.0f;

    /** If true, ignores the captured montage position and uses FixedReversePosition as the reverse start instead. Useful for consistent recovery framing. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ThrustSystem|Timing")
    bool bForceFixedReverseFrame = false;

    /** Montage position (seconds) used as the reverse start when bForceFixedReverseFrame is enabled. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ThrustSystem|Timing",
              meta = (EditCondition = "bForceFixedReverseFrame"))
    float FixedReversePosition = 0.0f;

    // ── Nearby physics disable ────────────────────────────────────────────────

    /** Radius (cm) around the attacker within which wounded victims have their physics blended out at attack start. Prevents rigid-body blocking of weapon traces mid-swing. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ThrustSystem|WoundedHit",
              meta = (ClampMin = "0.0"))
    float NearbyPhysicsDisableRadius = 300.f;

    /** Time in seconds to blend out nearby victims' physics when DisableNearbyVictimsPhysics is called. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ThrustSystem|WoundedHit",
              meta = (ClampMin = "0.0"))
    float NearbyPhysicsBlendOutDuration = 0.3f;

    // ── Reach ─────────────────────────────────────────────────────────────────

    /** Maximum distance (cm) from LimitBone the dominant hand is allowed to reach during plant. Exceeding this triggers an early exit and recovery. Lower = tighter arm constraint. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ThrustSystem|Reach",
              meta = (ClampMin = "0.0"))
    float MaxDistFromBone = 50.f;

    /** How far (cm) to push the impact point into the surface along the hit direction, making the blade visually penetrate the target. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ThrustSystem|Reach",
              meta = (ClampMin = "0.0"))
    float StabDepth = 10.f;

    /** Scale (0-1) applied to the slave hand position goal relative to the dominant hand. 0 = slave stays at rest, 1 = slave mirrors dom fully, 0.5 = halfway. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ThrustSystem|Reach",
              meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float ArmReachPercent = 0.5f;

    /** Scale (0-1) applied to the hip goal relative to the dominant hand additive offset. 0 = no hip movement, 1 = hip follows arm fully. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ThrustSystem|Reach",
              meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float HipFollowPercent = 0.3f;

    // ── Spine look-at ─────────────────────────────────────────────────────────

    /** Interpolation speed for blending the spine look-at alpha in when a target is locked. Higher = snappier spine turn toward the hit point. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "SpineLook",
              meta = (ClampMin = "0.1"))
    float SpineInterpInSpeed = 15.f;

    /** Interpolation speed for blending the spine look-at alpha out when the thrust ends. Lower = slower, more organic return to neutral. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "SpineLook",
              meta = (ClampMin = "0.1"))
    float SpineInterpOutSpeed = 5.f;

    // ── Additive context ──────────────────────────────────────────────────────

    /** Pelvis bone name used to capture the hip rest position at setup. Must match the skeleton. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ThrustSystem|Additive")
    FName PelvisBone = TEXT("pelvis");

    /** ABP variable name (FRotator) for the spine additive rotation driven by the look-at system during plant. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ThrustSystem|Additive")
    FName SpineRotVarName = TEXT("SpineLookAtAdditiveRot");

    /** ABP variable name (float) for the spine look-at alpha. Ramped 0 to 1 over PlantDuration and back to 0 during recovery. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ThrustSystem|Additive")
    FName SpineAlphaVarName = TEXT("SpineLookAtAlpha");

    /** Physical animation profile applied when PhysicRecovery() is called. Must exist in the Physics Asset on the attacker's skeletal mesh. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "HitImpact")
    FName PhysicRecoveryProfile = TEXT("HitReaction");

    /** Draws debug lines for the stab trace and target positions each frame. Disable in shipping. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ThrustSystem|Debug")
    bool bDebug = false;

private:
    UPROPERTY() TObjectPtr<ACharacter>        OwnerCharacter;
    UPROPERTY() TObjectPtr<UAnimInstanceBase> AnimInstance;
    UPROPERTY() TObjectPtr<AActor>            VictimActor;

    FThrustState State;
    bool         bThrustActive = false;
    bool         bThrust       = false;
    bool         bRecover      = false;

    float SpineCurrentAlpha   = 0.f;
    float SpineTargetAlpha    = 0.f;

    // Raw values captured at hit time before any multiplier is applied; needed to recompute
    // exit-specific rate/pos without re-querying the (now-frozen) montage.
    float CapturedMontageRate    = 1.f;
    float CapturedMontageRawPos  = 0.f;
};
