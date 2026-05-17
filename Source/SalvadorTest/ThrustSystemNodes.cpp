#include "ThrustSystemNodes.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "Engine/SkeletalMeshSocket.h"
#include "GameFramework/Character.h"

static void SetAnimVec(UAnimInstance* Inst, FName PropName, const FVector& Val)
{
    if (!Inst || PropName == NAME_None) return;
    if (FProperty* P = Inst->GetClass()->FindPropertyByName(PropName))
        if (FStructProperty* SP = CastField<FStructProperty>(P))
            if (SP->Struct == TBaseStructure<FVector>::Get())
                *SP->ContainerPtrToValuePtr<FVector>(Inst) = Val;
}

static void SetAnimRot(UAnimInstance* Inst, FName PropName, const FRotator& Val)
{
    if (!Inst || PropName == NAME_None) return;
    if (FProperty* P = Inst->GetClass()->FindPropertyByName(PropName))
        if (FStructProperty* SP = CastField<FStructProperty>(P))
            if (SP->Struct == TBaseStructure<FRotator>::Get())
                *SP->ContainerPtrToValuePtr<FRotator>(Inst) = Val;
}

static FVector GetAnimVec(UAnimInstance* Inst, FName PropName)
{
    if (!Inst || PropName == NAME_None) return FVector::ZeroVector;
    if (FProperty* P = Inst->GetClass()->FindPropertyByName(PropName))
        if (FStructProperty* SP = CastField<FStructProperty>(P))
            if (SP->Struct == TBaseStructure<FVector>::Get())
                return *SP->ContainerPtrToValuePtr<FVector>(Inst);
    return FVector::ZeroVector;
}

static FRotator GetAnimRot(UAnimInstance* Inst, FName PropName)
{
    if (!Inst || PropName == NAME_None) return FRotator::ZeroRotator;
    if (FProperty* P = Inst->GetClass()->FindPropertyByName(PropName))
        if (FStructProperty* SP = CastField<FStructProperty>(P))
            if (SP->Struct == TBaseStructure<FRotator>::Get())
                return *SP->ContainerPtrToValuePtr<FRotator>(Inst);
    return FRotator::ZeroRotator;
}

