// Copyright Epic Games, Inc. All Rights Reserved.

#include "Character/Mover/AC_CharacterBase.h"

#include "AbilitySystem/LyraAbilitySystemComponent.h"
#include "Animation/AnimInstance.h"
#include "Camera/LyraCameraComponent.h"
#include "Character/LyraHeroComponent.h"
#include "Character/LyraHealthComponent.h"
#include "Character/LyraPawnExtensionComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Containers/Ticker.h"
#include "Cosmetics/LyraPawnComponent_CharacterParts.h"
#include "DefaultMovementSet/CharacterMoverComponent.h"
#include "Engine/CollisionProfile.h"
#include "Equipment/LyraEquipmentManagerComponent.h"
#include "Equipment/LyraEquipmentInstance.h"
#include "Equipment/LyraQuickBarComponent.h"
#include "GameFramework/Controller.h"
#include "LyraGameplayTags.h"
#include "Inventory/LyraInventoryItemDefinition.h"
#include "Inventory/LyraInventoryItemInstance.h"
#include "Inventory/LyraInventoryManagerComponent.h"
#include "Weapons/LyraRangedWeaponInstance.h"
#include "MoverDataModelTypes.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "Player/LyraPlayerController.h"
#include "Player/LyraPlayerState.h"
#include "UObject/ConstructorHelpers.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AC_CharacterBase)

AAC_CharacterBase::AAC_CharacterBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = true;
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;
	SetReplicatingMovement(false);

	CapsuleComponent = CreateDefaultSubobject<UCapsuleComponent>(TEXT("CapsuleComponent"));
	CapsuleComponent->InitCapsuleSize(42.0f, 96.0f);
	CapsuleComponent->SetCollisionProfileName(UCollisionProfile::Pawn_ProfileName);
	CapsuleComponent->SetCanEverAffectNavigation(true);
	SetRootComponent(CapsuleComponent);

	MeshComponent = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("MeshComponent"));
	MeshComponent->SetupAttachment(CapsuleComponent);
	MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	MeshComponent->SetRelativeLocation(FVector(0.0f, 0.0f, -96.0f));
	MeshComponent->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));

	static ConstructorHelpers::FClassFinder<UAnimInstance> MannequinAnimBP(
		TEXT("/Game/Characters/Heroes/Mannequin/Animations/ABP_Mannequin_Base"));
	if (MannequinAnimBP.Succeeded() && MeshComponent)
	{
		MeshComponent->SetAnimInstanceClass(MannequinAnimBP.Class);
	}

	MoverComponent = CreateDefaultSubobject<UCharacterMoverComponent>(TEXT("MoverComponent"));
	MoverComponent->SetUpdatedComponent(CapsuleComponent);

	CameraComponent = CreateDefaultSubobject<ULyraCameraComponent>(TEXT("CameraComponent"));
	CameraComponent->SetupAttachment(CapsuleComponent);
	// 카메라 거리와 오프셋은 CM_ThirdPerson 의 오프셋 커브가 결정한다.
	// 여기서는 피벗만 캡슐 중심에 둔다.
	CameraComponent->SetRelativeLocation(FVector::ZeroVector);

	PawnExtComponent = CreateDefaultSubobject<ULyraPawnExtensionComponent>(TEXT("PawnExtensionComponent"));
	PawnExtComponent->OnAbilitySystemInitialized_RegisterAndCall(
		FSimpleMulticastDelegate::FDelegate::CreateUObject(this, &ThisClass::OnAbilitySystemInitialized));
	PawnExtComponent->OnAbilitySystemUninitialized_Register(
		FSimpleMulticastDelegate::FDelegate::CreateUObject(this, &ThisClass::OnAbilitySystemUninitialized));
	HeroComponent = CreateDefaultSubobject<ULyraHeroComponent>(TEXT("HeroComponent"));
	CharacterPartsComponent = CreateDefaultSubobject<ULyraPawnComponent_CharacterParts>(TEXT("CharacterPartsComponent"));
	EquipmentManagerComponent = CreateDefaultSubobject<ULyraEquipmentManagerComponent>(TEXT("EquipmentManagerComponent"));
	HealthComponent = CreateDefaultSubobject<ULyraHealthComponent>(TEXT("HealthComponent"));
	HealthComponent->OnDeathStarted.AddDynamic(this, &ThisClass::OnDeathStarted);
	HealthComponent->OnDeathFinished.AddDynamic(this, &ThisClass::OnDeathFinished);
}

