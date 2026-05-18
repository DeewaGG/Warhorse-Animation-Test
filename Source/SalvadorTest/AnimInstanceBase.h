#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "FootIKNodes.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "AnimInstanceBase.generated.h"

class APlayableCharacter;

UCLASS()
class SALVADORTEST_API UAnimInstanceBase : public UAnimInstance
{
    GENERATED_BODY()

public:
    virtual void NativeInitializeAnimation() override;
    virtual void NativeUpdateAnimation(float DeltaSeconds) override;

    UPROPERTY(BlueprintReadWrite, Category = "State")
    bool bIsStunned = false;

    UPROPERTY(BlueprintReadWrite, Category = "State")
    bool bHit = false;

    // ── Foot IK goals (written by HitReactionComponent and ABP children) ─────
    UPROPERTY(BlueprintReadWrite, Category = "FootIK")
    FVector LeftFootIKPosition = FVector::ZeroVector;

    UPROPERTY(BlueprintReadWrite, Category = "FootIK")
    FRotator LeftFootRot = FRotator::ZeroRotator;

    UPROPERTY(BlueprintReadWrite, Category = "FootIK")
    FVector RightFootIKPosition = FVector::ZeroVector;

    UPROPERTY(BlueprintReadWrite, Category = "FootIK")
    FRotator RightFootRot = FRotator::ZeroRotator;

    UPROPERTY(BlueprintReadOnly, Category = "FootIK")
    float LeftFootIKAlpha = 0.f;

    UPROPERTY(BlueprintReadOnly, Category = "FootIK")
    float RightFootIKAlpha = 0.f;

    UPROPERTY(BlueprintReadWrite, Category = "FootIK")
    FVector LeftHandIKPosition = FVector::ZeroVector;

    UPROPERTY(BlueprintReadWrite, Category = "FootIK")
    FRotator LeftHandIKRotation = FRotator::ZeroRotator;

    UPROPERTY(BlueprintReadWrite, Category = "FootIK")
    FVector RightHandIKPosition = FVector::ZeroVector;

    UPROPERTY(BlueprintReadWrite, Category = "FootIK")
    FRotator RightHandIKRotation = FRotator::ZeroRotator;

    UPROPERTY(BlueprintReadWrite, Category = "FootIK")
    FVector PelvisGoalPosition = FVector::ZeroVector;

    UPROPERTY(BlueprintReadWrite, Category = "FootIK")
    FVector PlantedDomLoc = FVector::ZeroVector;

    UPROPERTY(BlueprintReadWrite, Category = "FootIK")
    FRotator PlantedDomRot = FRotator::ZeroRotator;

    UPROPERTY(BlueprintReadWrite, Category = "FootIK")
    FVector PlantedSlaveLoc = FVector::ZeroVector;

    UPROPERTY(BlueprintReadWrite, Category = "FootIK")
    FRotator PlantedSlaveRot = FRotator::ZeroRotator;

    UPROPERTY(BlueprintReadOnly, Category = "Combat|IK")
    FVector HandHeightAdditiveOffset = FVector::ZeroVector;

protected:

    // ── References ───────────────────────────────────────────────────────────
    UPROPERTY(BlueprintReadOnly, Category = "References")
    TObjectPtr<UCharacterMovementComponent> MovementComponent;

    UPROPERTY(BlueprintReadOnly, Category = "References")
    TObjectPtr<USkeletalMeshComponent> OwnerMesh;

    // ── Movement ─────────────────────────────────────────────────────────────
    UPROPERTY(BlueprintReadOnly, Category = "Movement")
    FVector Velocity = FVector::ZeroVector;

    UPROPERTY(BlueprintReadOnly, Category = "Movement")
    float Speed = 0.f;

    UPROPERTY(BlueprintReadOnly, Category = "Movement")
    float Direction = 0.f;

    UPROPERTY(BlueprintReadOnly, Category = "Movement")
    bool bShouldMove = false;

    UPROPERTY(BlueprintReadOnly, Category = "Movement")
    bool bIsFalling = false;

    // ── PlayableCharacter data (read by ABP children) ─────────────────────────
    UPROPERTY(BlueprintReadOnly, Category = "Movement")
    float TurningSpeed = 0.f;

    UPROPERTY(BlueprintReadOnly, Category = "Movement")
    bool bTurningR = false;

    UPROPERTY(BlueprintReadOnly, Category = "Movement")
    bool bTurningL = false;

    UPROPERTY(BlueprintReadOnly, Category = "Movement")
    FVector CamForward = FVector::ZeroVector;

    UPROPERTY(BlueprintReadOnly, Category = "Combat")
    FVector TargetPos = FVector::ZeroVector;

    UPROPERTY(BlueprintReadOnly, Category = "Combat")
    float TargetAlpha = 0.f;

    UPROPERTY(EditDefaultsOnly, Category = "Combat", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float TargetOriMinInfluence = 0.8f;

    UPROPERTY(EditDefaultsOnly, Category = "Combat")
    FName PelvisBone = TEXT("pelvis");

    UPROPERTY(EditDefaultsOnly, Category = "Combat", meta = (ClampMin = "0.0"))
    float HandHeightMaxOffset = 50.f;

    // ── Foot IK ──────────────────────────────────────────────────────────────
    UPROPERTY(BlueprintReadWrite, Category = "FootIK|Setup")
    FName FootL;

    UPROPERTY(BlueprintReadWrite, Category = "FootIK|Setup")
    FName FootR;

    UPROPERTY(BlueprintReadOnly, Category = "FootIK")
    FVector BodyIKOffset = FVector::ZeroVector;

    UPROPERTY(BlueprintReadWrite, Category = "FootIK")
    FRotator FootRGoalRotation = FRotator::ZeroRotator;

    UPROPERTY(BlueprintReadWrite, Category = "FootIK")
    FRotator FootLGoalRotation = FRotator::ZeroRotator;

    UPROPERTY(BlueprintReadWrite, Category = "FootIK|State")
    FFootIKState LeftFootState;

    UPROPERTY(BlueprintReadWrite, Category = "FootIK|State")
    FFootIKState RightFootState;

    UPROPERTY(BlueprintReadWrite, Category = "FootIK|Setup")
    float FootInterpSpeed = 10.f;

private:
    UPROPERTY() TObjectPtr<APlayableCharacter> OwningPlayableCharacter;

    void GetMovComp();
    void VelocityAndSpeed();
    void CalculateDirections();
    void ShouldMove();
    void IsFalling();
    void BodyIK(float DeltaSeconds);
    void TraceFootIK(FName FootBone, float DeltaSeconds, FVector& OutPos, FRotator& OutRot, float& OutAlpha);
    void SyncPlayableCharacterData();
    void ComputeTargetAlpha();
    void ComputeHandHeightIK();
};
