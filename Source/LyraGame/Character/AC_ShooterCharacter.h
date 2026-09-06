// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Character/LyraHeroCharacter.h"
#include "Character/LyraEmoteSoundInterface.h"
#include "GameplayTagContainer.h"

#include "AC_ShooterCharacter.generated.h"

#define UE_API LYRAGAME_API

class UAudioComponent;
class UInputAction;
class ULyraHealthComponent;
struct FInputActionValue;
class ULyraInventoryItemDefinition;
class ULyraInventoryManagerComponent;
class ULyraPawnComponent_CharacterParts;
class ULyraQuickBarComponent;
class ULyraTeamDisplayAsset;
class UMaterialInstanceDynamic;
class UMaterialInterface;

/**
 * AAC_ShooterCharacter
 *
 * B_Hero_ShooterMannequin BP 로직(인벤토리, 퀵바, 아웃라인, 팀 컬러)을 C++로 포팅한 캐릭터 클래스.
 * B_Hero_ShooterMannequin BP의 Parent Class로 사용된다.
 */
UCLASS(MinimalAPI, Blueprintable)
class AAC_ShooterCharacter : public ALyraHeroCharacter, public ILyraEmoteSoundInterface
{
    GENERATED_BODY()

public:
    UE_API AAC_ShooterCharacter(const FObjectInitializer& ObjectInitializer);

    UFUNCTION(BlueprintCallable, Category="Shooter|Inventory")
    UE_API void AddInitialInventory();

    UFUNCTION(BlueprintCallable, Category="Shooter|Inventory")
    UE_API void ClearInventory();

    UFUNCTION(BlueprintCallable, Category="Shooter|Input")
    UE_API void ChangeQuickbarSlot(int32 NewSlotIndex, FGameplayTag SlotActionTag);

    UFUNCTION(BlueprintCallable, Category="Shooter|Outline")
    UE_API void CreateOrUpdateOutlineIfNeeded(int32 TeamID, FLinearColor OutlineColor);

    UFUNCTION(BlueprintCallable, Category="Shooter|Team")
    UE_API void OnTeamOrCosmeticsChanged(int32 TeamID, ULyraTeamDisplayAsset* TeamDisplayAsset);

    UE_API virtual void SetEmoteAudioComponent_Implementation(UAudioComponent* InAudioComponent) override;

    UE_API virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

public:
    UPROPERTY(EditDefaultsOnly, Category="Shooter|Inventory")
    TArray<TSubclassOf<ULyraInventoryItemDefinition>> InitialInventoryItems;

    UPROPERTY(EditDefaultsOnly, Category="Shooter|Outline")
    TObjectPtr<UMaterialInterface> OutlineMaterialTemplate;

    /** 비행에서 하강할 때 적용할 중력 배율. 낮을수록 천천히 내려온다. */
    UPROPERTY(EditDefaultsOnly, Category="Shooter|Flight", meta=(ClampMin="0.0", UIMin="0.0", UIMax="1.0"))
    float FlightDescendGravityScale = 0.2f;

    UPROPERTY(EditDefaultsOnly, Category="Shooter|Input")
    TObjectPtr<UInputAction> QuickSlot1InputAction;

    UPROPERTY(EditDefaultsOnly, Category="Shooter|Input")
    TObjectPtr<UInputAction> QuickSlot2InputAction;

    UPROPERTY(EditDefaultsOnly, Category="Shooter|Input")
    TObjectPtr<UInputAction> QuickSlot3InputAction;

    UPROPERTY(EditDefaultsOnly, Category="Shooter|Input")
    TObjectPtr<UInputAction> QuickSlotCycleBackwardInputAction;

    UPROPERTY(EditDefaultsOnly, Category="Shooter|Input")
    TObjectPtr<UInputAction> QuickSlotCycleForwardInputAction;

    UPROPERTY(BlueprintReadWrite, Transient, Category="Shooter|Outline")
    TObjectPtr<UMaterialInstanceDynamic> MPP_OutlineMaterial;

    UPROPERTY(BlueprintReadWrite, Transient, Category="Shooter|Audio")
    TObjectPtr<UAudioComponent> EmoteAudioComponent;

protected:
    UE_API virtual void BeginPlay() override;
    UE_API virtual void OnAbilitySystemInitialized() override;
    UE_API virtual void OnDeathFinished(AActor* OwningActor) override;
    UE_API virtual void Reset() override;

    UFUNCTION()
    void OnHealthChangedHandler(ULyraHealthComponent* InHealthComponent,
        float OldValue, float NewValue, AActor* InInstigator);

private:
    /** ASC 재초기화(예: 탈것 하차)가 초기 장비를 중복 지급하지 않도록 한다. */
    bool bInitialInventoryGranted = false;

    UPROPERTY(Transient)
    TObjectPtr<ULyraInventoryManagerComponent> InventoryManager;

    UPROPERTY(Transient)
    TObjectPtr<ULyraQuickBarComponent> QuickBar;

    void ShowPawnAfterInitialized();

    UFUNCTION()
    void OnTeamIndexChanged(UObject* ObjectChangingTeam, int32 OldTeamID, int32 NewTeamID);

    UFUNCTION()
    void OnDisplayAssetChanged(const ULyraTeamDisplayAsset* DisplayAsset);

    void RefreshTeamColors();

    UFUNCTION()
    void OnCharacterPartsChanged(ULyraPawnComponent_CharacterParts* ComponentWithChangedParts);

    void Input_QuickSlot1();
    void Input_QuickSlot2();
    void Input_QuickSlot3();
    void Input_QuickSlotCycleBackward();
    void Input_QuickSlotCycleForward();

    // Flight handlers
    void Input_AscendTriggered(const FInputActionValue& Value);
    void Input_DescendActive(const FInputActionValue& Value);

    /** 하강 중 낮춘 중력을 착지 시 되돌리기 위한 원래 값. */
    float DefaultGravityScale = 1.0f;

    virtual void Landed(const FHitResult& Hit) override;
};

#undef UE_API
