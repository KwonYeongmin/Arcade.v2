// Copyright Epic Games, Inc. All Rights Reserved.

#include "Character/AC_ShooterCharacter.h"

#include "Camera/CameraComponent.h"
#include "Character/LyraHealthComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Cosmetics/LyraPawnComponent_CharacterParts.h"
#include "Equipment/LyraQuickBarComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Input/LyraInputComponent.h"
#include "Input/LyraInputConfig.h"
#include "InputActionValue.h"
#include "LyraGameplayTags.h"
#include "Character/LyraPawnExtensionComponent.h"
#include "Character/LyraPawnData.h"
#include "Inventory/LyraInventoryItemDefinition.h"
#include "Inventory/LyraInventoryManagerComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Perception/AISense_Damage.h"
#include "Teams/LyraTeamAgentInterface.h"
#include "Teams/LyraTeamDisplayAsset.h"
#include "Teams/LyraTeamStatics.h"
#include "Teams/LyraTeamSubsystem.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "GameFramework/PlayerState.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AC_ShooterCharacter)

AAC_ShooterCharacter::AAC_ShooterCharacter(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    // 오토바이에 Attach 되면 메시가 화면 프레임/바운드 계산이 꼬여 "렌더링 안 됨"으로
    // 오판될 수 있다. 기본값(화면에 보일 때만 포즈 계산)이면 그 상태에서 애니메이션
    // 평가 자체가 스킵되어 마지막 포즈(또는 바인드 포즈=T-pose)에 멈춘다.
    // 탈 것 검증 목적으로 항상 포즈를 계산하도록 강제한다.
    if (USkeletalMeshComponent* MeshComp = GetMesh())
    {
        MeshComp->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
    }
}

void AAC_ShooterCharacter::BeginPlay()
{
    Super::BeginPlay();

    if (const UCharacterMovementComponent* CMC = GetCharacterMovement())
    {
        DefaultGravityScale = CMC->GravityScale;
    }

    if (ULyraHealthComponent* HealthComp = ULyraHealthComponent::FindHealthComponent(this))
    {
        HealthComp->OnHealthChanged.AddDynamic(this, &AAC_ShooterCharacter::OnHealthChangedHandler);
    }

    SetActorHiddenInGame(true);

    if (ILyraTeamAgentInterface* TeamInterface = Cast<ILyraTeamAgentInterface>(this))
    {
        TeamInterface->GetTeamChangedDelegateChecked().AddDynamic(
            this, &AAC_ShooterCharacter::OnTeamIndexChanged);

        const int32 TeamId = GenericTeamIdToInteger(GetGenericTeamId());
        OnTeamIndexChanged(nullptr, INDEX_NONE, TeamId);
    }

    if (ULyraPawnComponent_CharacterParts* CosmeticsComp =
            FindComponentByClass<ULyraPawnComponent_CharacterParts>())
    {
        CosmeticsComp->OnCharacterPartsChanged.AddDynamic(
            this, &AAC_ShooterCharacter::OnCharacterPartsChanged);
    }

}

void AAC_ShooterCharacter::OnAbilitySystemInitialized()
{
    Super::OnAbilitySystemInitialized();

    GetWorld()->GetTimerManager().SetTimerForNextTick(
        this, &AAC_ShooterCharacter::ShowPawnAfterInitialized);
}

void AAC_ShooterCharacter::ShowPawnAfterInitialized()
{
    SetActorHiddenInGame(false);

    if (HasAuthority())
    {
        AddInitialInventory();
    }
}

