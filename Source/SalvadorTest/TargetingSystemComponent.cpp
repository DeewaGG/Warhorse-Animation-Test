#include "TargetingSystemComponent.h"
#include "TargetComponent.h"
#include "Camera/CameraComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "GameFramework/Actor.h"

UTargetingSystemComponent::UTargetingSystemComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	// Only ticks while aiming; enabled/disabled by BeginAiming/EndAiming.
	PrimaryComponentTick.bStartWithTickEnabled = false;
	TraceChannel = ECC_GameTraceChannel1;
}

void UTargetingSystemComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
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
	SetCurrentTargetActor(nullptr);
	SetCurrentTarget(nullptr);
}

void UTargetingSystemComponent::PerformTrace()
{
	AActor* Owner = GetOwner();
	if (!Owner) return;

	UCameraComponent* Camera = Owner->FindComponentByClass<UCameraComponent>();
	if (!Camera) return;

	UWorld* World = GetWorld();
	if (!World) return;

	FVector Start = Camera->GetComponentLocation() + FVector(0.f, 0.f, TraceStartHeightOffset);
	FVector End = Start + (Camera->GetForwardVector() * TraceRange);

	TArray<FHitResult> HitResults;
	FCollisionShape Shape = FCollisionShape::MakeSphere(TraceRadius);
	World->SweepMultiByChannel(
		HitResults, Start, End, FQuat::Identity, TraceChannel, Shape);

	UTargetComponent* BestTarget = nullptr;
	AActor* BestActor = nullptr;
	float BestDot = -1.f;

	// Pick the component with the highest dot product against camera forward — i.e., the most
	// centered target from the player's perspective.
	for (const FHitResult& Hit : HitResults)
	{
		UTargetComponent* TC = Cast<UTargetComponent>(Hit.GetComponent());
		if (!TC) continue;

		FVector ToTarget = (TC->GetComponentLocation() - Start).GetSafeNormal();
		float Dot = FVector::DotProduct(Camera->GetForwardVector(), ToTarget);

		if (Dot > BestDot)
		{
			BestDot = Dot;
			BestTarget = TC;
			BestActor = Hit.GetActor();
		}
	}

	if (bEnableDebug)
		DrawDebugLine(GetWorld(), Start, End, FColor::Green, false, -1.f, 0, 1.f);

	SetCurrentTargetActor(BestActor);
	SetCurrentTarget(BestTarget);
}

void UTargetingSystemComponent::OnAttackStart()
{
	for (UTargetComponent* Target : CachedTargetComponents)
	{
		if (Target) Target->SetVisible(false);
	}
}

void UTargetingSystemComponent::OnAttackEnd()
{
	for (UTargetComponent* Target : CachedTargetComponents)
	{
		if (Target) Target->SetVisible(true);
	}
}

void UTargetingSystemComponent::SetCurrentTargetActor(AActor* NewActor)
{
	if (CurrentTargetActor == NewActor) return;

	for (UTargetComponent* Target : CachedTargetComponents)
	{
		if (Target) Target->SetVisible(false);
	}

	// Reset the cache; the new actor's components are fetched below.
	CachedTargetComponents.Reset();
	CurrentTargetActor = NewActor;

	if (!CurrentTargetActor) return;

	CurrentTargetActor->GetComponents<UTargetComponent>(CachedTargetComponents);

	for (UTargetComponent* Target : CachedTargetComponents)
	{
		if (Target) Target->SetVisible(true);
	}
}

void UTargetingSystemComponent::SetCurrentTarget(UTargetComponent* NewTarget)
{
	if (CurrentTarget == NewTarget) return;

	if (CurrentTarget)
		CurrentTarget->SetSelected(false);

	CurrentTarget = NewTarget;

	if (CurrentTarget)
		CurrentTarget->SetSelected(true);

	OnTargetChanged.Broadcast(CurrentTarget);
}
