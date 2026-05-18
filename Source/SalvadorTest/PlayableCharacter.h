#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "InputActionValue.h"
#include "PlayableCharacter.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UTargetingSystemComponent;
class UInputMappingContext;
class UInputAction;
class UAnimMontage;
class UAnimInstanceBase;

UCLASS()
class SALVADORTEST_API APlayableCharacter : public ACharacter
{
    GENERATED_BODY()

public:
    APlayableCharacter();

    virtual void BeginPlay() override;
    virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

    // ── Components ────────────────────────────────────────────────────────────
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
    TObjectPtr<USpringArmComponent> CameraBoom;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
    TObjectPtr<UCameraComponent> FollowCamera;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Targeting")
    TObjectPtr<UTargetingSystemComponent> TargetingSystem;

    // ── Input ─────────────────────────────────────────────────────────────────
    UPROPERTY(EditDefaultsOnly, Category = "Input")
    TObjectPtr<UInputMappingContext> DefaultMappingContext;

    UPROPERTY(EditDefaultsOnly, Category = "Input")
    TObjectPtr<UInputAction> IA_Move;

    UPROPERTY(EditDefaultsOnly, Category = "Input")
    TObjectPtr<UInputAction> IA_MouseLook;

    UPROPERTY(EditDefaultsOnly, Category = "Input")
    TObjectPtr<UInputAction> IA_Attack;

    UPROPERTY(EditDefaultsOnly, Category = "Input")
    TObjectPtr<UInputAction> IA_Aim;

    // ── Combat ────────────────────────────────────────────────────────────────
    UPROPERTY(EditDefaultsOnly, Category = "Combat")
    TObjectPtr<UAnimMontage> AttackMontage;

    // ── ABP-facing variables ──────────────────────────────────────────────────
    UPROPERTY(BlueprintReadWrite, Category = "Animation")
    bool Turning_R = false;

    UPROPERTY(BlueprintReadWrite, Category = "Animation")
    bool Turning_L = false;

    UPROPERTY(BlueprintReadWrite, Category = "Animation")
    double Turning_Speed = 0.0;

    UPROPERTY(BlueprintReadWrite, Category = "Animation")
    FVector Cam_Forward = FVector::ZeroVector;

    UPROPERTY(BlueprintReadWrite, Category = "Combat")
    FVector TargetPos = FVector::ZeroVector;

private:
    UPROPERTY() TObjectPtr<UAnimInstanceBase> ABP;
    FTimerHandle AttackTimerHandle;

    void OnMove(const FInputActionValue& Value);
    void OnMouseLook(const FInputActionValue& Value);
    void OnMouseLookCompleted(const FInputActionValue& Value);
    void OnAttackStarted(const FInputActionValue& Value);
    void OnAimStarted(const FInputActionValue& Value);
    void OnAimCanceled(const FInputActionValue& Value);
    void OnAimCompleted(const FInputActionValue& Value);
    void TurnValuesUpdate(double Axis);
    void CamForwardUpdate();
    void AnimVars_BeginPlay();
    void FinishAttack();
};
