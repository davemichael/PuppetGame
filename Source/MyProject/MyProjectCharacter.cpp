// Copyright Epic Games, Inc. All Rights Reserved.

#include "MyProjectCharacter.h"
#include "MyProjectProjectile.h"
#include "Animation/AnimInstance.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/EngineTypes.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"


//////////////////////////////////////////////////////////////////////////
// AMyProjectCharacter

AMyProjectCharacter::AMyProjectCharacter()
{
	// Character doesnt have a rifle at start
	bHasRifle = false;

    // Powers and perks that we need to make "gettable":
	bHasMarionette = true;
	JumpMaxCount = 2;

	bPower1Tracing = false;
	
	// Set size for collision capsule
	GetCapsuleComponent()->InitCapsuleSize(55.f, 96.0f);
		
	// Create a CameraComponent	
	FirstPersonCameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));
	FirstPersonCameraComponent->SetupAttachment(GetCapsuleComponent());
	FirstPersonCameraComponent->SetRelativeLocation(FVector(-10.f, 0.f, 60.f)); // Position the camera
	FirstPersonCameraComponent->bUsePawnControlRotation = true;

	// Create a mesh component that will be used when being viewed from a '1st person' view (when controlling this pawn)
	Mesh1P = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("CharacterMesh1P"));
	Mesh1P->SetOnlyOwnerSee(true);
	Mesh1P->SetupAttachment(FirstPersonCameraComponent);
	Mesh1P->bCastDynamicShadow = false;
	Mesh1P->CastShadow = false;
	//Mesh1P->SetRelativeRotation(FRotator(0.9f, -19.19f, 5.2f));
	Mesh1P->SetRelativeLocation(FVector(-30.f, 0.f, -150.f));

	// Set up power target
	static ConstructorHelpers::FObjectFinder<UBlueprint> powerTargetBlueprint(
		TEXT("Blueprint'/Game/FirstPerson/Blueprints/BP_PowerTarget'"));
	powerTargetBlueprintClass = static_cast<UClass *>(powerTargetBlueprint.Object->GeneratedClass);
}

void AMyProjectCharacter::BeginPlay()
{
	// Call the base class  
	Super::BeginPlay();

	//Add Input Mapping Context
	if (APlayerController* PlayerController = Cast<APlayerController>(Controller))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer()))
		{
			Subsystem->AddMappingContext(DefaultMappingContext, 0);
		}
	}
}

void AMyProjectCharacter::Tick(float DeltaSeconds) {
	if (bPower1Tracing) {
		TracePower();
	}
}

//////////////////////////////////////////////////////////////////////////// Input

void AMyProjectCharacter::SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent)
{
	// Set up action bindings
	if (UEnhancedInputComponent* EnhancedInputComponent = CastChecked<UEnhancedInputComponent>(PlayerInputComponent))
	{
		//Jumping
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Triggered, this, &ACharacter::Jump);
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);

		//Moving
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AMyProjectCharacter::Move);

		//Looking
		EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &AMyProjectCharacter::Look);

		// Powers
		EnhancedInputComponent->BindAction(Power1StartAction, ETriggerEvent::Triggered, this, &AMyProjectCharacter::Power1Start);
		EnhancedInputComponent->BindAction(Power1CompleteAction, ETriggerEvent::Triggered, this, &AMyProjectCharacter::Power1Complete);
	}
}


void AMyProjectCharacter::Move(const FInputActionValue& Value)
{
	// input is a Vector2D
	FVector2D MovementVector = Value.Get<FVector2D>();

	if (Controller != nullptr)
	{
		// add movement 
		AddMovementInput(GetActorForwardVector(), MovementVector.Y);
		AddMovementInput(GetActorRightVector(), MovementVector.X);
	}
}

void AMyProjectCharacter::Look(const FInputActionValue& Value)
{
	// input is a Vector2D
	FVector2D LookAxisVector = Value.Get<FVector2D>();

	if (Controller != nullptr)
	{
		// add yaw and pitch input to controller
		AddControllerYawInput(LookAxisVector.X);
		AddControllerPitchInput(LookAxisVector.Y);
	}
}

void AMyProjectCharacter::Power1Start(const FInputActionValue& Value)
{
    // TODO: Aim.
	// Rough outline:
	// 1. Make an actor to represent destination
	// 
	// 2. On every tick, trace and place the actor(?)
	// 3. On completion, delete the actor
	GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Yellow, TEXT("keydown"));
	SetPower1Tracing(true);
}

void AMyProjectCharacter::Power1Complete(const FInputActionValue& Value)
{
	SetPower1Tracing(false);
	GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Yellow, TEXT("keyup"));
	if (Controller != nullptr)
	{
		if (bHasMarionette) {
			FVector location;
			FRotator rotation;
			GetActorEyesViewPoint(location, rotation);
			FVector TraceEnd = location + rotation.Vector() * 1000.0f;
			const FVector DeltaLocation(1000.0f, 0, 0);
			FHitResult unusedOutSweepHitResult;
			TracePower(/*move_player = */ true);
			powerTarget->Destroy();
			powerTarget = nullptr;
		}
	}
}

void AMyProjectCharacter::SetHasRifle(bool bNewHasRifle)
{
	bHasRifle = bNewHasRifle;
}

bool AMyProjectCharacter::GetHasRifle()
{
	return bHasRifle;
}

void AMyProjectCharacter::SetHasMarionette(bool bNewHasMarionette)
{
	bHasMarionette = bNewHasMarionette;
}

bool AMyProjectCharacter::GetHasMarionette()
{
	return bHasMarionette;
}

void AMyProjectCharacter::SetPower1Tracing(bool bNewPower1Tracing)
{
	bPower1Tracing = bNewPower1Tracing;
}

bool AMyProjectCharacter::GetPower1Tracing()
{
	return bPower1Tracing;
}

void AMyProjectCharacter::TracePower(bool move_player) {
	// FHitResult will hold all data returned by our line collision query
	FHitResult Hit;

	// We set up a line trace from our camera location to a point 1000cm ahead of us
	FVector location;
	FRotator rotation;
	GetActorEyesViewPoint(location, rotation);
	FVector TraceEnd = location + rotation.Vector() * 1000.0f;

    // If the power target hasn't spawned yet, spawn it
	if (!powerTarget) {
		powerTarget = GetWorld()->SpawnActor<AActor>(powerTargetBlueprintClass);
	}

	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(this);

	GetWorld()->LineTraceSingleByChannel(Hit, location, TraceEnd, PowerTraceProperty, QueryParams);
	FHitResult unused;
	FVector powerTargetLocation = TraceEnd;
	if (Hit.bBlockingHit) {
		powerTargetLocation = Hit.ImpactPoint;
	}
	if (move_player) {
		FTransform transform(powerTargetLocation);
		// TODO: Figure out correct Z to account for distance from bottom of pawn to camera height
		transform.AddToTranslation({ 0, 0, 100.0f });
		SetActorTransform(transform, /*bSweep=*/false, &unused, static_cast<ETeleportType>(/*TeleportPhysics*/1));
	} else {
		powerTarget->SetActorTransform(FTransform(powerTargetLocation), /*bSweep=*/false, &unused, static_cast<ETeleportType>(/*TeleportPhysics*/1));
	}
}