ALyraPlayerController* AAC_CharacterBase::GetLyraPlayerController() const
{
	return Cast<ALyraPlayerController>(GetController());
}

ALyraPlayerState* AAC_CharacterBase::GetLyraPlayerState() const
{
	return CastChecked<ALyraPlayerState>(GetPlayerState(), ECastCheckedType::NullAllowed);
}

ULyraAbilitySystemComponent* AAC_CharacterBase::GetLyraAbilitySystemComponent() const
{
	if (!PawnExtComponent)
	{
		return nullptr;
	}

	return PawnExtComponent->GetLyraAbilitySystemComponent();
}

UAbilitySystemComponent* AAC_CharacterBase::GetAbilitySystemComponent() const
{
	return GetLyraAbilitySystemComponent();
}

void AAC_CharacterBase::GetOwnedGameplayTags(FGameplayTagContainer& TagContainer) const
{
	if (const ULyraAbilitySystemComponent* LyraASC = GetLyraAbilitySystemComponent())
	{
		LyraASC->GetOwnedGameplayTags(TagContainer);
	}
}

bool AAC_CharacterBase::HasMatchingGameplayTag(FGameplayTag TagToCheck) const
{
	if (const ULyraAbilitySystemComponent* LyraASC = GetLyraAbilitySystemComponent())
	{
		return LyraASC->HasMatchingGameplayTag(TagToCheck);
	}

	return false;
}

bool AAC_CharacterBase::HasAllMatchingGameplayTags(const FGameplayTagContainer& TagContainer) const
{
	if (const ULyraAbilitySystemComponent* LyraASC = GetLyraAbilitySystemComponent())
	{
		return LyraASC->HasAllMatchingGameplayTags(TagContainer);
	}

	return false;
}

bool AAC_CharacterBase::HasAnyMatchingGameplayTags(const FGameplayTagContainer& TagContainer) const
{
	if (const ULyraAbilitySystemComponent* LyraASC = GetLyraAbilitySystemComponent())
	{
		return LyraASC->HasAnyMatchingGameplayTags(TagContainer);
	}

	return false;
}

FVector AAC_CharacterBase::GetVelocity() const
{
	// UMoverComponent is not a UPawnMovementComponent, so APawn::GetVelocity() finds no
	// movement component and returns zero. Read the velocity out of the Mover sync state.
	if (MoverComponent)
	{
		const FMoverSyncState& SyncState = MoverComponent->GetSyncState();
		if (const FMoverDefaultSyncState* DefaultSyncState =
				SyncState.SyncStateCollection.FindDataByType<FMoverDefaultSyncState>())
		{
			return DefaultSyncState->GetVelocity_WorldSpace();
		}
	}

	return Super::GetVelocity();
}

void AAC_CharacterBase::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	if (MeshComponent && CharacterPartsComponent)
	{
		// Blueprint component defaults have been applied by this point. Keep that
		// configured body as the cosmetic fallback so a part change cannot clear it.
		CharacterPartsComponent->SetDefaultBodyMesh(MeshComponent->GetSkeletalMeshAsset());
	}
}

void AAC_CharacterBase::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
}

void AAC_CharacterBase::BeginPlay()
{
	Super::BeginPlay();
}

void AAC_CharacterBase::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	UpdateAnimationStateFromMover(DeltaSeconds);
	HoldAttachmentAgainstMover();
}

void AAC_CharacterBase::HoldAttachmentAgainstMover()
{
	// 탈것에 탑승하면 AttachToComponent 가 루트 컴포넌트를 부모에 붙이고 상대 오프셋을
	// 0 으로 스냅한다. 그런데 UMoverComponent::FinalizeFrame(MoverComponent.cpp:356) 은
	// 매 시뮬레이션 프레임마다 UpdatedComponent->SetWorldTransform() 으로 SyncState 에
	// 저장된 절대 좌표를 되쓴다. Null 이동 모드는 SyncState 를 갱신하지 않으므로, 그 값은
	// 탑승 직전의 오래된 위치 그대로다 — 결과적으로 붙인 상대 오프셋이 매 프레임 지워지고
	// 캡슐이 그 오래된 위치로 계속 튕겨나간다. 부모-자식 관계 자체는 안 끊긴다
	// (SetWorldTransform 은 절대 좌표만 주고 부모 관계는 그대로 둔다) — 그래서 겉보기엔
	// "붙어 있는데 위치가 이상하고, 부모가 움직여도 안 따라가는" 증상으로 나타난다.
	//
	// 근본적으로 고치려면 Mover 의 이동 기반(Movement Base) 시스템으로 탑승을 구현해야
	// 하지만, 그건 전용 이동 모드가 필요한 별개 작업이다. 여기서는 Mover 가 되쓴 것을
	// 매 프레임 다시 지운다 — 부착 중일 때만, 원점으로.
	if (USceneComponent* Root = GetRootComponent())
	{
		if (Root->GetAttachParent() != nullptr)
		{
			Root->SetRelativeLocation(FVector::ZeroVector);
			Root->SetRelativeRotation(FRotator::ZeroRotator);
		}
	}
}