void UThrustSystemNodes::ThrustSetUp(
    FThrustState& State,
    AActor* AttackerActor,
    AActor* VictimActor,
    FName DomLocGoal,
    FName DomRotGoal,
    FName SlaveLocGoal,
    FName SlaveRotGoal,
    const TArray<FName>& ContactSockets,
    const TArray<FName>& SkipPlantBones,
    FVector HitLocation,
    FName TargetBoneName,
    float HitReachDelay,
    float PlantDuration,
    UAnimMontage* Montage,
    float MontageCurrentPos,
    float RecoverDuration,
    FName LimitBone,
    float MaxDistFromBone,
    float StabDepth,
    float ArmReachPercent,
    FName HipLocGoal,
    float HipFollowPercent,
    float ArmRecoverDuration,
    float HipRecoverDuration,
    bool bDebug)
{
    if (!AttackerActor || ContactSockets.Num() == 0 || HitReachDelay <= 0.f)
        return;

    USkeletalMeshComponent* AttackerMesh = nullptr;
    if (ACharacter* Char = Cast<ACharacter>(AttackerActor))
        AttackerMesh = Char->GetMesh();
    if (!AttackerMesh)
        AttackerMesh = AttackerActor->FindComponentByClass<USkeletalMeshComponent>();
    if (!AttackerMesh) return;

    UAnimInstance* AnimInst = AttackerMesh->GetAnimInstance();
    if (!AnimInst) return;

    USkeletalMeshComponent* VictimMesh = VictimActor
        ? VictimActor->FindComponentByClass<USkeletalMeshComponent>()
        : nullptr;

    const FVector RawTargetWorld = (VictimMesh && TargetBoneName != NAME_None)
        ? VictimMesh->GetBoneLocation(TargetBoneName)
        : HitLocation;

    FName   ClosestSocket      = ContactSockets[0];
    FVector ClosestSocketWorld = AttackerMesh->GetSocketLocation(ContactSockets[0]);
    float   MinDistSq          = FVector::DistSquared(HitLocation, ClosestSocketWorld);
    for (int32 i = 1; i < ContactSockets.Num(); ++i)
    {
        const FVector SW  = AttackerMesh->GetSocketLocation(ContactSockets[i]);
        const float   DSq = FVector::DistSquared(HitLocation, SW);
        if (DSq < MinDistSq) { MinDistSq = DSq; ClosestSocketWorld = SW; ClosestSocket = ContactSockets[i]; }
    }

    FVector TargetWorld      = RawTargetWorld;
    FVector TargetBoneOffset = FVector::ZeroVector;
    {
        UWorld* TraceWorld = AttackerMesh->GetWorld();
        FHitResult Hit;
        FCollisionQueryParams Params(NAME_None, false, AttackerActor);
        if (TraceWorld && TraceWorld->LineTraceSingleByChannel(
                Hit, ClosestSocketWorld, RawTargetWorld, ECC_GameTraceChannel2, Params))
        {
            const FVector TraceDir = (RawTargetWorld - ClosestSocketWorld).GetSafeNormal();
            TargetWorld      = Hit.ImpactPoint + TraceDir * StabDepth;
            TargetBoneOffset = TargetWorld - RawTargetWorld;
        }
    }

    const USkeletalMeshSocket* Sock      = AttackerMesh->GetSocketByName(ClosestSocket);
    const FName                PivotBone = Sock ? Sock->BoneName : NAME_None;
    const FVector              PivotWorld = (PivotBone != NAME_None)
        ? AttackerMesh->GetBoneLocation(PivotBone)
        : AttackerMesh->GetComponentLocation();

    State.DomRestPos   = GetAnimVec(AnimInst, DomLocGoal);
    State.DomRestRot   = GetAnimRot(AnimInst, DomRotGoal);
    State.SlaveRestPos = GetAnimVec(AnimInst, SlaveLocGoal);
    State.SlaveRestRot = GetAnimRot(AnimInst, SlaveRotGoal);

    {
        const FTransform CompTW_S   = AttackerMesh->GetComponentTransform();
        const FVector HandWorldRest = CompTW_S.TransformPosition(State.DomRestPos);
        const FQuat   HandRotRest   = CompTW_S.GetRotation() * State.DomRestRot.Quaternion();
        State.SocketRelativeLocation = HandRotRest.Inverse().RotateVector(ClosestSocketWorld - HandWorldRest);
    }

    const FVector CurrentDir   = (ClosestSocketWorld - PivotWorld).GetSafeNormal();
    const FVector RawTargetDir = (TargetWorld        - PivotWorld).GetSafeNormal();

    FVector TargetDir = RawTargetDir;
    if (FVector::DotProduct(CurrentDir, RawTargetDir) < 0.f)
    {
        const FVector Tangent = (RawTargetDir - FVector::DotProduct(RawTargetDir, CurrentDir) * CurrentDir).GetSafeNormal();
        TargetDir = Tangent.IsNearlyZero() ? CurrentDir : Tangent;
    }

    const FQuat DeltaWorld    = FQuat::FindBetweenVectors(CurrentDir, TargetDir);
    const FQuat CompWorldQuat = AttackerMesh->GetComponentTransform().GetRotation();
    const FQuat DeltaCS       = CompWorldQuat.Inverse() * DeltaWorld * CompWorldQuat;
    const FQuat StartRotCS    = State.DomRestRot.Quaternion();
    const FQuat TargetRotCS   = DeltaCS * StartRotCS;

    State.AttackerMesh    = AttackerMesh;
    State.VictimActor     = VictimActor;
    State.DomLocGoal      = DomLocGoal;
    State.DomRotGoal      = DomRotGoal;
    State.SlaveLocGoal    = SlaveLocGoal;
    State.SlaveRotGoal    = SlaveRotGoal;
    State.PivotBone       = PivotBone;
    State.ContactSocket   = ClosestSocket;
    State.TargetBone      = TargetBoneName;
    State.TargetBoneWorld  = TargetWorld;
    State.TargetBoneOffset = TargetBoneOffset;
    State.DomStartRotCS   = StartRotCS.Rotator();
    State.DomTargetRotCS  = TargetRotCS.Rotator();
    State.HitReachDelay   = HitReachDelay;
    State.PlantDuration   = PlantDuration;
    State.PlantElapsed    = 0.f;
    State.Montage         = Montage;
    State.MontagePos      = MontageCurrentPos;
    State.RecoverDuration = RecoverDuration;
    State.TotalFrames     = 0;
    State.FramesRemaining = 0;
    State.bActive         = true;
    State.bPlanted        = false;
    State.PlantedRotCS    = FRotator::ZeroRotator;
    State.LimitBone       = LimitBone;
    State.MaxDistFromBone = MaxDistFromBone;
    State.StabDepth         = StabDepth;
    State.SkipPlantBones    = SkipPlantBones;
    State.bSkipPlant        = false;
    State.ArmReachPercent   = ArmReachPercent;
    State.HipLocGoal         = HipLocGoal;
    State.HipFollowPercent   = HipFollowPercent;
    State.HipRestPosCS       = GetAnimVec(AnimInst, HipLocGoal);
    State.ArmRecoverDuration = ArmRecoverDuration > 0.f ? ArmRecoverDuration : RecoverDuration;
    State.HipRecoverDuration = HipRecoverDuration > 0.f ? HipRecoverDuration : RecoverDuration;
    State.bDebug            = bDebug;

    if (bDebug)
    {
        UWorld* World = AttackerMesh->GetWorld();
        if (World)
        {
            for (const FName& SocketName : ContactSockets)
            {
                const FVector SW     = AttackerMesh->GetSocketLocation(SocketName);
                const bool    bClose = SocketName == ClosestSocket;
                DrawDebugSphere(World, SW, 3.f, 8, bClose ? FColor::Cyan : FColor::Red, false, 4.f);
            }
            DrawDebugSphere(World, PivotWorld,     5.f, 8,  FColor::Green, false, 4.f);
            DrawDebugSphere(World, RawTargetWorld, 5.f, 12, FColor::Black, false, 4.f);
            DrawDebugBox   (World, RawTargetWorld, FVector(4.f, 4.f, 50.f), FQuat::Identity, FColor::Black, false, 4.f);
            DrawDebugSphere(World, TargetWorld,    5.f, 12, FColor::White, false, 4.f);
            DrawDebugBox   (World, TargetWorld,    FVector(4.f, 4.f, 50.f), FQuat::Identity, FColor::White, false, 4.f);
            DrawDebugLine(World, ClosestSocketWorld, RawTargetWorld, FColor::Orange, false, 4.f, 0, 0.5f);
            DrawDebugLine(World, ClosestSocketWorld, TargetWorld,    FColor::Yellow, false, 4.f, 0, 1.5f);
            DrawDebugLine(World, PivotWorld, PivotWorld + CurrentDir * 40.f, FColor::Red,   false, 4.f, 0, 0.5f);
            DrawDebugLine(World, PivotWorld, PivotWorld + TargetDir  * 40.f, FColor::Green, false, 4.f, 0, 0.5f);
        }
    }
}

