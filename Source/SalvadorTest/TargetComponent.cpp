#include "TargetComponent.h"

UTargetComponent::UTargetComponent()
{
    PrimaryComponentTick.bCanEverTick = false;

    SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    SetCollisionResponseToAllChannels(ECR_Ignore);
    // Must match the sweep channel used by UTargetingSystemComponent (ECC_GameTraceChannel1).
    SetCollisionResponseToChannel(ECC_GameTraceChannel1, ECR_Block);

    // Hidden until UTargetingSystemComponent calls SetVisible(true) on aim start.
    SetHiddenInGame(true);
}

void UTargetComponent::SetVisible(bool bShow)
{
    SetHiddenInGame(!bShow);

    if (!bShow && bIsActiveTarget)
    {
        bIsActiveTarget = false;
        OnTargetStateChanged.Broadcast(false);
    }
}

void UTargetComponent::SetSelected(bool bSelected)
{
    if (bIsActiveTarget == bSelected) return;

    bIsActiveTarget = bSelected;
    OnTargetStateChanged.Broadcast(bIsActiveTarget);
}