void AAC_CharacterBase::UpdateAnimationStateFromMover(float DeltaSeconds)
{
	// Aiming only depends on the controller, so it is resolved before the Mover state and
	// stays valid even while the Mover component is missing.
	const FRotator AimDelta = (GetBaseAimRotation() - GetActorRotation()).GetNormalized();
	AimYaw = FMath::Clamp(AimDelta.Yaw, -90.0f, 90.0f);
	AimPitch = FMath::Clamp(AimDelta.Pitch, -90.0f, 90.0f);

	if (!MoverComponent)
	{
		GroundSpeed = 0.0f;
		LocalVelocity = FVector::ZeroVector;
		MovementDirection = FVector::ZeroVector;
		bIsMoving = false;
		bIsFalling = false;
		bIsJumpStarting = false;
		bIsLanding = false;
		MovementMode = NAME_None;
		return;
	}

	const FMoverSyncState& SyncState = MoverComponent->GetSyncState();
	MovementMode = SyncState.MovementMode;
	bIsFalling = MoverComponent->IsFalling();
	RefreshMovementModeGameplayTag();

	// The landing pose is a timed window opened by the airborne-to-grounded edge, because
	// Mover reports no landing state of its own.
	if (bWasFallingLastUpdate && !bIsFalling)
	{
		LandingTimeRemaining = LandingRecoveryTime;
	}
	else
	{
		LandingTimeRemaining = FMath::Max(0.0f, LandingTimeRemaining - DeltaSeconds);
	}
	bIsLanding = !bIsFalling && LandingTimeRemaining > 0.0f;
	bWasFallingLastUpdate = bIsFalling;

	const FMoverDefaultSyncState* DefaultSyncState =
		SyncState.SyncStateCollection.FindDataByType<FMoverDefaultSyncState>();
	if (!DefaultSyncState)
	{
		GroundSpeed = 0.0f;
		LocalVelocity = FVector::ZeroVector;
		MovementDirection = FVector::ZeroVector;
		bIsMoving = false;
		bIsJumpStarting = false;
		return;
	}

	const FVector WorldVelocity = DefaultSyncState->GetVelocity_WorldSpace();
	const FVector PlanarVelocity = FVector::VectorPlaneProject(WorldVelocity, MoverComponent->GetUpDirection());
	const FTransform ActorTransform = GetActorTransform();

	GroundSpeed = PlanarVelocity.Size();
	LocalVelocity = ActorTransform.InverseTransformVectorNoScale(WorldVelocity);
	MovementDirection = ActorTransform.InverseTransformVectorNoScale(PlanarVelocity).GetSafeNormal();
	bIsMoving = GroundSpeed > MovingSpeedThreshold;

	const double VerticalSpeed = WorldVelocity | MoverComponent->GetUpDirection();
	bIsJumpStarting = bIsFalling && VerticalSpeed > JumpRisingSpeedThreshold;

	if (bIsFalling && VerticalSpeed > 0.0)
	{
		const double GravityMagnitude = MoverComponent->GetGravityAcceleration().Size();
		TimeToJumpApex = (GravityMagnitude > UE_SMALL_NUMBER)
			? static_cast<float>(VerticalSpeed / GravityMagnitude)
			: 0.0f;
	}
	else
	{
		TimeToJumpApex = 0.0f;
	}

	if (!bHasLoggedMovementState || bLastLoggedMovingState != bIsMoving)
	{
		UE_LOG(LogTemp, Display, TEXT("[UAFMover] Movement state: %s, GroundSpeed=%.2f, Mode=%s"),
			bIsMoving ? TEXT("Moving") : TEXT("Idle"), GroundSpeed, *MovementMode.ToString());
		bHasLoggedMovementState = true;
		bLastLoggedMovingState = bIsMoving;
	}
}