void UThrustSystemNodes::ThrustTick(
    FThrustState& State,
    float DeltaTime,
    bool& bOutComplete)
{
    bOutComplete = false;

    if (!State.bActive || !State.AttackerMesh) return;

    UAnimInstance* AnimInst = State.AttackerMesh->GetAnimInstance();
    if (!AnimInst) return;

    if (State.TotalFrames == 0)
    {
        State.TotalFrames     = FMath::Max(1, FMath::RoundToInt(State.HitReachDelay / DeltaTime));
        State.FramesRemaining = State.TotalFrames;
    }

    const int32 CurrentFrame = State.TotalFrames - State.FramesRemaining + 1;
    const float Alpha        = FMath::Clamp(float(CurrentFrame) / float(State.TotalFrames), 0.f, 1.f);

    const FQuat    StartQ    = State.DomStartRotCS.Quaternion();
    const FQuat    TargetQ   = State.DomTargetRotCS.Quaternion();
    const FQuat    LerpedCSQ = FQuat::Slerp(StartQ, TargetQ, Alpha);
    const FRotator LerpedRot = LerpedCSQ.Rotator();

    FVector DomGoalCS   = State.DomRestPos;
    FVector SlaveGoalCS = State.SlaveRestPos;

    if (State.ArmReachPercent > 0.f)
    {
        const FTransform CompTW      = State.AttackerMesh->GetComponentTransform();
        const FVector    TargetCS    = CompTW.InverseTransformPosition(State.TargetBoneWorld);
        // Offset the hand target so the contact socket tip lands on TargetCS, not the hand.
        const FVector    HandTargetCS = TargetCS - LerpedCSQ.RotateVector(State.SocketRelativeLocation);
        const FVector    ArmDelta     = (HandTargetCS - State.DomRestPos) * (State.ArmReachPercent * Alpha);
        DomGoalCS   = State.DomRestPos   + ArmDelta;
        SlaveGoalCS = State.SlaveRestPos + ArmDelta;
    }

    SetAnimVec(AnimInst, State.DomLocGoal,   DomGoalCS);
    SetAnimRot(AnimInst, State.DomRotGoal,   LerpedRot);
    SetAnimVec(AnimInst, State.SlaveLocGoal, SlaveGoalCS);
    SetAnimRot(AnimInst, State.SlaveRotGoal, State.SlaveRestRot);

    --State.FramesRemaining;
    if (State.FramesRemaining <= 0)
    {
        State.FramesRemaining = 0;
        bOutComplete          = true;
        State.bActive         = false;
        State.bPlanted        = true;
        State.PlantedRotCS    = State.DomTargetRotCS;
        State.bSkipPlant      = State.SkipPlantBones.Contains(State.TargetBone);
    }
}

