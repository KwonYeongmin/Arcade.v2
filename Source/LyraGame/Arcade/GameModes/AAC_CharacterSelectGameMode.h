// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ModularGameMode.h"

#include "AAC_CharacterSelectGameMode.generated.h"

class UAC_CharacterSelectWidget;

/**
 * AAC_CharacterSelectGameMode
 *
 * Server game mode for L_CharacterSelect. Gives every joining PlayerController a
 * UAC_CharacterSelectComponent (which shows the select widget on that client), and once every
 * connected player holds a distinct role and is ready, records the roles on the GameInstance
 * roster and ServerTravels to the gameplay level (connected clients follow).
 *
 * Networked listen-server co-op: there is no local-player / splitscreen path here.
 */
UCLASS()
class LYRAGAME_API AAC_CharacterSelectGameMode : public AModularGameModeBase
{
	GENERATED_BODY()

public:
	AAC_CharacterSelectGameMode();

	/** Called by a player's UAC_CharacterSelectComponent after it changes role / ready on the server. */
	void NotifySelectionChanged();

protected:
	virtual void PostLogin(APlayerController* newPlayer) override;

	/** Widget class each client's select component shows. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Character Select")
	TSubclassOf<UAC_CharacterSelectWidget> SelectWidgetClass;

	/** Level travelled to once everyone is ready. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Character Select")
	FString GameplayLevelPath = TEXT("/Arcade/Maps/L_Arcade");

private:
	bool EveryoneReadyWithDistinctRoles() const;
	void WriteRosterAndTravel();

	bool bTravelStarted = false;
};
