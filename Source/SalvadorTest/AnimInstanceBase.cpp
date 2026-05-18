#include "AnimInstanceBase.h"
#include "PlayableCharacter.h"
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

    if (!MovementComponent)
    {
        GetMovComp();
        if (!MovementComponent) return;
    }

    VelocityAndSpeed();
    CalculateDirections();
    ShouldMove();
    IsFalling();
    SyncPlayableCharacterData();
    ComputeTargetAlpha();
    ComputeHandHeightIK();
    BodyIK(DeltaSeconds);
}

void UAnimInstanceBase::GetMovComp()
{
    ACharacter* Character = Cast<ACharacter>(GetOwningActor());
    if (!Character) return;

    MovementComponent        = Character->GetCharacterMovement();
    OwnerMesh                = Character->GetMesh();
    OwningPlayableCharacter  = Cast<APlayableCharacter>(Character);
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

void UAnimInstanceBase::ComputeTargetAlpha()
{
    if (GetCurveValue(TEXT("LerpAttack")) <= 0.f || TargetPos.IsZero())
    {
        TargetAlpha = 0.f;
        return;
    }

    APawn* Pawn = TryGetPawnOwner();
    if (!Pawn)
    {
        TargetAlpha = 0.f;
        return;
    }

    const FVector ToTarget = (TargetPos - Pawn->GetActorLocation()).GetSafeNormal();
    const float   Dot      = FVector::DotProduct(CamForward.GetSafeNormal(), ToTarget);

    TargetAlpha = FMath::Clamp((Dot - TargetOriMinInfluence) / (1.f - TargetOriMinInfluence), 0.f, 1.f);
}

void UAnimInstanceBase::SyncPlayableCharacterData()
{
    if (!OwningPlayableCharacter) return;
    TurningSpeed = (float)OwningPlayableCharacter->Turning_Speed;
    bTurningR    = OwningPlayableCharacter->Turning_R;
    bTurningL    = OwningPlayableCharacter->Turning_L;
    CamForward   = OwningPlayableCharacter->Cam_Forward;
    TargetPos    = OwningPlayableCharacter->TargetPos;
    HandIKOffset = OwningPlayableCharacter->HandIKOffset;
}

void UAnimInstanceBase::BodyIK(float DeltaSeconds)
{
    if (bIsFalling)
    {
        LeftFootIKPosition  = FVector::ZeroVector;
        LeftFootRot         = FRotator::ZeroRotator;
        LeftFootIKAlpha     = 0.f;
        RightFootIKPosition = FVector::ZeroVector;
        RightFootRot        = FRotator::ZeroRotator;
        RightFootIKAlpha    = 0.f;
        LeftHandIKPosition  = FVector::ZeroVector;
        RightHandIKPosition = FVector::ZeroVector;
        BodyIKOffset        = FVector::ZeroVector;
        return;
    }

    if (!OwnerMesh) return;

    TraceFootIK(FootL, DeltaSeconds, LeftFootIKPosition,  LeftFootRot,  LeftFootIKAlpha);
    TraceFootIK(FootR, DeltaSeconds, RightFootIKPosition, RightFootRot, RightFootIKAlpha);

    BodyIKOffset = FVector(0.f, 0.f, FMath::Min(RightFootIKPosition.Z, LeftFootIKPosition.Z));
}

void UAnimInstanceBase::ComputeHandHeightIK()
{
    const float LerpAttack = GetCurveValue(TEXT("LerpAttack"));
    if (LerpAttack <= 0.f || TargetPos.IsZero())
    {
        HandHeightAdditiveOffset = FVector::ZeroVector;
        return;
    }

    APawn* Pawn = TryGetPawnOwner();
    if (!Pawn)
    {
        HandHeightAdditiveOffset = FVector::ZeroVector;
        return;
    }

    const float HeightDelta = TargetPos.Z - Pawn->GetActorLocation().Z;
    HandHeightAdditiveOffset = (HandIKOffset + FVector(0.f, 0.f, HeightDelta)) * LerpAttack;
}

void UAnimInstanceBase::TraceFootIK(FName FootBone, float DeltaSeconds, FVector& OutPos, FRotator& OutRot, float& OutAlpha)
{
    if (FootBone.IsNone()) return;

    const FVector FootLoc = OwnerMesh->GetSocketLocation(FootBone);
    const FVector RootLoc = OwnerMesh->GetSocketLocation(TEXT("root"));

    const FVector Start(FootLoc.X, FootLoc.Y, RootLoc.Z + 50.f);
    const FVector End  (FootLoc.X, FootLoc.Y, RootLoc.Z - 50.f);

    UWorld* World = GetWorld();
    if (!World) return;

    FHitResult Hit;
    FCollisionQueryParams Params(NAME_None, false, GetOwningActor());
    const bool bFootHit = World->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params);

    const FVector  FootTargetPos = bFootHit ? FVector(0.f, 0.f, Hit.ImpactPoint.Z - RootLoc.Z) : FVector::ZeroVector;
    const float    Roll          = bFootHit ? FMath::RadiansToDegrees(FMath::Atan2(Hit.Normal.Y, Hit.Normal.Z)) : 0.f;
    const float    Pitch         = bFootHit ? FMath::RadiansToDegrees(FMath::Atan2(Hit.Normal.X, Hit.Normal.Z)) * -1.f : 0.f;
    const FRotator TargetRot     = bFootHit ? FRotator(Pitch, 0.f, Roll) : FRotator::ZeroRotator;

    OutPos   = FMath::VInterpTo(OutPos, FootTargetPos, DeltaSeconds, FootInterpSpeed);
    OutRot   = FMath::RInterpTo(OutRot, TargetRot,    DeltaSeconds, FootInterpSpeed);
    OutAlpha = FMath::FInterpTo(OutAlpha, bFootHit ? 1.f : 0.f, DeltaSeconds, FootInterpSpeed);
}
