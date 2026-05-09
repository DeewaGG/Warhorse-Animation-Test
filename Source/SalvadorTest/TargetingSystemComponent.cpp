#include "TargetingSystemComponent.h"
#include "TargetComponent.h"

#include "Camera/CameraComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "GameFramework/Actor.h"

UTargetingSystemComponent::UTargetingSystemComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UTargetingSystemComponent::TickComponent(
    float DeltaTime,
    ELevelTick TickType,
    FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    PerformTrace();
}

void UTargetingSystemComponent::BeginAiming()
{
    bIsAiming = true;
    SetComponentTickEnabled(true);
}

void UTargetingSystemComponent::EndAiming()
{
    bIsAiming = false;

    SetComponentTickEnabled(false);

    SetCurrentTarget(nullptr);
}

void UTargetingSystemComponent::PerformTrace()
{
    AActor* Owner = GetOwner();
    if (!Owner)
    {
        return;
    }

    UCameraComponent* Camera = Owner->FindComponentByClass<UCameraComponent>();
    if (!Camera)
    {
        return;
    }

    FVector Start = Camera->GetComponentLocation();
    FVector End = Start + (Camera->GetForwardVector() * TraceRange);

    FHitResult HitResult;

    FCollisionShape Shape = FCollisionShape::MakeSphere(TraceRadius);

    bool bHit = GetWorld()->SweepSingleByChannel(
        HitResult,
        Start,
        End,
        FQuat::Identity,
        ECC_GameTraceChannel1,
        Shape
    );

    if (bEnableDebug)
    {
        DrawDebugLine(
            GetWorld(),
            Start,
            End,
            bHit ? FColor::Green : FColor::Red,
            false,
            0.1f,
            0,
            1.5f
        );

        DrawDebugSphere(
            GetWorld(),
            bHit ? HitResult.ImpactPoint : End,
            TraceRadius,
            12,
            bHit ? FColor::Green : FColor::Red,
            false,
            0.1f
        );

        if (bHit && GEngine && HitResult.GetComponent())
        {
            FString ComponentName = HitResult.GetComponent()->GetName();

            GEngine->AddOnScreenDebugMessage(
                -1,
                0.1f,
                FColor::Yellow,
                FString::Printf(TEXT("Hit Component: %s"), *ComponentName)
            );
        }
    }

    UTargetComponent* HitTarget = nullptr;

    if (bHit)
    {
        HitTarget = Cast<UTargetComponent>(HitResult.GetComponent());
    }

    SetCurrentTarget(HitTarget);
}

void UTargetingSystemComponent::SetCurrentTarget(UTargetComponent* NewTarget)
{
    if (CurrentTarget == NewTarget)
    {
        return;
    }

    if (CurrentTarget)
    {
        CurrentTarget->SetTargetActive(false);
    }

    CurrentTarget = NewTarget;

    if (CurrentTarget)
    {
        CurrentTarget->SetTargetActive(true);
    }

    OnTargetChanged.Broadcast(CurrentTarget);
}