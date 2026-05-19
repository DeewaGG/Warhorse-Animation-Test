#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TargetingSystemComponent.generated.h"

class UTargetComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTargetChanged, UTargetComponent*, NewTarget);

// Camera-forward sphere sweep that selects the most centered visible UTargetComponent each tick.
// Tick is disabled by default and only runs while bIsAiming is true.
UCLASS(ClassGroup = (Combat), meta = (BlueprintSpawnableComponent))
class SALVADORTEST_API UTargetingSystemComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UTargetingSystemComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION(BlueprintCallable, Category = "Targeting")
	void BeginAiming();

	UFUNCTION(BlueprintCallable, Category = "Targeting")
	void EndAiming();

	// Called at attack start to hide all slots on the current target actor.
	UFUNCTION(BlueprintCallable, Category = "Targeting")
	void OnAttackStart();

	// Called at attack end to restore slot visibility.
	UFUNCTION(BlueprintCallable, Category = "Targeting")
	void OnAttackEnd();

	UFUNCTION(BlueprintPure, Category = "Targeting")
	UTargetComponent* GetCurrentTarget() const { return CurrentTarget; }

	UPROPERTY(BlueprintAssignable, Category = "Targeting")
	FOnTargetChanged OnTargetChanged;

protected:
	UPROPERTY(EditDefaultsOnly, Category = "Targeting")
	float TraceRadius = 80.f;

	UPROPERTY(EditDefaultsOnly, Category = "Targeting")
	float TraceRange = 1500.f;

	UPROPERTY(EditDefaultsOnly, Category = "Targeting")
	float TraceStartHeightOffset = 0.f;

	// Must use ECC_GameTraceChannel1 to match UTargetComponent collision response.
	UPROPERTY(EditDefaultsOnly, Category = "Targeting")
	TEnumAsByte<ECollisionChannel> TraceChannel = ECC_Visibility;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting|Debug")
	bool bEnableDebug = false;

private:
	void PerformTrace();
	void SetCurrentTargetActor(AActor* NewActor);
	void SetCurrentTarget(UTargetComponent* NewTarget);

	UPROPERTY()
	TObjectPtr<UTargetComponent> CurrentTarget;

	UPROPERTY()
	TObjectPtr<AActor> CurrentTargetActor;

	// All UTargetComponents on the current target actor; cached to avoid per-frame GetComponents calls.
	UPROPERTY()
	TArray<UTargetComponent*> CachedTargetComponents;

	bool bIsAiming = false;
};
