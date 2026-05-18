#include "TargetSlotIKComponent.h"

FVector UTargetSlotIKNodes::GetHandHeightOffset(
    USkeletalMeshComponent* Mesh,
    FVector TargetWorldPos,
    FName HandBone)
{
    if (!Mesh) return FVector::ZeroVector;

    const float DeltaWorld = TargetWorldPos.Z - Mesh->GetBoneLocation(HandBone).Z;
    const float DeltaCS    = Mesh->GetComponentTransform().InverseTransformVector(FVector(0.f, 0.f, DeltaWorld)).Z;
    return FVector(0.f, 0.f, DeltaCS);
}
