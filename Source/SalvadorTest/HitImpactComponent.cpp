#include "HitImpactComponent.h"
#include "AnimInstanceBase.h"
#include "HitReactionComponent.h"
#include "ThrustSystemNodes.h"
#include "PhysicsEngine/PhysicalAnimationComponent.h"
#include "GameFramework/Character.h"
#include "Components/SkeletalMeshComponent.h"

UHitImpactComponent::UHitImpactComponent()
{
    PrimaryComponentTick.bCanEverTick          = true;
    PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UHitImpactComponent::BeginPlay()
{
    Super::BeginPlay();
    OwnerCharacter = Cast<ACharacter>(GetOwner());
    if (!OwnerCharacter) return;
    AnimInstance = Cast<UAnimInstanceBase>(OwnerCharacter->GetMesh()->GetAnimInstance());
}

void UHitImpactComponent::TickComponent(float DeltaTime, ELevelTick TickType,
                                         FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    // Spine look-at alpha — interpolates independently of thrust phase
    const float SpineInterp = (SpineTargetAlpha < SpineCurrentAlpha)
        ? SpineInterpOutSpeed : SpineInterpInSpeed;
    SpineCurrentAlpha = FMath::FInterpTo(SpineCurrentAlpha, SpineTargetAlpha, DeltaTime, SpineInterp);
    if (AnimInstance) AnimInstance->SpineLookAtAlpha = SpineCurrentAlpha;

    // Thrust state machine
    if (bThrustActive)
    {
        if (bRecover)
        {
            bool bComplete = false;
            UThrustSystemNodes::ThrustRecover(State, DeltaTime, bComplete);
            if (bComplete)
            {
                UThrustSystemNodes::ThrustEnd(State);
                bThrust       = false;
                bRecover      = false;
                bThrustActive = false;
                if (AnimInstance) AnimInstance->bHit = false;
                SpineTargetAlpha = 0.f;
            }
        }
        else if (bThrust)
        {
            bool bBlacklisted = false;
            bool bComplete    = false;
            UThrustSystemNodes::ThrustPlant(State, DeltaTime, bBlacklisted, bComplete);

            bRecover = bComplete;
            if (bRecover && VictimActor)
            {
                if (UHitReactionComponent* HRC = VictimActor->FindComponentByClass<UHitReactionComponent>())
                {
                    if (HRC->CurrentHP <= 0)
                        HRC->ActivateRagdoll();
                }
            }
        }
        else
        {
            bool bComplete = false;
            UThrustSystemNodes::ThrustTick(State, DeltaTime, bComplete);
            bThrust = bComplete;
        }
    }

    // Stop ticking once thrust is done and spine has fully faded out
    if (!bThrustActive && SpineCurrentAlpha < 0.001f)
    {
        if (AnimInstance) AnimInstance->SpineLookAtAlpha = 0.f;
        SpineCurrentAlpha = 0.f;
        SetComponentTickEnabled(false);
    }
}

void UHitImpactComponent::HitImpact(AActor* HitActor, FVector HitLocation,
                                     FName HitBone, UAnimMontage* Montage)
{
    if (!AnimInstance || !Montage) return;

    VictimActor   = HitActor;
    SetComponentTickEnabled(true);
    bThrustActive = true;
    bThrust       = false;
    bRecover      = false;

    AnimInstance->bHit = false;

    AnimInstance->SetSpineLookAtTarget(HitActor ? HitActor->GetActorLocation() : HitLocation);
    SpineTargetAlpha = 0.f; // ThrustPlant drives SpineLookAtAlpha directly; component only fades post-ThrustEnd

    const float MontagePos = AnimInstance->Montage_GetPosition(Montage);
    AnimInstance->Montage_SetPlayRate(Montage, 0.f);

    UThrustSystemNodes::ThrustSetUp(
        State, GetOwner(), HitActor,
        DomLocGoal, DomRotGoal, SlaveLocGoal, SlaveRotGoal,
        ContactSockets, SkipPlantBones,
        HitLocation, HitBone,
        HitReachDelay, PlantDuration, Montage, MontagePos,
        RecoverDuration, LimitBone, MaxDistFromBone,
        StabDepth, ArmReachPercent,
        HipLocGoal, HipFollowPercent,
        ArmRecoverDuration, HipRecoverDuration,
        bDebug,
        PelvisBone,
        TEXT("SpineLookAtAdditiveRot"),
        TEXT("SpineLookAtAlpha")
    );

    AnimInstance->bHit = true;
}

void UHitImpactComponent::PhysicRecovery(FName BoneName, FVector ImpulseDirection, float ImpulseMagnitude)
{
    if (!OwnerCharacter || BoneName == NAME_None) return;
    USkeletalMeshComponent* Mesh = OwnerCharacter->GetMesh();
    if (!Mesh) return;

    UPhysicalAnimationComponent* PhysAnim =
        NewObject<UPhysicalAnimationComponent>(OwnerCharacter);
    PhysAnim->RegisterComponent();
    PhysAnim->SetSkeletalMeshComponent(Mesh);

    Mesh->SetAllBodiesBelowSimulatePhysics(BoneName, true, true);
    PhysAnim->ApplyPhysicalAnimationProfileBelow(BoneName, TEXT("HitReaction"), true, true);

    if (ImpulseMagnitude > 0.f)
        Mesh->AddImpulse(ImpulseDirection.GetSafeNormal() * ImpulseMagnitude, BoneName, true);
}
