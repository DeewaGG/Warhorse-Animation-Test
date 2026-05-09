#pragma once

#include "CoreMinimal.h"
#include "Components/StaticMeshComponent.h"
#include "TargetComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FTargetStateChangedSignature, bool, bNewState);

UCLASS(ClassGroup = (Combat), meta = (BlueprintSpawnableComponent))
class SALVADORTEST_API UTargetComponent : public UStaticMeshComponent
{
	GENERATED_BODY()

public:
	UTargetComponent();

	UFUNCTION(BlueprintCallable, Category = "Targeting")
	void SetTargetActive(bool bActive);

	bool IsActiveTarget() const { return bIsActiveTarget; }

	UPROPERTY(BlueprintAssignable, Category = "Targeting")
	FTargetStateChangedSignature OnTargetStateChanged;

private:
	bool bIsActiveTarget = false;
};