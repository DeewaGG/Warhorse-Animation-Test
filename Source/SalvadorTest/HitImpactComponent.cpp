#include "HitImpactComponent.h"
#include "AnimInstanceBase.h"
#include "HitReactionComponent.h"
#include "ThrustSystemNodes.h"
#include "PlayableCharacter.h"
#include "PhysicsEngine/PhysicalAnimationComponent.h"
#include "GameFramework/Character.h"
#include "Components/SkeletalMeshComponent.h"
#include "Kismet/KismetSystemLibrary.h"

UHitImpactComponent::UHitImpactComponent()
{
    PrimaryComponentTick.bCanEverTick          = true;
    PrimaryComponentTick.bStartWithTickEnabled = false;
    StabTraceChannel = ECC_GameTraceChannel2;
}

void UHitImpactComponent::BeginPlay()
{
    Super::BeginPlay();
    OwnerCharacter = Cast<ACharacter>(GetOwner());
    if (!OwnerCharacter) return;
    USkeletalMeshComponent* CharMesh = OwnerCharacter->GetMesh();
    if (!CharMesh) return;
    AnimInstance = Cast<UAnimInstanceBase>(CharMesh->GetAnimInstance());
}

void UHitImpactComponent::TickComponent(float DeltaTime, ELevelTick TickType,
                                         FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (DeltaTime <= 0.f) return;

    // Spine look-at interpolates independently of thrust phase so it fades out smoothly
    // after ThrustEnd even when the thrust state machine has already stopped.
    const float SpineInterp = (SpineTargetAlpha < SpineCurrentAlpha)
        ? SpineInterpOutSpeed : SpineInterpInSpeed;
    SpineCurrentAlpha = FMath::FInterpTo(SpineCurrentAlpha, SpineTargetAlpha, DeltaTime, SpineInterp);
    if (AnimInstance) AnimInstance->SpineLookAtAlpha = SpineCurrentAlpha;

    // Three-phase thrust state machine: approach -> plant -> recover.
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
                if (OwnerCharacter)
                    OwnerCharacter->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
            }
        }
        else if (bThrust)
        {
            bool bComplete = false;
            UThrustSystemNodes::ThrustPlant(State, DeltaTime, bComplete);

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

    // Stop ticking once thrust is fully done and the spine alpha has faded to near-zero.
    if (!bThrustActive && SpineCurrentAlpha < 0.001f)
    {
        if (AnimInstance) AnimInstance->SpineLookAtAlpha = 0.f;
        SpineCurrentAlpha = 0.f;
        SetComponentTickEnabled(false);
    }
}

void UHitImpactComponent::DisableNearbyVictimsPhysics()
{
    if (!OwnerCharacter) return;

    TArray<AActor*> OverlappedActors;
    TArray<TEnumAsByte<EObjectTypeQuery>> ObjectTypes = { UEngineTypes::ConvertToObjectType(ECC_Pawn) };
    TArray<AActor*> Ignored = { OwnerCharacter };

    UKismetSystemLibrary::SphereOverlapActors(
        this, OwnerCharacter->GetActorLocation(),
        NearbyPhysicsDisableRadius, ObjectTypes,
        nullptr, Ignored, OverlappedActors);

    for (AActor* Actor : OverlappedActors)
    {
        if (UHitReactionComponent* HRC = Actor->FindComponentByClass<UHitReactionComponent>())
            HRC->BlendOutPhysics(NearbyPhysicsBlendOutDuration);
    }
}

void UHitImpactComponent::HitImpact(AActor* HitActor, FVector HitLocation,
                                     FName HitBone, UAnimMontage* Montage)
{
    if (!AnimInstance || !Montage) return;

    VictimActor = HitActor;
    SetComponentTickEnabled(true);
    bThrustActive = true;
    bThrust       = false;
    bRecover      = false;

    if (OwnerCharacter)
        OwnerCharacter->GetCharacterMovement()->DisableMovement();

    AnimInstance->bHit = false;

    AnimInstance->SetSpineLookAtTarget(HitActor ? HitActor->GetActorLocation() : HitLocation);
    // SpineTargetAlpha stays 0 here; ThrustPlant ramps SpineLookAtAlpha directly.
    // This component only fades the alpha back out after ThrustEnd.
    SpineTargetAlpha = 0.f;

    CapturedMontageRate   = AnimInstance->Montage_GetPlayRate(Montage);
    CapturedMontageRawPos = AnimInstance->Montage_GetPosition(Montage);

    // Compute normal reverse parameters. Exit cases may override these after ThrustPlant completes.
    const float MontageRate = CapturedMontageRate * ReverseRateMultiplier;
    const float MontagePos  = bForceFixedReverseFrame
        ? FixedReversePosition
        : CapturedMontageRawPos - ReverseStartOffset;
    AnimInstance->Montage_SetPlayRate(Montage, 0.f);

    // Transform the DataTable per-slot position offset (stored in attacker component space) to
    // world space so ThrustSetUp receives the correctly shifted target regardless of attacker orientation.
    FVector EffHitLocation = HitLocation;
    if (APlayableCharacter* PC = Cast<APlayableCharacter>(GetOwner()))
    {
        const FVector& CSOffset = PC->CurrentSlotData.DomHand.PositionOffset;
        if (!CSOffset.IsZero())
        {
            USkeletalMeshComponent* Mesh = OwnerCharacter ? OwnerCharacter->GetMesh() : nullptr;
            if (Mesh)
                EffHitLocation += Mesh->GetComponentTransform().TransformVector(CSOffset);
        }

        if (HitActor)
        {
            if (UHitReactionComponent* HRC = HitActor->FindComponentByClass<UHitReactionComponent>())
            {
                if (HRC->CurrentHP == 1)
                    EffHitLocation.Z += PC->CurrentSlotData.DomHand.WoundedZOffset;
            }
        }
    }

    float EffPlantDuration = PlantDuration;
    if (HitActor)
    {
        if (UHitReactionComponent* HRC = HitActor->FindComponentByClass<UHitReactionComponent>())
        {
            if (HRC->BlacklistedHitBones.Contains(HitBone))
                EffPlantDuration = BlacklistedPlantDuration;
        }
    }

    UThrustSystemNodes::ThrustSetUp(
        State, GetOwner(), HitActor,
        DomLocGoal, DomRotGoal, SlaveLocGoal, SlaveRotGoal,
        ContactSockets,
        EffHitLocation, HitBone,
        HitReachDelay, EffPlantDuration, Montage, MontagePos, MontageRate,
        RecoverDuration, LimitBone, MaxDistFromBone,
        StabDepth, ArmReachPercent,
        HipLocGoal, HipFollowPercent,
        ArmRecoverDuration, HipRecoverDuration,
        bDebug,
        PelvisBone,
        SpineRotVarName,
        SpineAlphaVarName,
        StabTraceChannel
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
    PhysAnim->ApplyPhysicalAnimationProfileBelow(BoneName, PhysicRecoveryProfile, true, true);

    if (ImpulseMagnitude > 0.f)
        Mesh->AddImpulse(ImpulseDirection.GetSafeNormal() * ImpulseMagnitude, BoneName, true);
}
