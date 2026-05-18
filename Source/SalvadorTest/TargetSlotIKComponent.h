#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Components/SkeletalMeshComponent.h"
#include "TargetSlotIKComponent.generated.h"

UCLASS()
class SALVADORTEST_API UTargetSlotIKNodes : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintPure, Category = "TargetSlotIK")
    static FVector GetHandHeightOffset(
        USkeletalMeshComponent* Mesh,
        FVector TargetWorldPos,
        FName HandBone);
};