int32 AAC_CharacterBase::GetAnimGaitIndex() const
{
	if (GroundSpeed <= WalkGaitMaxSpeed)
	{
		return 0;
	}
	return (GroundSpeed <= RunGaitMaxSpeed) ? 1 : 2;
}

FRotator AAC_CharacterBase::GetAnimAimRotation() const
{
	return GetBaseAimRotation();
}

FVector AAC_CharacterBase::GetAnimGroundLocation() const
{
	const float HalfHeight = CapsuleComponent ? CapsuleComponent->GetScaledCapsuleHalfHeight() : 0.0f;
	return GetActorLocation() - GetActorUpVector() * HalfHeight;
}

FVector AAC_CharacterBase::GetAnimGroundNormal() const
{
	return MoverComponent ? MoverComponent->GetUpDirection() : FVector::UpVector;
}

FVector AAC_CharacterBase::GetAnimInputAcceleration() const
{
	return LastConsumedMovementInput.GetSafeNormal() * AnimMaxAcceleration;
}

bool AAC_CharacterBase::IsPistolEquipped() const
{
	if (bIsMounted)
	{
		return false;
	}

	return EquipmentManagerComponent
		&& EquipmentManagerComponent->GetFirstInstanceOfType<ULyraRangedWeaponInstance>() != nullptr;
}

// ULyraPawnExtensionComponent 는 이 알림들을 받아야 CheckDefaultInitialization() 을 다시 돌린다.
// 알림이 없으면 PawnData 와 Controller 가 모두 갖춰진 뒤에도 InitState 체인이 Spawned 에서 멈추고,
// ULyraHeroComponent 가 DataInitialized 에 도달하지 못해 입력 바인딩과 카메라 모드가 모두 설정되지 않는다.
// ALyraCharacter 는 같은 알림을 PossessedBy / UnPossessed / OnRep_Controller / OnRep_PlayerState /
// SetupPlayerInputComponent 다섯 곳에서 전달한다.

void AAC_CharacterBase::OnAbilitySystemInitialized()
{
	if (ULyraAbilitySystemComponent* LyraASC = PawnExtComponent->GetLyraAbilitySystemComponent())
	{
		HealthComponent->InitializeWithAbilitySystem(LyraASC);
		ClearMovementModeGameplayTags(*LyraASC);
		ActiveMovementModeTag = FGameplayTag();
	}
	RefreshMovementModeGameplayTag();
	AddInitialInventory();
}

void AAC_CharacterBase::ClearInitialInventory()
{
	if (!HasAuthority())
	{
		return;
	}

	AController* OwningController = GetController();
	ULyraInventoryManagerComponent* InventoryManager =
		OwningController ? OwningController->FindComponentByClass<ULyraInventoryManagerComponent>() : nullptr;
	ULyraQuickBarComponent* QuickBar =
		OwningController ? OwningController->FindComponentByClass<ULyraQuickBarComponent>() : nullptr;
	if (!InventoryManager || !QuickBar)
	{
		return;
	}

	for (int32 SlotIndex = 0; SlotIndex < QuickBar->GetSlots().Num(); ++SlotIndex)
	{
		QuickBar->RemoveItemFromSlot(SlotIndex);
	}
	for (ULyraInventoryItemInstance* Item : InventoryManager->GetAllItems())
	{
		InventoryManager->RemoveItemInstance(Item);
	}
	bInitialInventoryGranted = false;
}

