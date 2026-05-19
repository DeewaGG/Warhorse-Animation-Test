#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TargetingSystemComponent.generated.h"

class UTargetComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTargetChanged, UTargetComponent*, NewTarget);

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

	UFUNCTION(BlueprintCallable, Category = "Targeting")
	void OnAttackStart();

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

	UPROPERTY()
	TArray<UTargetComponent*> CachedTargetComponents;

	bool bIsAiming = false;
};