void UThrustSystemNodes::ThrustPlant(
    FThrustState& State,
    float DeltaTime,
    bool& bOutBlacklisted,
    bool& bOutComplete)
{
    bOutComplete    = false;
    bOutBlacklisted = State.SkipPlantBones.Contains(State.TargetBone);
    if (!State.AttackerMesh) return;

    if (State.SkipPlantBones.Contains(State.TargetBone))
    {
        bOutComplete = true;
        return;
    }

    UAnimInstance* AnimInst = State.AttackerMesh->GetAnimInstance();
    if (!AnimInst) return;

    const FTransform CompTW = State.AttackerMesh->GetComponentTransform();
    const FQuat      CompQ  = CompTW.GetRotation();

    USkeletalMeshComponent* VictimMesh = State.VictimActor
        ? State.VictimActor->FindComponentByClass<USkeletalMeshComponent>()
        : nullptr;

    if (State.FramesRemaining == 0)
    {
        State.PlantedTargetBoneWorld = (VictimMesh && State.TargetBone != NAME_None)
            ? VictimMesh->GetBoneLocation(State.TargetBone) + State.TargetBoneOffset
            : State.TargetBoneWorld;
        State.PlantedHandWorldPos = CompTW.TransformPosition(GetAnimVec(AnimInst, State.DomLocGoal));

        const FVector DomHandWorld = CompTW.TransformPosition(State.DomRestPos);
        const FQuat   DomRotWorld  = CompQ * State.PlantedRotCS.Quaternion();
        const FTransform DomHandTW(DomRotWorld, DomHandWorld);

        const FVector SlaveHandWorld = CompTW.TransformPosition(GetAnimVec(AnimInst, State.SlaveLocGoal));
        const FQuat   SlaveRotWorld  = CompQ * GetAnimRot(AnimInst, State.SlaveRotGoal).Quaternion();

        State.SlaveInDomHandOffsetLocal = DomHandTW.InverseTransformPosition(SlaveHandWorld);
        State.SlaveRotInDomHandLocal    = (DomRotWorld.Inverse() * SlaveRotWorld).Rotator();

        State.FramesRemaining = -1;
    }

    const FVector CurrentTargetWorld = (VictimMesh && State.TargetBone != NAME_None)
        ? VictimMesh->GetBoneLocation(State.TargetBone) + State.TargetBoneOffset
        : State.TargetBoneWorld;

    const FVector WoundDelta    = CurrentTargetWorld - State.PlantedTargetBoneWorld;
    const FVector DesiredHandCS = CompTW.InverseTransformPosition(State.PlantedHandWorldPos + WoundDelta);

    if (State.MaxDistFromBone > 0.f && State.LimitBone != NAME_None)
    {
        const FVector LimitBoneWorld = State.AttackerMesh->GetBoneLocation(State.LimitBone);
        if (FVector::Dist(CurrentTargetWorld, LimitBoneWorld) > State.MaxDistFromBone)
            bOutComplete = true;
    }

    if (State.bDebug && State.LimitBone != NAME_None && GEngine)
    {
        const FVector LimitBoneWorld = State.AttackerMesh->GetBoneLocation(State.LimitBone);
        const float   Dist           = FVector::Dist(CurrentTargetWorld, LimitBoneWorld);
        GEngine->AddOnScreenDebugMessage(-1, 0.f, FColor::Green,
            FString::Printf(TEXT("ThrustPlant | dist to %s: %.1f cm"), *State.LimitBone.ToString(), Dist));
    }

    if (State.HipLocGoal != NAME_None && State.HipFollowPercent > 0.f)
    {
        const FVector HandDelta = DesiredHandCS - State.DomRestPos;
        const FVector HipGoalCS = State.HipRestPosCS + HandDelta * State.HipFollowPercent;
        SetAnimVec(AnimInst, State.HipLocGoal, HipGoalCS);

        if (State.bDebug && GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 0.f, FColor::Magenta,
                FString::Printf(TEXT("HipFollow | HandDelta=(%.1f,%.1f,%.1f) HipGoal=(%.1f,%.1f,%.1f)"),
                    HandDelta.X, HandDelta.Y, HandDelta.Z,
                    HipGoalCS.X, HipGoalCS.Y, HipGoalCS.Z));
        }
    }
    else if (State.bDebug && GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 0.f, FColor::Orange,
            FString::Printf(TEXT("HipFollow | INACTIVE — Goal='%s' Percent=%.2f"),
                *State.HipLocGoal.ToString(), State.HipFollowPercent));
    }

    SetAnimVec(AnimInst, State.DomLocGoal, DesiredHandCS);
    SetAnimRot(AnimInst, State.DomRotGoal, State.PlantedRotCS);

    const FVector DomHandWorld   = CompTW.TransformPosition(DesiredHandCS);
    const FQuat   DomRotWorld    = CompQ * State.PlantedRotCS.Quaternion();
    const FTransform DomHandTW(DomRotWorld, DomHandWorld);

    const FVector SlaveHandWorld = DomHandTW.TransformPosition(State.SlaveInDomHandOffsetLocal);
    const FQuat   SlaveRotWorld  = DomRotWorld * State.SlaveRotInDomHandLocal.Quaternion();

    SetAnimVec(AnimInst, State.SlaveLocGoal, CompTW.InverseTransformPosition(SlaveHandWorld));
    SetAnimRot(AnimInst, State.SlaveRotGoal, (CompQ.Inverse() * SlaveRotWorld).Rotator());

    if (State.PlantDuration > 0.f)
    {
        State.PlantElapsed += DeltaTime;
        if (State.PlantElapsed >= State.PlantDuration)
            bOutComplete = true;
    }
}

