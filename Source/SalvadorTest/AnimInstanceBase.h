#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "FootIKNodes.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "AnimInstanceBase.generated.h"

UCLASS()
class SALVADORTEST_API UAnimInstanceBase : public UAnimInstance
{
    GENERATED_BODY()

public:
    virtual void NativeInitializeAnimation() override;
    virtual void NativeUpdateAnimation(float DeltaSeconds) override;

    UPROPERTY(BlueprintReadWrite, Category = "State")
    bool bIsStunned = false;

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
    FVector LeftHandIKPosition = FVector::ZeroVector;

    UPROPERTY(BlueprintReadOnly, Category = "FootIK")
    FVector RightHandIKPosition = FVector::ZeroVector;

    UPROPERTY(BlueprintReadWrite, Category = "FootIK")
    FVector PelvisGoalPosition = FVector::ZeroVector;

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
    void GetMovComp();
    void VelocityAndSpeed();
    void CalculateDirections();
    void ShouldMove();
    void IsFalling();
    void BodyIK(float DeltaSeconds);
    void TraceFootIK(FName FootBone, float DeltaSeconds, FVector& OutPos, FRotator& OutRot);
};
