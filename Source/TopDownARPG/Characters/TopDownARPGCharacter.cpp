// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.
#include "InventoryComponent.h"
#include "TopDownARPGCharacter.h"
#include "UObject/ConstructorHelpers.h"
#include "Camera/CameraComponent.h"
#include "Components/DecalComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "HeadMountedDisplayFunctionLibrary.h"
#include "Materials/Material.h"
#include "Engine/World.h"
#include "TopDownARPG.h"
#include "GameModes/TopDownARPGGameMode.h"

ATopDownARPGCharacter::ATopDownARPGCharacter()
{
	// Set size for player capsule
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);

	// Don't rotate character to camera direction
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	// Configure character movement
	GetCharacterMovement()->bOrientRotationToMovement = true; // Rotate character to moving direction
	GetCharacterMovement()->RotationRate = FRotator(0.f, 640.f, 0.f);
	GetCharacterMovement()->bConstrainToPlane = true;
	GetCharacterMovement()->bSnapToPlaneAtStart = true;

	// Create a camera boom...
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->bAbsoluteRotation = true; // Don't want arm to rotate when character does
	CameraBoom->TargetArmLength = 800.f;
	CameraBoom->RelativeRotation = FRotator(-60.f, 0.f, 0.f);
	CameraBoom->bDoCollisionTest = false; // Don't want to pull camera in when it collides with level

	// Create a camera...
	TopDownCameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("TopDownCamera"));
	TopDownCameraComponent->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	TopDownCameraComponent->bUsePawnControlRotation = false; // Camera does not rotate relative to arm

	Inventory = CreateDefaultSubobject<UInventoryComponent>(TEXT("Inventory"));

	//OverlapComponent = CreateDefaultSubobject<USphereOfCollision>(TEXT("SphereComp"));
	//OverlapComponent= CreateDefaultSubobject<USphereComponent>(TEXT("SphereComp"));
	//OverlapComponent->OnComponentBeginOverlap.AddUniqueDynamic(this, &ATopDownARPGCharacter::Overlapping);


	// Create a decal in the world to show the cursor's location
	CursorToWorld = CreateDefaultSubobject<UDecalComponent>("CursorToWorld");
	CursorToWorld->SetupAttachment(RootComponent);
	static ConstructorHelpers::FObjectFinder<UMaterial> DecalMaterialAsset(TEXT("Material'/Game/TopDownCPP/Blueprints/M_Cursor_Decal.M_Cursor_Decal'"));
	if (DecalMaterialAsset.Succeeded())
	{
		CursorToWorld->SetDecalMaterial(DecalMaterialAsset.Object);
	}
	CursorToWorld->DecalSize = FVector(16.0f, 32.0f, 32.0f);
	CursorToWorld->SetRelativeRotation(FRotator(90.0f, 0.0f, 0.0f).Quaternion());

	// Activate ticking in order to update the cursor every frame.
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	OnTakeAnyDamage.AddDynamic(this, &ATopDownARPGCharacter::TakeAnyDamage);
}

void ATopDownARPGCharacter::BeginPlay()
{
	Super::BeginPlay();
	FTopDownARPGCharacterStruct* CharacterStruct = CharacterStatsRow.GetRow<FTopDownARPGCharacterStruct>(TEXT(""));

	if (CharacterStruct == nullptr)
	{
		UE_LOG(LogTopDownARPG, Error, TEXT("ATopDownARPGCharacter::BeginPlay CharacterStruct != nullptr"));
		return;
	}

	Health = CharacterStruct->MaximumHealth;

	for (const TSubclassOf<UAbility>Template : CharacterStruct->AbilityTemplates)
	{
		AbilityInstances.Add(NewObject<UAbility>(this, Template));
	}
}

void ATopDownARPGCharacter::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

	if (CursorToWorld != nullptr)
	{
		if (APlayerController* PC = Cast<APlayerController>(GetController()))
		{
			FHitResult TraceHitResult;
			PC->GetHitResultUnderCursor(ECC_Visibility, true, TraceHitResult);
			FVector CursorFV = TraceHitResult.ImpactNormal;
			FRotator CursorR = CursorFV.Rotation();
			CursorToWorld->SetWorldLocation(TraceHitResult.Location);
			CursorToWorld->SetWorldRotation(CursorR);
		}
	}
}

void ATopDownARPGCharacter::TakeAnyDamage(AActor* DamagedActor, float Damage, const class UDamageType* DamageType, class AController* InstigateBy, AActor* DamageCauser)
{
	UE_LOG(LogTopDownARPG, Display, TEXT("ATopDownARPGCharacter::TakeAnyDamage current health = %f"), (Health - Damage));
	Health -= Damage;
	if (Health <= 0.0f)
	{
		Death();
	}
}

void ATopDownARPGCharacter::Death()
{
	UCharacterMovementComponent* Movement = GetCharacterMovement();
	Movement->MaxWalkSpeed = 0.0f;
	Movement->bOrientRotationToMovement = false;


	ATopDownARPGGameMode* GameMode = Cast<ATopDownARPGGameMode>(GetWorld()->GetAuthGameMode());
	if (IsValid(GameMode))
	{
		GameMode->EndGame(false);
	}
}

bool ATopDownARPGCharacter::ActivateInteract()
{
	UWorld* World = GetWorld();
	if (IsValid(World) == false)
	{
		UE_LOG(LogTopDownARPG, Error, TEXT("UBoltAbility::Activate IsValid(World) == false"));
	}

	AActor* Ownerr = Cast<AActor>(GetOuter());
	if (IsValid(Ownerr) == false)
	{
		UE_LOG(LogTopDownARPG, Error, TEXT("UAbility::Activate IsValid(Owner) == false"));
		return false;
	}
	

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Owner = Ownerr;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	FVector Direction;
	Direction.Z = 0.0f;
	Direction.X = 0.0f;
	Direction.Y = 0.0f;
	Direction.Normalize();

	FVector SpawnLocation = Ownerr->GetActorLocation() + Direction * 100.0f;
	AActor* Sphere = World->SpawnActor<AActor>(SC, SpawnLocation,	Direction.Rotation(), SpawnParameters);
	if (IsValid(Sphere) == false)
	{
		UE_LOG(LogTopDownARPG, Error, TEXT("UBoltAbility::Activate IsValid(Projectile) == false"));
		return false;
	}
	return true;
}

