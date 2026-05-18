#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "AttackData.generated.h"

USTRUCT(BlueprintType)
struct FHandIKSlotData
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "IK")
    FVector PositionOffset = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "IK")
    FRotator RotationOffset = FRotator::ZeroRotator;
};

USTRUCT(BlueprintType)
struct FTargetSlotIKData
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "IK")
    FHandIKSlotData DomHand;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "IK")
    FHandIKSlotData SlaveHand;
};

USTRUCT(BlueprintType)
struct FHandIKLimits
{
    GENERATED_BODY()

    // X = min, Y = max
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Limits")
    FVector2D HeightLimit = FVector2D(-100.f, 100.f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Limits")
    FRotator RotMin = FRotator(-180.f, -180.f, -180.f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Limits")
    FRotator RotMax = FRotator(180.f, 180.f, 180.f);
};

USTRUCT(BlueprintType)
struct FAttackGlobalLimits
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Limits")
    FHandIKLimits DomHand;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Limits")
    FHandIKLimits SlaveHand;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Limits", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float PelvisInfluence = 0.5f;

    // X = min, Y = max
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Limits")
    FVector2D HipHeightLimit = FVector2D(-50.f, 50.f);
};

USTRUCT(BlueprintType)
struct FAttackMontageData : public FTableRowBase
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "IK")
    FTargetSlotIKData TopTarget;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "IK")
    FTargetSlotIKData MidTarget;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "IK")
    FTargetSlotIKData BotTarget;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Limits")
    FAttackGlobalLimits GlobalLimits;
};
