#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "InputActionValue.h"
#include "AttackData.h"
#include "PlayableCharacter.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UTargetingSystemComponent;
class UInputMappingContext;
class UInputAction;
class UAnimMontage;
class UAnimInstanceBase;
class UHitImpactComponent;

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

    UPROPERTY(EditDefaultsOnly, Category = "Combat")
    TObjectPtr<UDataTable> AttackDataTable;

    // ── ABP-facing variables ──────────────────────────────────────────────────
    UPROPERTY(BlueprintReadWrite, Category = "Animation")
    bool Turning_R = false;

    UPROPERTY(BlueprintReadWrite, Category = "Animation")
    bool Turning_L = false;

    UPROPERTY(BlueprintReadWrite, Category = "Animation")
    double Turning_Speed = 0.0;

    UPROPERTY(BlueprintReadWrite, Category = "Animation")
    FVector Cam_Forward = FVector::ZeroVector;

    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Animation",
              meta = (ClampMin = "0.0"))
    float TurningSpeedMultiplier = 1.f;

    UPROPERTY(BlueprintReadWrite, Category = "Combat")
    FVector TargetPos = FVector::ZeroVector;

    UPROPERTY(BlueprintReadWrite, Category = "Combat")
    FTargetSlotIKData CurrentSlotData;

    UPROPERTY(BlueprintReadWrite, Category = "Combat")
    FAttackGlobalLimits CurrentLimits;

private:
    UPROPERTY() TObjectPtr<UAnimInstanceBase>    ABP;
    UPROPERTY() TObjectPtr<UHitImpactComponent>  HitImpactComp;

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

    UFUNCTION()
    void OnAttackMontageEnded(UAnimMontage* Montage, bool bInterrupted);
};