void AAC_CharacterBase::AddInitialInventory()
{
	if (!HasAuthority() || bInitialInventoryGranted ||
		(InitialInventoryItems.IsEmpty() && InitialInventoryItemClassPaths.IsEmpty()))
	{
		return;
	}

	AController* OwningController = GetController();
	ULyraInventoryManagerComponent* InventoryManager =
		OwningController ? OwningController->FindComponentByClass<ULyraInventoryManagerComponent>() : nullptr;
	ULyraQuickBarComponent* QuickBar =
		OwningController ? OwningController->FindComponentByClass<ULyraQuickBarComponent>() : nullptr;
	if (!InventoryManager || !QuickBar)
	{
		return;
	}

	TArray<TSubclassOf<ULyraInventoryItemDefinition>> ResolvedItems = InitialInventoryItems;
	for (const FString& ItemClassPath : InitialInventoryItemClassPaths)
	{
		if (UClass* ItemClass = LoadClass<ULyraInventoryItemDefinition>(nullptr, *ItemClassPath))
		{
			ResolvedItems.Add(ItemClass);
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("[UAFMover][Inventory] Failed to load item definition: %s"), *ItemClassPath);
		}
	}

	bInitialInventoryGranted = true;
	int32 GrantedItemCount = 0;
	for (int32 Index = 0; Index < ResolvedItems.Num(); ++Index)
	{
		if (TSubclassOf<ULyraInventoryItemDefinition> ItemDefinition = ResolvedItems[Index])
		{
			if (ULyraInventoryItemInstance* Item = InventoryManager->AddItemDefinition(ItemDefinition))
			{
				QuickBar->AddItemToSlot(Index, Item);
				++GrantedItemCount;
			}
		}
	}

	if (GrantedItemCount > 0)
	{
		QuickBar->SetActiveSlotIndex(0);
	}
	ULyraEquipmentInstance* EquippedInstance = EquipmentManagerComponent
		? EquipmentManagerComponent->GetFirstInstanceOfType<ULyraEquipmentInstance>()
		: nullptr;
	const TArray<AActor*> SpawnedActors = EquippedInstance ? EquippedInstance->GetSpawnedActors() : TArray<AActor*>();
	const bool bFirstActorAttachedToMesh = SpawnedActors.Num() > 0 && SpawnedActors[0]
		&& SpawnedActors[0]->GetRootComponent()
		&& SpawnedActors[0]->GetRootComponent()->GetAttachParent() == MeshComponent;
	UE_LOG(LogTemp, Display,
		TEXT("[UAFMover][Inventory] Granted=%d EquippedSlot=%d Equipment=%s SpawnedActors=%d AttachedToMesh=%d"),
		GrantedItemCount, QuickBar->GetActiveSlotIndex(), *GetNameSafe(EquippedInstance),
		SpawnedActors.Num(), bFirstActorAttachedToMesh ? 1 : 0);
}

void AAC_CharacterBase::OnAbilitySystemUninitialized()
{
	HealthComponent->UninitializeFromAbilitySystem();
	if (ULyraAbilitySystemComponent* LyraASC = PawnExtComponent->GetLyraAbilitySystemComponent())
	{
		ClearMovementModeGameplayTags(*LyraASC);
	}
	ActiveMovementModeTag = FGameplayTag();
}

void AAC_CharacterBase::Reset()
{
	DisableMovementAndCollision();
	ClearInitialInventory();
	UninitAndDestroy();
}

void AAC_CharacterBase::FellOutOfWorld(const UDamageType& /*DamageType*/)
{
	HealthComponent->DamageSelfDestruct(true);
}

void AAC_CharacterBase::OnDeathStarted(AActor* /*OwningActor*/)
{
	DisableMovementAndCollision();
	HideEquippedWeapons();
	StartRagdoll();
}

void AAC_CharacterBase::OnDeathFinished(AActor* /*OwningActor*/)
{
	ClearInitialInventory();
	GetWorld()->GetTimerManager().SetTimerForNextTick(this, &ThisClass::DestroyDueToDeath);
}

void AAC_CharacterBase::DisableMovementAndCollision()
{
	if (Controller)
	{
		Controller->SetIgnoreMoveInput(true);
	}
	bJumpPressed = false;
	bJumpJustPressed = false;
	CapsuleComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	CapsuleComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
	MoverComponent->Deactivate();
}

void AAC_CharacterBase::StartRagdoll()
{
	SetActorTickEnabled(false);
	if (MeshComponent)
	{
		MeshComponent->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
		MeshComponent->SetCollisionProfileName(TEXT("Ragdoll"), true);
		MeshComponent->SetSimulatePhysics(true);
		MeshComponent->SetAllBodiesBelowSimulatePhysics(NAME_None, true, true);
		MeshComponent->SetAllBodiesPhysicsBlendWeight(1.0f, false);
		MeshComponent->SetPhysicsLinearVelocity(FVector::ZeroVector);
		MeshComponent->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
		MeshComponent->WakeAllRigidBodies();
		UE_LOG(LogTemp, Display,
			TEXT("[UAFMover][Death] Ragdoll Mesh=%s PhysicsAsset=%s Simulating=%d"),
			*GetNameSafe(MeshComponent->GetSkeletalMeshAsset()),
			*GetNameSafe(MeshComponent->GetPhysicsAsset()),
			MeshComponent->IsSimulatingPhysics() ? 1 : 0);
	}
}