void AAC_ShooterCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);

    ULyraInputComponent* LIC = Cast<ULyraInputComponent>(PlayerInputComponent);
    if (!LIC) return;

    const ULyraPawnExtensionComponent* PawnExtComp = ULyraPawnExtensionComponent::FindPawnExtensionComponent(this);
    const ULyraPawnData* PawnData = PawnExtComp ? PawnExtComp->GetPawnData<ULyraPawnData>() : nullptr;
    const ULyraInputConfig* InputConfig = PawnData ? PawnData->InputConfig : nullptr;

    if (InputConfig)
    {
        LIC->BindNativeAction(InputConfig, LyraGameplayTags::InputTag_Flight_Ascend, ETriggerEvent::Triggered, this, &ThisClass::Input_AscendTriggered, /*bLogIfNotFound=*/ true);
        LIC->BindNativeAction(InputConfig, LyraGameplayTags::InputTag_Flight_Descend, ETriggerEvent::Started, this, &ThisClass::Input_DescendActive, /*bLogIfNotFound=*/ true);
    }

    if (QuickSlot1InputAction)
        LIC->BindAction(QuickSlot1InputAction, ETriggerEvent::Triggered, this, &ThisClass::Input_QuickSlot1);
    if (QuickSlot2InputAction)
        LIC->BindAction(QuickSlot2InputAction, ETriggerEvent::Triggered, this, &ThisClass::Input_QuickSlot2);
    if (QuickSlot3InputAction)
        LIC->BindAction(QuickSlot3InputAction, ETriggerEvent::Triggered, this, &ThisClass::Input_QuickSlot3);
    if (QuickSlotCycleBackwardInputAction)
        LIC->BindAction(QuickSlotCycleBackwardInputAction, ETriggerEvent::Triggered, this, &ThisClass::Input_QuickSlotCycleBackward);
    if (QuickSlotCycleForwardInputAction)
        LIC->BindAction(QuickSlotCycleForwardInputAction, ETriggerEvent::Triggered, this, &ThisClass::Input_QuickSlotCycleForward);
}

void AAC_ShooterCharacter::Input_QuickSlot1()
{
    static const FGameplayTag Tag =
        FGameplayTag::RequestGameplayTag(FName("InputTag.Ability.Quickslot.SelectSlot"));
    ChangeQuickbarSlot(0, Tag);
}

void AAC_ShooterCharacter::Input_QuickSlot2()
{
    static const FGameplayTag Tag =
        FGameplayTag::RequestGameplayTag(FName("InputTag.Ability.Quickslot.SelectSlot"));
    ChangeQuickbarSlot(1, Tag);
}

void AAC_ShooterCharacter::Input_QuickSlot3()
{
    static const FGameplayTag Tag =
        FGameplayTag::RequestGameplayTag(FName("InputTag.Ability.Quickslot.SelectSlot"));
    ChangeQuickbarSlot(2, Tag);
}

void AAC_ShooterCharacter::Input_QuickSlotCycleBackward()
{
    static const FGameplayTag Tag =
        FGameplayTag::RequestGameplayTag(FName("InputTag.Ability.Quickslot.CycleBackward"));
    ChangeQuickbarSlot(0, Tag);
}

void AAC_ShooterCharacter::Input_QuickSlotCycleForward()
{
    static const FGameplayTag Tag =
        FGameplayTag::RequestGameplayTag(FName("InputTag.Ability.Quickslot.CycleForward"));
    ChangeQuickbarSlot(0, Tag);
}

void AAC_ShooterCharacter::OnTeamIndexChanged(
    UObject* /*ObjectChangingTeam*/, int32 OldTeamID, int32 NewTeamID)
{
    if (ULyraTeamSubsystem* TeamSub = GetWorld()->GetSubsystem<ULyraTeamSubsystem>())
    {
        if (OldTeamID != INDEX_NONE)
            TeamSub->GetTeamDisplayAssetChangedDelegate(OldTeamID).RemoveAll(this);
        if (NewTeamID != INDEX_NONE)
            TeamSub->GetTeamDisplayAssetChangedDelegate(NewTeamID).AddUniqueDynamic(
                this, &AAC_ShooterCharacter::OnDisplayAssetChanged);
    }
    RefreshTeamColors();
}

void AAC_ShooterCharacter::OnDisplayAssetChanged(const ULyraTeamDisplayAsset* /*DisplayAsset*/)
{
    RefreshTeamColors();
}

