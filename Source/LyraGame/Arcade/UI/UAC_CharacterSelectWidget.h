// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Blueprint/UserWidget.h"

#include "UAC_CharacterSelectWidget.generated.h"

class UAC_CharacterCardWidget;
class UAC_CharacterSelectComponent;
struct FGameplayTag;

/**
 * UAC_CharacterSelectWidget
 *
 * Screen container for the two operative cards. Created on the owning client by
 * UAC_CharacterSelectComponent, which calls BindToLocalComponent. Reads every player's
 * replicated UAC_CharacterSelectComponent to fill both cards (occupied / ready / mine), and
 * forwards the local player's clicks to the server.
 *
 * Bind (optional): SlickCard, FlintCard (UAC_CharacterCardWidget).
 */
UCLASS(Abstract)
class LYRAGAME_API UAC_CharacterSelectWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Called by the local player's UAC_CharacterSelectComponent right after the widget is shown. */
	void BindToLocalComponent(UAC_CharacterSelectComponent* localComponent);

	/** BP hook — fired after every refresh (e.g. to show "both ready, starting…"). */
	UFUNCTION(BlueprintImplementableEvent, Category = "Character Select")
	void OnSelectionStateChanged(bool bEveryoneReady);

protected:
	virtual void NativeDestruct() override;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UAC_CharacterCardWidget> SlickCard;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UAC_CharacterCardWidget> FlintCard;

private:
	UFUNCTION()
	void HandleCardClicked(FGameplayTag roleTag);

	void RefreshFromState();

	UPROPERTY(Transient)
	TObjectPtr<UAC_CharacterSelectComponent> LocalComponent;
};