void AAC_CharacterBase::HideEquippedWeapons()
{
	if (!EquipmentManagerComponent)
	{
		return;
	}
	for (ULyraEquipmentInstance* Instance :
		EquipmentManagerComponent->GetEquipmentInstancesOfType(ULyraEquipmentInstance::StaticClass()))
	{
		for (AActor* SpawnedActor : Instance->GetSpawnedActors())
		{
			if (SpawnedActor)
			{
				SpawnedActor->SetActorHiddenInGame(true);
			}
		}
	}
}

void AAC_CharacterBase::DestroyDueToDeath()
{
	UninitAndDestroy();
}

void AAC_CharacterBase::UninitAndDestroy()
{
	if (GetLocalRole() == ROLE_Authority)
	{
		DetachFromControllerPendingDestroy();
		SetLifeSpan(0.1f);
	}

	if (ULyraAbilitySystemComponent* LyraASC = PawnExtComponent->GetLyraAbilitySystemComponent())
	{
		if (LyraASC->GetAvatarActor() == this)
		{
			PawnExtComponent->UninitializeAbilitySystem();
		}
	}
	SetActorHiddenInGame(true);
}

void AAC_CharacterBase::RefreshMovementModeGameplayTag()
{
	ULyraAbilitySystemComponent* LyraASC = PawnExtComponent->GetLyraAbilitySystemComponent();
	if (!LyraASC)
	{
		ActiveMovementModeTag = FGameplayTag();
		return;
	}

	FGameplayTag DesiredTag;
	if (MovementMode == DefaultModeNames::Walking)
	{
		DesiredTag = LyraGameplayTags::Movement_Mode_Walking;
	}
	else if (MovementMode == DefaultModeNames::Falling)
	{
		DesiredTag = LyraGameplayTags::Movement_Mode_Falling;
	}
	else if (MovementMode == DefaultModeNames::Flying)
	{
		DesiredTag = LyraGameplayTags::Movement_Mode_Flying;
	}
	else if (MovementMode == DefaultModeNames::Swimming)
	{
		DesiredTag = LyraGameplayTags::Movement_Mode_Swimming;
	}

	if (DesiredTag == ActiveMovementModeTag)
	{
		return;
	}

	const FGameplayTag PreviousTag = ActiveMovementModeTag;
	if (ActiveMovementModeTag.IsValid())
	{
		LyraASC->SetLooseGameplayTagCount(ActiveMovementModeTag, 0);
	}
	if (DesiredTag.IsValid())
	{
		LyraASC->SetLooseGameplayTagCount(DesiredTag, 1);
	}
	ActiveMovementModeTag = DesiredTag;

	UE_LOG(LogTemp, Display, TEXT("[UAFMover][MovementTag] Mode=%s Previous=%s Current=%s"),
		*MovementMode.ToString(), *PreviousTag.ToString(), *ActiveMovementModeTag.ToString());
}

void AAC_CharacterBase::ClearMovementModeGameplayTags(ULyraAbilitySystemComponent& AbilitySystem)
{
	for (const TPair<uint8, FGameplayTag>& TagMapping : LyraGameplayTags::MovementModeTagMap)
	{
		if (TagMapping.Value.IsValid())
		{
			AbilitySystem.SetLooseGameplayTagCount(TagMapping.Value, 0);
		}
	}
	for (const TPair<uint8, FGameplayTag>& TagMapping : LyraGameplayTags::CustomMovementModeTagMap)
	{
		if (TagMapping.Value.IsValid())
		{
			AbilitySystem.SetLooseGameplayTagCount(TagMapping.Value, 0);
		}
	}
}

void AAC_CharacterBase::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	if (PawnExtComponent)
	{
		PawnExtComponent->HandleControllerChanged();
	}
}

void AAC_CharacterBase::UnPossessed()
{
	bJumpPressed = false;
	bJumpJustPressed = false;

	Super::UnPossessed();

	if (PawnExtComponent)
	{
		PawnExtComponent->HandleControllerChanged();
	}
}

void AAC_CharacterBase::OnRep_Controller()
{
	Super::OnRep_Controller();

	if (PawnExtComponent)
	{
		PawnExtComponent->HandleControllerChanged();
	}
}

