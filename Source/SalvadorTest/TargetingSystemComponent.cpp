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

	FVector Start = Camera->GetComponentLocation() - FVector(0,0,10);
	FVector End = Start + (Camera->GetForwardVector() * TraceRange);

	TArray<FHitResult> HitResults;
	FCollisionShape Shape = FCollisionShape::MakeSphere(TraceRadius);
	GetWorld()->SweepMultiByChannel(
		HitResults, Start, End, FQuat::Identity, ECC_GameTraceChannel1, Shape);

	UTargetComponent* BestTarget = nullptr;
	AActor* BestActor = nullptr;
	float BestDot = -1.f;

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