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

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class SALVADORTEST_API UHitReactionComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UHitReactionComponent();

    virtual void BeginPlay() override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    UFUNCTION(BlueprintCallable, Category = "HitReaction")
    void HitW_Physics(int32 InAttackSide, FName InBoneHit, FVector InHitDir, double InHitStrength);

    UFUNCTION(BlueprintCallable, Category = "HitReaction")
    void ActivateRagdoll();

    UFUNCTION(BlueprintCallable, Category = "HitReaction")
    void SetDeathPlantBlend(float Blend);

    // ── Ragdoll transition ────────────────────────────────────────────────────
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HitReaction|Ragdoll",
              meta = (ClampMin = "0.0"))
    float RagdollTransitionTime = 0.3f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HitReaction|Ragdoll")
    TObjectPtr<UCurveFloat> RagdollTransitionCurve;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HitReaction|Ragdoll",
              meta = (ClampMin = "0.0"))
    float RagdollImpulse = 500.f;

    // ── Health ────────────────────────────────────────────────────────────────
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HitReaction|Health")
    int32 MaxHP = 3;

    UPROPERTY(BlueprintReadOnly, Category = "HitReaction|Health")
    int32 CurrentHP = 3;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HitReaction|Health")
    TArray<FName> BlacklistedHitBones;

    // ── Stunt timing ─────────────────────────────────────────────────────────
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HitReaction|Stunt")
    double StuntTime = 1.0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HitReaction|Stunt")
    TObjectPtr<UCurveFloat> StuntCurveTop;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HitReaction|Stunt")
    TObjectPtr<UCurveFloat> StuntCurveMid;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HitReaction|Stunt")
    TObjectPtr<UCurveFloat> StuntCurveBot;

    // ── Pelvis disruption ─────────────────────────────────────────────────────
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HitReaction|Pelvis")
    double PelvisDisruption = 0.0;

    // ── Right foot stride params ──────────────────────────────────────────────
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HitReaction|FootIK|Right")
    FVector2D RStrideThreshold = FVector2D(10.f, 15.f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HitReaction|FootIK|Right")
    FVector2D RStrideDuration = FVector2D(0.15f, 0.25f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HitReaction|FootIK|Right")
    FVector2D RStrideHeight = FVector2D(8.f, 15.f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HitReaction|FootIK|Right")
    FVector2D RStrideCooldown = FVector2D(0.05f, 0.1f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HitReaction|FootIK|Right")
    FVector2D RStrideReach = FVector2D(0.f, 3.f);

    // ── Left foot stride params ───────────────────────────────────────────────
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HitReaction|FootIK|Left")
    FVector2D LStrideThreshold = FVector2D(10.f, 15.f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HitReaction|FootIK|Left")
    FVector2D LStrideDuration = FVector2D(0.15f, 0.25f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HitReaction|FootIK|Left")
    FVector2D LStrideHeight = FVector2D(8.f, 15.f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HitReaction|FootIK|Left")
    FVector2D LStrideCooldown = FVector2D(0.05f, 0.1f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HitReaction|FootIK|Left")
    FVector2D LStrideReach = FVector2D(0.f, 3.f);

    // ── Push force ───────────────────────────────────────────────────────────
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HitReaction|Push")
    double VictimPushForce = 1.0;

    // ── Bone names ────────────────────────────────────────────────────────────
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HitReaction|Bones")
    FName PelvisBoneName = TEXT("pelvis");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HitReaction|Bones")
    FName FallbackHitBone = TEXT("spine_01");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HitReaction|Bones")
    FName UpperSimBone = TEXT("spine_03");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HitReaction|Bones")
    FName MidSimBone = TEXT("spine_01");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HitReaction|Bones")
    FName LowerSimBoneL = TEXT("thigh_l");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HitReaction|Bones")
    FName LowerSimBoneR = TEXT("thigh_r");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HitReaction|Bones")
    FName VirtualPelvisBone = TEXT("VB pelvis");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HitReaction|Bones")
    FName VirtualLeftFootBone = TEXT("VB foot_l");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HitReaction|Bones")
    FName VirtualRightFootBone = TEXT("VB foot_r");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HitReaction|Bones")
    FName RootSimBone = TEXT("root");

    // ── Physical animation ────────────────────────────────────────────────────
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HitReaction|Physics")
    FName PhysicalAnimProfile = TEXT("HitReaction");

    // ── Impulse direction/drop scalars ────────────────────────────────────────
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HitReaction|Impulse")
    float RHitDirScale = 1.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HitReaction|Impulse")
    float RHitDropScale = 1.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HitReaction|Impulse")
    float LHitDirScale = 1.f;

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
    double              SimValue       = 0.0;
    FCurveSamplerState  StuntValueState;
    bool                bResetHit      = false;
    bool                bSimFinishTriggered = false;
    bool                bRepositioning = false;
    bool                bDoOnceFired   = false;

    // ── Ragdoll state ─────────────────────────────────────────────────────────
    bool  bIsRagdoll     = false;
    float RagdollElapsed = 0.f;

    // ── IK state ──────────────────────────────────────────────────────────────
    FVector     StartPelvisPos        = FVector::ZeroVector;
    FVector2D   PushGoal;
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
};