void AAC_ShooterCharacter::RefreshTeamColors()
{
    const int32 TeamId = GenericTeamIdToInteger(GetGenericTeamId());
    ULyraTeamDisplayAsset* DisplayAsset = ULyraTeamStatics::GetTeamDisplayAsset(this, TeamId);
    OnTeamOrCosmeticsChanged(TeamId, DisplayAsset);
}

void AAC_ShooterCharacter::OnCharacterPartsChanged(
    ULyraPawnComponent_CharacterParts* /*ComponentWithChangedParts*/)
{
    RefreshTeamColors();
}

void AAC_ShooterCharacter::SetEmoteAudioComponent_Implementation(
    UAudioComponent* InAudioComponent)
{
    EmoteAudioComponent = InAudioComponent;
}

void AAC_ShooterCharacter::AddInitialInventory()
{
    if (!HasAuthority() || bInitialInventoryGranted)
        return;

    AController* PC = GetController();
    if (!IsValid(PC))
        return;

    ULyraInventoryManagerComponent* InvMgr = PC->FindComponentByClass<ULyraInventoryManagerComponent>();
    if (!IsValid(InvMgr))
        return;

    ULyraQuickBarComponent* QB = PC->FindComponentByClass<ULyraQuickBarComponent>();
    if (!IsValid(QB))
        return;

    InventoryManager = InvMgr;
    QuickBar = QB;
    bInitialInventoryGranted = true;

    for (int32 i = 0; i < InitialInventoryItems.Num(); ++i)
    {
        TSubclassOf<ULyraInventoryItemDefinition> ItemDef = InitialInventoryItems[i];
        if (!ItemDef)
            continue;

        ULyraInventoryItemInstance* Item = InvMgr->AddItemDefinition(ItemDef);
        QB->AddItemToSlot(i, Item);
    }

    if (InitialInventoryItems.Num() > 0)
    {
        QB->SetActiveSlotIndex(0);
    }
}

void AAC_ShooterCharacter::ClearInventory()
{
    if (!HasAuthority())
        return;

    AController* PC = GetController();
    if (!IsValid(PC))
        return;

    ULyraQuickBarComponent* QB = PC->FindComponentByClass<ULyraQuickBarComponent>();
    ULyraInventoryManagerComponent* InvMgr = PC->FindComponentByClass<ULyraInventoryManagerComponent>();

    if (!IsValid(QB) || !IsValid(InvMgr))
        return;

    QuickBar = QB;
    InventoryManager = InvMgr;
    bInitialInventoryGranted = false;

    TArray<ULyraInventoryItemInstance*> Slots = QB->GetSlots();
    for (int32 i = 0; i < Slots.Num(); ++i)
    {
        QB->RemoveItemFromSlot(i);
    }

    TArray<ULyraInventoryItemInstance*> AllItems = InvMgr->GetAllItems();
    for (ULyraInventoryItemInstance* Item : AllItems)
    {
        InvMgr->RemoveItemInstance(Item);
    }
}

void AAC_ShooterCharacter::ChangeQuickbarSlot(int32 NewSlotIndex, FGameplayTag SlotActionTag)
{
    AController* PC = GetController();
    if (!IsValid(PC))
        return;

    ULyraQuickBarComponent* QB = PC->FindComponentByClass<ULyraQuickBarComponent>();
    if (!IsValid(QB))
        return;

    TArray<ULyraInventoryItemInstance*> Slots = QB->GetSlots();
    if (!Slots.IsValidIndex(NewSlotIndex) || !IsValid(Slots[NewSlotIndex]))
        return;

    FGameplayEventData Payload;
    Payload.EventMagnitude = static_cast<float>(NewSlotIndex);

    UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(this, SlotActionTag, Payload);
}