void AAC_CharacterBase::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();

	if (PawnExtComponent)
	{
		PawnExtComponent->HandlePlayerStateReplicated();
	}
}

void AAC_CharacterBase::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	// Move / Look / IMC 등록은 ULyraHeroComponent 가 PawnData 의 InputConfig 로 처리한다.
	// 여기서는 Jump 만 직접 바인딩한다. Lyra 는 InputTag.Jump 를 어빌리티 입력으로 쓰지만
	// LyraGameplayAbility_Jump 가 ALyraCharacter 로 캐스팅하므로 Mover 폰에서는 쓸 수 없다.
	// 초기화 체인을 진행시킨다. 이 알림이 없으면 HeroComponent 가 입력을 바인딩하지 못한다.
	if (PawnExtComponent)
	{
		PawnExtComponent->SetupPlayerInputComponent();
	}

}

void AAC_CharacterBase::ProduceInput_Implementation(int32 /*SimTimeMs*/, FMoverInputCmdContext& InputCmd)
{
	FCharacterDefaultInputs& Inputs =
		InputCmd.InputCollection.FindOrAddMutableDataByType<FCharacterDefaultInputs>();

	// NetworkPrediction (UMoverComponent's default backend) ticks at a fixed 60 Hz and
	// calls ProduceInput once per SIMULATION frame, not once per engine frame - see
	// NetworkPrediction/Public/NetworkPredictionConfig.h:24. Below 60 fps several sim
	// frames can be produced within a single engine frame to catch up.
	// ConsumeMovementInputVector() is destructive: the first call in an engine frame
	// returns the accumulated input and drains it, so every catch-up call in that same
	// frame would otherwise see zero and roughly half the movement intent would be
	// dropped, making speed framerate-dependent and causing stutter after hitches.
	// Controller->GetControlRotation() is not destructive, but [AimDebug] logging showed
	// it reading back as exactly (0,0,0) on roughly every other ProduceInput call within
	// the same engine frame -- some catch-up call in the chain observes the controller
	// mid-update, before its rotation for this frame is fully applied. Feeding that
	// transient zero into OrientationIntent below flips the body to face world-forward
	// on alternating simulation frames, which is exactly what looked like "won't turn" /
	// "animation is glitching": the target orientation was oscillating at simulation rate.
	// Latch BOTH the movement input and the control rotation once per engine frame
	// (GFrameCounter) and reuse them for any further ProduceInput calls within that same
	// frame. Do NOT "simplify" this back to bare ConsumeMovementInputVector() /
	// Controller->GetControlRotation() calls - that was the pre-fix bug.
	if (!bHasConsumedInputThisFrame || LastInputConsumedFrame != GFrameCounter)
	{
		LastConsumedMovementInput = ConsumeMovementInputVector();
		if (Controller)
		{
			LastConsumedControlRotation = Controller->GetControlRotation();
		}
		LastInputConsumedFrame = GFrameCounter;
		bHasConsumedInputThisFrame = true;
	}

	Inputs.ControlRotation = LastConsumedControlRotation;

	// ULyraHeroComponent::Input_Move 가 컨트롤 로테이션 Yaw 를 이미 반영한
	// 월드 공간 방향으로 AddMovementInput 을 호출해 누적해 둔 값이다.
	const FVector Intent = LastConsumedMovementInput;
	const FVector ClampedIntent = Intent.GetClampedToMaxSize(1.0f);

	Inputs.SetMoveInput(EMoveInputType::DirectionalIntent, ClampedIntent);

	// Keep body orientation independent from camera aim so Aim Offset handles aiming.
	// The Mover uses the movement intent as the character's facing direction.
	Inputs.OrientationIntent = ClampedIntent;
	Inputs.bIsJumpPressed = bJumpPressed;
	Inputs.bIsJumpJustPressed = bJumpJustPressed;

	bJumpJustPressed = false;
}

bool AAC_CharacterBase::CanStartMoverJump() const
{
	return MoverComponent && !MoverComponent->IsFalling();
}

void AAC_CharacterBase::StartMoverJump()
{
	if (IsLocallyControlled() && CanStartMoverJump())
	{
		bJumpPressed = true;
		bJumpJustPressed = true;
	}
}

void AAC_CharacterBase::StopMoverJump()
{
	bJumpPressed = false;
}
