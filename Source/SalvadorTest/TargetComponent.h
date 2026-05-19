#pragma once

#include "CoreMinimal.h"
#include "Components/StaticMeshComponent.h"
#include "TargetComponent.generated.h"

UENUM(BlueprintType)
enum class ETargetSlot : uint8
{
    TopTarget UMETA(DisplayName = "Top Target"),
    MidTarget UMETA(DisplayName = "Mid Target"),
    BotTarget UMETA(DisplayName = "Bot Target")
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FTargetStateChangedSignature, bool, bNewState);

// Static mesh component placed on enemy characters as a hittable target slot.
// Hidden by default; UTargetingSystemComponent shows/hides it based on aim state.
UCLASS(ClassGroup = (Combat), meta = (BlueprintSpawnableComponent))
class SALVADORTEST_API UTargetComponent : public UStaticMeshComponent
{
    GENERATED_BODY()

public:
    UTargetComponent();

    UFUNCTION(BlueprintCallable, Category = "Targeting")
    void SetVisible(bool bShow);

    UFUNCTION(BlueprintCallable, Category = "Targeting")
    void SetSelected(bool bSelected);

    bool IsActiveTarget() const { return bIsActiveTarget; }

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting")
    ETargetSlot TargetSlot;

    // Fires when this slot becomes selected or deselected; BP_Victim uses this to update visuals.
    UPROPERTY(BlueprintAssignable, Category = "Targeting")
    FTargetStateChangedSignature OnTargetStateChanged;

private:
    bool bIsActiveTarget = false;
};