void AAC_ShooterCharacter::CreateOrUpdateOutlineIfNeeded(int32 TeamID, FLinearColor OutlineColor)
{
    if (!IsLocallyControlled())
        return;

    if (!IsValid(MPP_OutlineMaterial))
    {
        if (!OutlineMaterialTemplate)
            return;

        MPP_OutlineMaterial = UMaterialInstanceDynamic::Create(OutlineMaterialTemplate, this);

        if (UCameraComponent* CameraComp = FindComponentByClass<UCameraComponent>())
        {
            CameraComp->AddOrUpdateBlendable(MPP_OutlineMaterial);
        }
    }

    MPP_OutlineMaterial->SetScalarParameterValue(TEXT("TeamID"), static_cast<float>(TeamID));
    MPP_OutlineMaterial->SetVectorParameterValue(TEXT("TeamColor"), OutlineColor);
}

void AAC_ShooterCharacter::OnTeamOrCosmeticsChanged(int32 TeamID, ULyraTeamDisplayAsset* TeamDisplayAsset)
{
    FLinearColor TeamColor(0.645833f, 0.0f, 0.568715f, 1.0f);
    if (IsValid(TeamDisplayAsset))
    {
        if (const FLinearColor* Found = TeamDisplayAsset->ColorParameters.Find(TEXT("TeamColor")))
        {
            TeamColor = *Found;
        }
    }

    CreateOrUpdateOutlineIfNeeded(TeamID, TeamColor);
    UpdateCustomStencilFromTeamID(TeamID);

    if (IsValid(TeamDisplayAsset))
    {
        TeamDisplayAsset->ApplyToActor(this, true);
    }
}

void AAC_ShooterCharacter::OnHealthChangedHandler(
    ULyraHealthComponent* InHealthComponent, float OldValue, float NewValue, AActor* InInstigator)
{
    if (!HasAuthority())
        return;

    APlayerState* InstigatorPS = Cast<APlayerState>(InInstigator);
    if (!IsValid(InstigatorPS))
        return;

    UAISense_Damage::ReportDamageEvent(
        GetWorld(),
        this,
        InInstigator,
        FMath::Abs(OldValue - NewValue),
        GetActorLocation(),
        GetActorLocation());
}

void AAC_ShooterCharacter::OnDeathFinished(AActor* OwningActor)
{
    ClearInventory();
    Super::OnDeathFinished(OwningActor);
}

void AAC_ShooterCharacter::Reset()
{
    ClearInventory();
    Super::Reset();
}

// --- Flight Movement ---

void AAC_ShooterCharacter::Input_AscendTriggered(const FInputActionValue& /*Value*/)
{
    UCharacterMovementComponent* CMC = GetCharacterMovement();
    if (!CMC || CMC->MovementMode == MOVE_Flying)
    {
        return;
    }

    // MOVE_Flying에서는 CanCrouchInCurrentState()가 false라 앉은 채로는 일어설 수 없다.
    UnCrouch();
    CMC->SetMovementMode(MOVE_Flying);

    UE_LOG(LogTemp, Warning, TEXT("[Flight] 비행"));
}

void AAC_ShooterCharacter::Input_DescendActive(const FInputActionValue& /*Value*/)
{
    UCharacterMovementComponent* CMC = GetCharacterMovement();
    if (!CMC || CMC->MovementMode != MOVE_Flying)
    {
        // 지상에서의 LeftCtrl은 Lyra의 ToggleCrouch가 처리한다.
        return;
    }

    // PhysFlying은 지면 충돌 시 미끄러질 뿐 착지를 감지하지 않는다.
    // Falling으로 넘겨야 착지 후 엔진이 Walking으로 복귀시킨다.
    // 중력을 낮춰 천천히 내려오게 하고, 착지 시 Landed()가 원복한다.
    CMC->GravityScale = FlightDescendGravityScale;
    CMC->SetMovementMode(MOVE_Falling);

    UE_LOG(LogTemp, Warning, TEXT("[Flight] 하강"));
}

void AAC_ShooterCharacter::Landed(const FHitResult& Hit)
{
    Super::Landed(Hit);

    if (UCharacterMovementComponent* CMC = GetCharacterMovement())
    {
        CMC->GravityScale = DefaultGravityScale;
    }
}
