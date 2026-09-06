// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Blueprint/UserWidget.h"
#include "GameplayTagContainer.h"

#include "UAC_CharacterCardWidget.generated.h"

class UButton;
class UDataTable;
class UImage;
class UTextBlock;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCharacterCardClicked, FGameplayTag, RoleTag);

/**
 * UAC_CharacterCardWidget
 *
 * One operative card. Fills its own portrait / name / tagline from the matching
 * FAC_CharacterCardRow (RoleTag + DT_CharacterCards) — in the designer preview too. Its
 * ReadyButton broadcasts OnCardClicked(RoleTag). Occupancy visuals (who holds this role, are
 * they ready, is it me) come from SetOccupancy, driven by replicated state.
 *
 * BP child names: PortraitImage / NameText / TaglineText / WeaponIconImage / StatusText /
 * ReadyButton (all optional).
 */
UCLASS(Abstract)
class LYRAGAME_API UAC_CharacterCardWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable, Category = "Character Card")
	FOnCharacterCardClicked OnCardClicked;

	/** Which operative this card shows. Set per placed instance (or by the screen). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Character Card", meta = (Categories = "Arcade.Role"))
	FGameplayTag RoleTag;

	UFUNCTION(BlueprintCallable, Category = "Character Card")
	void RefreshFromData();

	/**
	 * @param bOccupied      a player currently holds this role
	 * @param occupantName   that player's name (empty when unoccupied)
	 * @param bLocalPlayer   the holder is the viewing player
	 * @param bReady         the holder has confirmed ready
	 */
	UFUNCTION(BlueprintCallable, Category = "Character Card")
	void SetOccupancy(bool bOccupied, const FText& occupantName, bool bLocalPlayer, bool bReady);

	UFUNCTION(BlueprintPure, Category = "Character Card")
	bool IsOccupied() const { return bIsOccupied; }

	/** BP hook for occupancy visuals (frame highlight, dim, ready glow, ...). */
	UFUNCTION(BlueprintImplementableEvent, Category = "Character Card")
	void OnOccupancyChanged(bool bOccupied, bool bLocalPlayer, bool bReady);

protected:
	virtual void NativePreConstruct() override;
	virtual void NativeConstruct() override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Character Card")
	TObjectPtr<UDataTable> CardTable;

	/** Status line format: {0} = occupant name. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Character Card")
	FText ReadyStatusFormat = FText::FromString(TEXT("{0} — READY"));

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Character Card")
	FText SelectingStatusFormat = FText::FromString(TEXT("{0}"));

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Character Card")
	FText EmptyStatusText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> PortraitImage;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> NameText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TaglineText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> WeaponIconImage;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> StatusText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> ReadyButton;

private:
	UFUNCTION()
	void HandleReadyButtonClicked();

	bool bIsOccupied = false;
};
