#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "FootIKNodes.h"
#include "CurveSamplerNodes.h"
#include "PhysicsEngine/PhysicalAnimationComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Curves/CurveFloat.h"
#include "HitReactionComponent.generated.h"

class UAnimInstanceBase;

// Drives physical hit reactions, foot repositioning, low-health simulation, and ragdoll death.
//
// State machine (tick disabled between states):
//   Idle
//     → HitW_Physics()     — stunt running: curve drives SimValue, SimulationWeight/PushVictim/PelvisMovement each frame
//          → bFinished      — SimFinish() stops stunt physics; ReactiveSteps repositions feet
//          → AreFeetRepositioned() — if HP==1: LowHealthTick activates (persistent mid-bone oscillation)
//     → ActivateRagdoll()  — full ragdoll blend-in over RagdollTransitionTime
//          → T >= 1         — if RagdollLifetime > 0: freeze bodies in place, kill all ticks and collisions
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class SALVADORTEST_API UHitReactionComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UHitReactionComponent();

    virtual void BeginPlay() override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    // Entry point called from BP on hit detection. Decrements HP (unless bone is blacklisted),
    // selects the physics bones for the attack side, and starts the stunt.
    UFUNCTION(BlueprintCallable, Category = "HitReaction")
    void HitW_Physics(int32 InAttackSide, FName InBoneHit, FVector InHitDir, double InHitStrength);

    // Transitions from stunt to full ragdoll. Call from BP when HP <= 0 (typically at ThrustRecover).
    // Applies the "Ragdoll" Physical Animation + Constraint profiles — both must exist in the Physics Asset.
    UFUNCTION(BlueprintCallable, Category = "HitReaction")
    void ActivateRagdoll();

    // Sets physics blend weight on all active PhysicsBones directly.
    // Called from BP during a death-plant montage to drive the blend from the animation side.
    UFUNCTION(BlueprintCallable, Category = "HitReaction")
    void SetDeathPlantBlend(float Blend);

    // Instantly disables MidSimBone simulation. Use BlendOutPhysics() for a gradual transition.
    UFUNCTION(BlueprintCallable, Category = "HitReaction")
    void StopLowHealthSim();

    // Ramps the low-health physics blend to zero over Duration seconds. No-op if low-health is not active.
    void BlendOutPhysics(float Duration);

    // Queried by BP to gate death triggers — ensures ActivateRagdoll doesn't fire before low-health starts.
    bool IsLowHealthActive() const { return bLowHealthActive; }

    // ── Ragdoll transition ────────────────────────────────────────────────────

    /** How long in seconds the physics blend weight takes to ramp from 0 to 1 after ActivateRagdoll is called. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HitReaction|Ragdoll",
              meta = (ClampMin = "0.0"))
    float RagdollTransitionTime = 0.3f;

    /** Optional curve remapping the blend-in progress (X = normalised time 0-1, Y = blend weight 0-1). Linear if unset. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HitReaction|Ragdoll")
    TObjectPtr<UCurveFloat> RagdollTransitionCurve;

    /** Velocity-change impulse (cm/s) applied to the pelvis in the opposite direction of the hit when ragdoll activates, sending the character flying back. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HitReaction|Ragdoll",
              meta = (ClampMin = "0.0"))
    float RagdollImpulse = 500.f;

    /** Seconds after the blend-in completes before the ragdoll is frozen in place. 0 = never auto-freeze. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HitReaction|Ragdoll",
              meta = (ClampMin = "0.0"))
    float RagdollLifetime = 0.f;

    // ── Health ────────────────────────────────────────────────────────────────

    /** Maximum hit points. CurrentHP is initialised to this value. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HitReaction|Health")
    int32 MaxHP = 3;

    /** Current hit points. Decremented by each hit unless the bone is blacklisted. Read-only at runtime. */
    UPROPERTY(BlueprintReadOnly, Category = "HitReaction|Health")
    int32 CurrentHP = 3;

    /** Hits to bones in this list skip HP decrement but still run the full stunt and ThrustRecover. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HitReaction|Health")
    TArray<FName> BlacklistedHitBones;

    // ── Low health simulation ─────────────────────────────────────────────────

    /** Target physics blend weight for MidSimBone during low-health simulation. Lower = subtle fatigue look; higher = near-ragdoll. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HitReaction|LowHealth",
              meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float LowHealthSimWeight = 0.3f;

    /** Time in seconds to blend from 0 to LowHealthSimWeight when low-health activates. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HitReaction|LowHealth",
              meta = (ClampMin = "0.0"))
    float LowHealthTransitionTime = 1.5f;

    /** Safety timeout in seconds: stops low-health simulation if the character is never killed. 0 = no timeout. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HitReaction|LowHealth",
              meta = (ClampMin = "0.0"))
    float LowHealthTickTimeout = 5.f;

    /** Frequency (Hz) of the primary oscillation wave. Low values (~0.3) produce a slow, laboured breathing feel. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HitReaction|LowHealth",
              meta = (ClampMin = "0.0"))
    float LowHealthOscFrequency = 0.35f;

    /** Amplitude of the primary wave in blend weight units, added on top of LowHealthSimWeight each frame. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HitReaction|LowHealth",
              meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float LowHealthOscAmplitude = 0.2f;

    /** Frequency (Hz) of the secondary oscillation wave. Higher values (~0.8) add an involuntary muscle-tremor layer. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HitReaction|LowHealth",
              meta = (ClampMin = "0.0"))
    float LowHealthOscFrequency2 = 0.8f;

    /** Amplitude of the secondary (tremor) wave in blend weight units. Keep lower than the primary amplitude. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HitReaction|LowHealth",
              meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float LowHealthOscAmplitude2 = 0.08f;

    // ── Stunt timing ─────────────────────────────────────────────────────────

    /** Base duration in seconds of the physics stunt. Top and bot hits multiply this by their respective time multipliers. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HitReaction|Stunt")
    double StuntTime = 1.0;

    /** Multiplier on StuntTime for top (upper-body, AttackSide 0) hits. Values above 1.0 extend the stunt window. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HitReaction|Stunt",
              meta = (ClampMin = "1.0"))
    float StuntTimeMultiplierTop = 1.1f;

    /** Multiplier on StuntTime for bot (lower-body, AttackSide 2) hits. Values above 1.0 extend the stunt window. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HitReaction|Stunt",
              meta = (ClampMin = "1.0"))
    float StuntTimeMultiplierBot = 1.4f;

    /** Maximum physics blend weight for bot hits (AttackSide 2). Capped below 1.0 to avoid a full ragdoll look on low-body strikes. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HitReaction|Stunt",
              meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float BotSideMaxBlendWeight = 0.5f;

    /** Curve driving the physics blend weight (SimValue) over the stunt duration for top hits. X = normalised time, Y = weight 0-1. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HitReaction|Stunt")
    TObjectPtr<UCurveFloat> StuntCurveTop;

    /** Curve driving SimValue for mid hits (AttackSide 1). X = normalised time, Y = weight 0-1. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HitReaction|Stunt")
    TObjectPtr<UCurveFloat> StuntCurveMid;

    /** Curve driving SimValue for bot hits (AttackSide 2). X = normalised time, Y = weight 0-1. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HitReaction|Stunt")
    TObjectPtr<UCurveFloat> StuntCurveBot;

    // ── Pelvis disruption ─────────────────────────────────────────────────────

    /** XY displacement magnitude (cm) applied to PelvisGoalPosition during bot hits. 0 disables horizontal pelvis disruption. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HitReaction|Pelvis")
    double PelvisDisruption = 0.0;

    /** Scales HitStrength into a pelvis Z dip on impact. Negative values make the pelvis drop (e.g. -0.07 × strength 100 = -7 cm). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HitReaction|Pelvis")
    float PelvisZHitStrengthScale = -0.07f;

    /** Min/max clamp (cm) applied to the scaled pelvis Z offset. X = floor (negative = dip), Y = ceiling. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HitReaction|Pelvis")
    FVector2D PelvisZClampRange = FVector2D(-70.f, 20.f);

    // ── Right foot stride params ──────────────────────────────────────────────

    /** Min/max distance (cm) the pelvis must move from the foot anchor before a right-foot step fires. Randomised per step. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HitReaction|FootIK|Right")
    FVector2D RStrideThreshold = FVector2D(10.f, 15.f);

    /** Min/max duration (s) of each right-foot step arc. Randomised per step. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HitReaction|FootIK|Right")
    FVector2D RStrideDuration = FVector2D(0.15f, 0.25f);

    /** Min/max arc height (cm) for each right-foot step. Randomised per step. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HitReaction|FootIK|Right")
    FVector2D RStrideHeight = FVector2D(8.f, 15.f);

    /** Min/max cooldown (s) between consecutive right-foot steps. Prevents machine-gun stepping. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HitReaction|FootIK|Right")
    FVector2D RStrideCooldown = FVector2D(0.05f, 0.1f);

    /** Min/max overshoot (cm) past the pelvis target when planting the right foot. Adds a natural weight-transfer look. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HitReaction|FootIK|Right")
    FVector2D RStrideReach = FVector2D(0.f, 3.f);

    // ── Left foot stride params ───────────────────────────────────────────────

    /** Min/max distance (cm) the pelvis must move from the foot anchor before a left-foot step fires. Randomised per step. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HitReaction|FootIK|Left")
    FVector2D LStrideThreshold = FVector2D(10.f, 15.f);

    /** Min/max duration (s) of each left-foot step arc. Randomised per step. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HitReaction|FootIK|Left")
    FVector2D LStrideDuration = FVector2D(0.15f, 0.25f);

    /** Min/max arc height (cm) for each left-foot step. Randomised per step. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HitReaction|FootIK|Left")
    FVector2D LStrideHeight = FVector2D(8.f, 15.f);

    /** Min/max cooldown (s) between consecutive left-foot steps. Prevents machine-gun stepping. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HitReaction|FootIK|Left")
    FVector2D LStrideCooldown = FVector2D(0.05f, 0.1f);

    /** Min/max overshoot (cm) past the pelvis target when planting the left foot. Adds a natural weight-transfer look. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HitReaction|FootIK|Left")
    FVector2D LStrideReach = FVector2D(0.f, 3.f);

    // ── Foot IK shared params ─────────────────────────────────────────────────

    /** Half-length of the foot (cm). Used to compute how far the foot tip has drifted from its anchor before a step fires. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HitReaction|FootIK",
              meta = (ClampMin = "0.0"))
    float FootSize = 31.f;

    /** Scales foot forward-tilt into the stride threshold. Higher values allow more tilt before the foot takes a corrective step. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HitReaction|FootIK",
              meta = (ClampMin = "0.0"))
    float FootPitchScale = 100.f;

    // ── Push force ───────────────────────────────────────────────────────────

    /** Scale factor on the lateral capsule push applied each frame during the stunt. Bot hits (AttackSide 2) always push with 0 force regardless of this value. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HitReaction|Push")
    double VictimPushForce = 1.0;

    /** Distance (cm) ahead of the impact point that defines the push target. Capsule movement stops once it reaches this position. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HitReaction|Push",
              meta = (ClampMin = "0.0"))
    double PushGoalDistance = 200.0;

    // ── Physics impulse ───────────────────────────────────────────────────────

    /** Divisor for the secondary impulse applied to each sim bone. Primary = HitStrength, Secondary = HitStrength / this. Higher = softer secondary jolt. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HitReaction|Physics",
              meta = (ClampMin = "0.01"))
    float SecondaryImpulseDivisor = 5.f;

    // ── Bone names ────────────────────────────────────────────────────────────

    /** Pelvis bone name. Used as the ragdoll simulation root and as the reference for foot IK pelvis tracking. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HitReaction|Bones")
    FName PelvisBoneName = TEXT("pelvis");

    /** Bone used when the actual hit bone is the pelvis or None. Prevents physics motors from anchoring at an unstable skeleton root. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HitReaction|Bones")
    FName FallbackHitBone = TEXT("spine_01");

    /** Root of the simulation chain for top hits (AttackSide 0). All bones below this receive physics. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HitReaction|Bones")
    FName UpperSimBone = TEXT("spine_03");

    /** Root of the simulation chain for mid/bot hits and the persistent low-health sim. All bones below this receive physics. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HitReaction|Bones")
    FName MidSimBone = TEXT("spine_01");

    /** Left thigh bone — available as an extension point for lower-body sim if needed. Not used in stunt logic. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HitReaction|Bones")
    FName LowerSimBoneL = TEXT("thigh_l");

    /** Right thigh bone — available as an extension point for lower-body sim if needed. Not used in stunt logic. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HitReaction|Bones")
    FName LowerSimBoneR = TEXT("thigh_r");

    /** Virtual bone used to sample pelvis world position for foot IK drift tracking. Must match the IK Rig virtual bone name. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HitReaction|Bones")
    FName VirtualPelvisBone = TEXT("VB pelvis");

    /** Virtual bone for the left foot IK goal. Must match the IK Rig virtual bone name. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HitReaction|Bones")
    FName VirtualLeftFootBone = TEXT("VB foot_l");

    /** Virtual bone for the right foot IK goal. Must match the IK Rig virtual bone name. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HitReaction|Bones")
    FName VirtualRightFootBone = TEXT("VB foot_r");

    /** Bone below which all simulation is disabled by SimFinish. Should be the skeleton root so every bone is covered. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HitReaction|Bones")
    FName RootSimBone = TEXT("root");

    // ── Physical animation ────────────────────────────────────────────────────

    /** Name of the Physical Animation profile applied to stunt sim bones. Must exist in the Physics Asset on this skeletal mesh. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HitReaction|Physics")
    FName PhysicalAnimProfile = TEXT("HitReaction");

    // ── Impulse direction/drop scalars ────────────────────────────────────────

    /** Multiplier on the lateral hit direction for right-side hits. Scale down to reduce how far the body flies sideways. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HitReaction|Impulse")
    float RHitDirScale = 1.f;

    /** Multiplier on the downward component of the impulse for right-side hits. Scale down to reduce the vertical knockdown force. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HitReaction|Impulse")
    float RHitDropScale = 1.f;

    /** Multiplier on the lateral hit direction for left-side hits. Scale down to reduce how far the body flies sideways. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HitReaction|Impulse")
    float LHitDirScale = 1.f;

    /** Multiplier on the downward component of the impulse for left-side hits. Scale down to reduce the vertical knockdown force. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HitReaction|Impulse")
    float LHitDropScale = 1.f;

private:
    // ── References ────────────────────────────────────────────────────────────
    UPROPERTY() TObjectPtr<UPhysicalAnimationComponent> PhysicAnimComp;
    UPROPERTY() TObjectPtr<USkeletalMeshComponent>      Mesh;
    UPROPERTY() TObjectPtr<UCharacterMovementComponent> CharacterMovement;
    UPROPERTY() TObjectPtr<AActor>                      BPVictim;
    UPROPERTY() TObjectPtr<UAnimInstanceBase>           ABP;

    // ── Hit state ─────────────────────────────────────────────────────────────
    int32         AttackSide     = 0;
    FName         HitBone;
    TArray<FName> PhysicsBones;
    FVector       HitDir         = FVector::ZeroVector;
    double        HitStrength    = 0.0;

    // ── Sim state ─────────────────────────────────────────────────────────────
    double              SimValue            = 0.0;
    FCurveSamplerState  StuntValueState;
    bool                bResetHit           = false;
    bool                bSimFinishTriggered = false;
    bool                bRepositioning      = false;
    bool                bDoOnceFired        = false;

    // ── Low health sim state ──────────────────────────────────────────────────
    bool  bLowHealthActive = false;
    float LowHealthElapsed = 0.f;
    float LowHealthBlend   = 0.f;

    // ── Physics blend-out state ───────────────────────────────────────────────
    bool  bBlendingOutPhysics = false;
    float BlendOutDuration    = 0.f;
    float BlendOutElapsed     = 0.f;
    float BlendOutStartWeight = 0.f;

    // ── Ragdoll state ─────────────────────────────────────────────────────────
    bool  bIsRagdoll     = false;
    float RagdollElapsed = 0.f;

    // ── IK state ──────────────────────────────────────────────────────────────
    FVector      StartPelvisPos       = FVector::ZeroVector;
    FVector2D    PushGoal;
    FFootIKState LFootState;
    FFootIKState RFootState;
    FVector      LeftHandIKPosition   = FVector::ZeroVector;
    FVector      RightHandIKPosition  = FVector::ZeroVector;
    FVector      RestLeftFootPos      = FVector::ZeroVector;
    FVector      RestRightFootPos     = FVector::ZeroVector;
    FVector      FrozenLeftFootIK     = FVector::ZeroVector;
    FVector      FrozenRightFootIK    = FVector::ZeroVector;
    FVector      FrozenPelvisWorldL   = FVector::ZeroVector;
    FVector      FrozenPelvisWorldR   = FVector::ZeroVector;
    bool         bWasLStriding        = false;
    bool         bWasRStriding        = false;

    // ── Helpers ───────────────────────────────────────────────────────────────
    void SetupComponent();
    void ProtectHit(FVector InHitDir, double InHitStrength);
    void ActivateSimBones();
    void SetupVarsForSim();
    void SetupStrides(FVector LeftGoal, FVector RightGoal, bool bIsRepositioning = false);
    bool CurveTickValues(float DeltaTime);
    void SimulationWeight();
    void PushVictim();
    void PelvisMovement();
    void ReactiveSteps(float DeltaTime);
    void SimFinish();
    void OpenTickGate();
    void LowHealthTick(float DeltaTime);
};