void UThrustSystemNodes::ThrustRecover(
    FThrustState& State,
    float DeltaTime,
    bool& bOutComplete)
{
    bOutComplete = false;

    if (!State.AttackerMesh) return;

    UAnimInstance* AnimInst = State.AttackerMesh->GetAnimInstance();
    if (!AnimInst) return;

    if (!State.bRecovering)
    {
        State.bRecovering            = true;
        State.RecoverDomStartPosCS   = GetAnimVec(AnimInst, State.DomLocGoal);
        State.RecoverDomStartRotCS   = GetAnimRot(AnimInst, State.DomRotGoal);
        State.RecoverSlaveStartPosCS = GetAnimVec(AnimInst, State.SlaveLocGoal);
        State.RecoverSlaveStartRotCS = GetAnimRot(AnimInst, State.SlaveRotGoal);
        State.RecoverArmElapsed    = 0.f;
        State.RecoverHipStartPosCS = GetAnimVec(AnimInst, State.HipLocGoal);
        State.RecoverHipElapsed    = 0.f;

        if (State.Montage)
        {
            const float StartPos = State.MontagePos > KINDA_SMALL_NUMBER
                ? State.MontagePos
                : State.Montage->GetPlayLength();

            if (StartPos > KINDA_SMALL_NUMBER)
            {
                State.MontagePos         = StartPos;
                State.bMontageReversing  = true;
                State.ArmRecoverDuration = FMath::Min(State.ArmRecoverDuration, StartPos);
                State.HipRecoverDuration = FMath::Min(State.HipRecoverDuration, StartPos);
                AnimInst->Montage_Play(State.Montage, -1.f,
                    EMontagePlayReturnType::MontageLength, StartPos, true);
            }
        }

        if (!State.bMontageReversing)
        {
            State.RecoverFramesTotal     = FMath::Max(1, FMath::RoundToInt(State.RecoverDuration / DeltaTime));
            State.RecoverFramesRemaining = State.RecoverFramesTotal;
        }

    }

    bool bDone = false;

    if (State.bMontageReversing)
    {
        const float CurPos = AnimInst->Montage_GetPosition(State.Montage);
        if (CurPos <= KINDA_SMALL_NUMBER)
        {
            AnimInst->Montage_SetPlayRate(State.Montage, 0.f);
            bDone = true;
        }
    }
    else
    {
        --State.RecoverFramesRemaining;
        if (State.RecoverFramesRemaining <= 0)
            bDone = true;
    }

    {
        State.RecoverArmElapsed += DeltaTime;
        const float ArmAlpha = FMath::Clamp(State.RecoverArmElapsed / State.ArmRecoverDuration, 0.f, 1.f);
        SetAnimVec(AnimInst, State.DomLocGoal,
            FMath::Lerp(State.RecoverDomStartPosCS,   State.DomRestPos,   ArmAlpha));
        SetAnimRot(AnimInst, State.DomRotGoal,
            FQuat::Slerp(State.RecoverDomStartRotCS.Quaternion(),   State.DomRestRot.Quaternion(),   ArmAlpha).Rotator());
        SetAnimVec(AnimInst, State.SlaveLocGoal,
            FMath::Lerp(State.RecoverSlaveStartPosCS, State.SlaveRestPos, ArmAlpha));
        SetAnimRot(AnimInst, State.SlaveRotGoal,
            FQuat::Slerp(State.RecoverSlaveStartRotCS.Quaternion(), State.SlaveRestRot.Quaternion(), ArmAlpha).Rotator());
    }
    if (State.HipLocGoal != NAME_None)
    {
        State.RecoverHipElapsed += DeltaTime;
        const float   HipAlpha   = FMath::Clamp(State.RecoverHipElapsed / State.HipRecoverDuration, 0.f, 1.f);
        const FVector HipCurrent = FMath::Lerp(State.RecoverHipStartPosCS, State.HipRestPosCS, HipAlpha);
        SetAnimVec(AnimInst, State.HipLocGoal, HipCurrent);

        if (State.bDebug)
        {
            if (GEngine)
                GEngine->AddOnScreenDebugMessage(-1, 0.f, FColor::Cyan,
                    FString::Printf(TEXT("HipRecover | Alpha=%.3f | Cur=(%.1f,%.1f,%.1f) | Target=(%.1f,%.1f,%.1f)"),
                        HipAlpha,
                        HipCurrent.X, HipCurrent.Y, HipCurrent.Z,
                        State.HipRestPosCS.X, State.HipRestPosCS.Y, State.HipRestPosCS.Z));

        }
    }

    if (bDone)
    {
        SetAnimVec(AnimInst, State.DomLocGoal,   State.DomRestPos);
        SetAnimRot(AnimInst, State.DomRotGoal,   State.DomRestRot);
        SetAnimVec(AnimInst, State.SlaveLocGoal, State.SlaveRestPos);
        SetAnimRot(AnimInst, State.SlaveRotGoal, State.SlaveRestRot);
        if (State.HipLocGoal != NAME_None)
            SetAnimVec(AnimInst, State.HipLocGoal, State.HipRestPosCS);
        bOutComplete = true;
    }
}

void UThrustSystemNodes::ThrustEnd(FThrustState& State)
{
    if (!State.AttackerMesh) return;

    UAnimInstance* AnimInst = State.AttackerMesh->GetAnimInstance();
    if (AnimInst)
    {
        SetAnimVec(AnimInst, State.DomLocGoal,   State.DomRestPos);
        SetAnimRot(AnimInst, State.DomRotGoal,   State.DomRestRot);
        SetAnimVec(AnimInst, State.SlaveLocGoal, State.SlaveRestPos);
        SetAnimRot(AnimInst, State.SlaveRotGoal, State.SlaveRestRot);
        if (State.HipLocGoal != NAME_None)
            SetAnimVec(AnimInst, State.HipLocGoal, State.HipRestPosCS);
    }

    State = FThrustState();
}
