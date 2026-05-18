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

    UFUNCTION(BlueprintCallable, Category = "HitImpact")
    void HitImpact(AActor* HitActor, FVector HitLocation, FName HitBone, UAnimMontage* Montage);

    UFUNCTION(BlueprintCallable, Category = "HitImpact")
    void PhysicRecovery(FVector Impulse);

protected:
    virtual void BeginPlay() override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType,
                               FActorComponentTickFunction* ThisTickFunction) override;

public:
    // ── ABP goal variable names ───────────────────────────────────────────────
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ThrustSystem|Goals")
    FName DomLocGoal;

    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ThrustSystem|Goals")
    FName DomRotGoal;

    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ThrustSystem|Goals")
    FName SlaveLocGoal;

    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ThrustSystem|Goals")
    FName SlaveRotGoal;

    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ThrustSystem|Goals")
    FName HipLocGoal;

    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ThrustSystem|Constraints")
    FName LimitBone;

    // ── Contact setup ─────────────────────────────────────────────────────────
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ThrustSystem|Setup")
    TArray<FName> ContactSockets;

    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ThrustSystem|Setup")
    TArray<FName> SkipPlantBones;

    // ── Timing ────────────────────────────────────────────────────────────────
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ThrustSystem|Timing",
              meta = (ClampMin = "0.0"))
    float HitReachDelay = 0.1f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ThrustSystem|Timing",
              meta = (ClampMin = "0.0"))
    float PlantDuration = 0.2f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ThrustSystem|Timing",
              meta = (ClampMin = "0.0"))
    float RecoverDuration = 0.3f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ThrustSystem|Timing",
              meta = (ClampMin = "0.0"))
    float ArmRecoverDuration = 0.3f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ThrustSystem|Timing",
              meta = (ClampMin = "0.0"))
    float HipRecoverDuration = 0.3f;

    // ── Reach ─────────────────────────────────────────────────────────────────
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ThrustSystem|Reach",
              meta = (ClampMin = "0.0"))
    float MaxDistFromBone = 50.f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ThrustSystem|Reach",
              meta = (ClampMin = "0.0"))
    float StabDepth = 10.f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ThrustSystem|Reach",
              meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float ArmReachPercent = 0.5f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ThrustSystem|Reach",
              meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float HipFollowPercent = 0.3f;

    // ── Spine look-at ─────────────────────────────────────────────────────────
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "SpineLook",
              meta = (ClampMin = "0.1"))
    float SpineInterpInSpeed = 15.f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "SpineLook",
              meta = (ClampMin = "0.1"))
    float SpineInterpOutSpeed = 5.f;

    // ── Additive context ──────────────────────────────────────────────────────
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ThrustSystem|Additive")
    FName PelvisBone = TEXT("pelvis");

    // ── Debug ─────────────────────────────────────────────────────────────────
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ThrustSystem|Debug")
    bool bDebug = false;

private:
    UPROPERTY() TObjectPtr<ACharacter>        OwnerCharacter;
    UPROPERTY() TObjectPtr<UAnimInstanceBase> AnimInstance;

    FThrustState State;
    bool         bThrustActive = false;
    bool         bThrust       = false;
    bool         bRecover      = false;

    float SpineCurrentAlpha = 0.f;
    float SpineTargetAlpha  = 0.f;
};
