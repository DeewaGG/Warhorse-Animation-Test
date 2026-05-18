#include "PlayableCharacter.h"
#include "AnimInstanceBase.h"
#include "TargetingSystemComponent.h"
#include "TargetComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"

APlayableCharacter::APlayableCharacter()
{
    PrimaryActorTick.bCanEverTick = true;

    // ── Character movement ────────────────────────────────────────────────────
    bUseControllerRotationYaw = true;

    UCharacterMovementComponent* Move = GetCharacterMovement();
    Move->BrakingFrictionFactor            = 1.f;
    Move->bUseSeparateBrakingFriction      = true;
    Move->MaxWalkSpeed                     = 400.f;
    Move->MinAnalogWalkSpeed               = 20.f;
    Move->BrakingDecelerationWalking       = 2000.f;
    Move->bOrientRotationToMovement        = false;
    Move->PerchRadiusThreshold             = 15.f;

    // ── Camera boom ───────────────────────────────────────────────────────────
    CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
    CameraBoom->SetupAttachment(RootComponent);
    CameraBoom->SetRelativeLocation(FVector(0.f, 40.f, 80.f));
    CameraBoom->TargetArmLength            = 90.f;
    CameraBoom->bUsePawnControlRotation    = true;

    // ── Follow camera ─────────────────────────────────────────────────────────
    FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
    FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
    FollowCamera->SetRelativeLocation(FVector(10.f, 0.f, 0.f));
    FollowCamera->bUsePawnControlRotation  = false;

    // ── Targeting system ──────────────────────────────────────────────────────
    TargetingSystem = CreateDefaultSubobject<UTargetingSystemComponent>(TEXT("TargetingSystem"));
}

void APlayableCharacter::BeginPlay()
{
    Super::BeginPlay();

    if (APlayerController* PC = Cast<APlayerController>(GetController()))
    {
        if (UEnhancedInputLocalPlayerSubsystem* Sub =
            ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
        {
            if (DefaultMappingContext)
                Sub->AddMappingContext(DefaultMappingContext, 0);
        }
    }

    ABP = Cast<UAnimInstanceBase>(GetMesh()->GetAnimInstance());

    AnimVars_BeginPlay();
}

void APlayableCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);

    UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PlayerInputComponent);
    if (!EIC) return;

    if (IA_Move)
    {
        EIC->BindAction(IA_Move, ETriggerEvent::Triggered, this, &APlayableCharacter::OnMove);
    }
    if (IA_MouseLook)
    {
        EIC->BindAction(IA_MouseLook, ETriggerEvent::Triggered, this, &APlayableCharacter::OnMouseLook);
        EIC->BindAction(IA_MouseLook, ETriggerEvent::Completed, this, &APlayableCharacter::OnMouseLookCompleted);
    }
    if (IA_Attack)
    {
        EIC->BindAction(IA_Attack, ETriggerEvent::Started, this, &APlayableCharacter::OnAttackStarted);
    }
    if (IA_Aim)
    {
        EIC->BindAction(IA_Aim, ETriggerEvent::Started,   this, &APlayableCharacter::OnAimStarted);
        EIC->BindAction(IA_Aim, ETriggerEvent::Canceled,  this, &APlayableCharacter::OnAimCanceled);
        EIC->BindAction(IA_Aim, ETriggerEvent::Completed, this, &APlayableCharacter::OnAimCompleted);
    }
}

void APlayableCharacter::OnMove(const FInputActionValue& Value)
{
    const FVector2D Axes = Value.Get<FVector2D>();
    if (Axes.IsZero()) return;

    const FRotator ControlRot    = GetControlRotation();
    const FRotator YawOnlyRot    = FRotator(0.f, ControlRot.Yaw, 0.f);

    AddMovementInput(FRotationMatrix(ControlRot).GetScaledAxis(EAxis::Y), Axes.X);
    AddMovementInput(FRotationMatrix(YawOnlyRot).GetScaledAxis(EAxis::X), Axes.Y);
}

void APlayableCharacter::OnMouseLook(const FInputActionValue& Value)
{
    const FVector2D Axes = Value.Get<FVector2D>();

    AddControllerYawInput(Axes.X);
    AddControllerPitchInput(Axes.Y);
    TurnValuesUpdate(Axes.X * 1.15);
    CamForwardUpdate();
}

void APlayableCharacter::OnMouseLookCompleted(const FInputActionValue& Value)
{
    Turning_R     = false;
    Turning_L     = false;
    Turning_Speed = 0.0;
}

void APlayableCharacter::TurnValuesUpdate(double Axis)
{
    const double DeltaTime = GetWorld()->GetDeltaSeconds();
    Turning_Speed = FMath::FInterpTo((float)Turning_Speed, (float)FMath::Abs(Axis), (float)DeltaTime, 10.f);
    if (Axis > 0.0) Turning_R = true;
    else            Turning_L = true;
}

void APlayableCharacter::CamForwardUpdate()
{
    Cam_Forward = FollowCamera->GetForwardVector() * 10000.f;
}

void APlayableCharacter::AnimVars_BeginPlay()
{
    Cam_Forward = FollowCamera->GetForwardVector();
}

void APlayableCharacter::OnAttackStarted(const FInputActionValue& Value)
{
    UTargetComponent* CurrentTarget = TargetingSystem->GetCurrentTarget();
    if (!CurrentTarget)
    {
        UE_LOG(LogTemp, Warning, TEXT("PlayableCharacter: OnAttackStarted — no current target"));
        return;
    }

    TargetPos = CurrentTarget->GetComponentLocation();
    TargetingSystem->OnAttackStart();

    const float Duration = PlayAnimMontage(AttackMontage);
    if (Duration > 0.f)
        GetWorldTimerManager().SetTimer(AttackTimerHandle, this, &APlayableCharacter::FinishAttack, Duration, false);
    else
        FinishAttack();
}

void APlayableCharacter::FinishAttack()
{
    TargetingSystem->OnAttackEnd();
}

void APlayableCharacter::OnAimStarted(const FInputActionValue& Value)
{
    TargetingSystem->BeginAiming();
}

void APlayableCharacter::OnAimCanceled(const FInputActionValue& Value)
{
    TargetingSystem->EndAiming();
}

void APlayableCharacter::OnAimCompleted(const FInputActionValue& Value)
{
    TargetingSystem->EndAiming();
}
