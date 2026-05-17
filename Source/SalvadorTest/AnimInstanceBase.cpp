#include "AnimInstanceBase.h"
#include "GameFramework/Character.h"
#include "KismetAnimationLibrary.h"
#include "Engine/World.h"

void UAnimInstanceBase::NativeInitializeAnimation()
{
    Super::NativeInitializeAnimation();
    GetMovComp();
}

void UAnimInstanceBase::NativeUpdateAnimation(float DeltaSeconds)
{
    Super::NativeUpdateAnimation(DeltaSeconds);

    if (!MovementComponent) return;

    VelocityAndSpeed();
    CalculateDirections();
    ShouldMove();
    IsFalling();
    BodyIK(DeltaSeconds);
}

void UAnimInstanceBase::GetMovComp()
{
    ACharacter* Character = Cast<ACharacter>(GetOwningActor());
    if (!Character)
    {
        UE_LOG(LogTemp, Error, TEXT("AnimInstanceBase: owning actor is not a Character"));
        return;
    }
    MovementComponent = Character->GetCharacterMovement();
    OwnerMesh         = Character->GetMesh();
}

void UAnimInstanceBase::VelocityAndSpeed()
{
    Velocity = MovementComponent->Velocity;
    Speed    = Velocity.Size2D();
}

void UAnimInstanceBase::CalculateDirections()
{
    APawn* Pawn = TryGetPawnOwner();
    if (!Pawn) return;
    Direction = UKismetAnimationLibrary::CalculateDirection(Velocity, Pawn->GetActorRotation());
}

void UAnimInstanceBase::ShouldMove()
{
    bShouldMove = Speed > 0.01f;
}

void UAnimInstanceBase::IsFalling()
{
    bIsFalling = MovementComponent->IsFalling();
}

void UAnimInstanceBase::BodyIK(float DeltaSeconds)
{
    if (bIsFalling)
    {
        LeftFootIKPosition  = FVector::ZeroVector;
        LeftFootRot         = FRotator::ZeroRotator;
        RightFootIKPosition = FVector::ZeroVector;
        RightFootRot        = FRotator::ZeroRotator;
        LeftHandIKPosition  = FVector::ZeroVector;
        RightHandIKPosition = FVector::ZeroVector;
        BodyIKOffset        = FVector::ZeroVector;
        return;
    }

    if (!OwnerMesh) return;

    TraceFootIK(FootL, DeltaSeconds, LeftFootIKPosition,  LeftFootRot);
    TraceFootIK(FootR, DeltaSeconds, RightFootIKPosition, RightFootRot);

    BodyIKOffset = FVector(0.f, 0.f, FMath::Min(RightFootIKPosition.Z, LeftFootIKPosition.Z));
}

void UAnimInstanceBase::TraceFootIK(FName FootBone, float DeltaSeconds, FVector& OutPos, FRotator& OutRot)
{
    if (FootBone.IsNone()) return;

    const FVector FootLoc = OwnerMesh->GetSocketLocation(FootBone);
    const FVector RootLoc = OwnerMesh->GetSocketLocation(TEXT("root"));

    const FVector Start(FootLoc.X, FootLoc.Y, RootLoc.Z + 50.f);
    const FVector End  (FootLoc.X, FootLoc.Y, RootLoc.Z - 50.f);

    FHitResult Hit;
    FCollisionQueryParams Params(NAME_None, false, GetOwningActor());
    const bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params);

    const FVector  TargetPos = bHit ? FVector(0.f, 0.f, Hit.ImpactPoint.Z - RootLoc.Z) : FVector::ZeroVector;
    const float    Roll      = bHit ? FMath::RadiansToDegrees(FMath::Atan2(Hit.Normal.Y, Hit.Normal.Z)) : 0.f;
    const float    Pitch     = bHit ? FMath::RadiansToDegrees(FMath::Atan2(Hit.Normal.X, Hit.Normal.Z)) * -1.f : 0.f;
    const FRotator TargetRot = bHit ? FRotator(Pitch, 0.f, Roll) : FRotator::ZeroRotator;

    OutPos = FMath::VInterpTo(OutPos, TargetPos, DeltaSeconds, FootInterpSpeed);
    OutRot = FMath::RInterpTo(OutRot, TargetRot, DeltaSeconds, FootInterpSpeed);
}
