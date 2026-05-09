#include "TargetComponent.h"

UTargetComponent::UTargetComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

	SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	SetCollisionResponseToAllChannels(ECR_Ignore);
	SetCollisionResponseToChannel(ECC_GameTraceChannel1, ECR_Block);
}

void UTargetComponent::SetTargetActive(bool bActive)
{
	if (bIsActiveTarget == bActive) return;

	bIsActiveTarget = bActive;

	OnTargetStateChanged.Broadcast(bIsActiveTarget);
}