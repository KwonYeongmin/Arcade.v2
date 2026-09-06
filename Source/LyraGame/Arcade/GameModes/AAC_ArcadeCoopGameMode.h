// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameModes/LyraGameMode.h"

#include "AAC_ArcadeCoopGameMode.generated.h"

class AController;
class ULyraPawnData;

/**
 * AAC_ArcadeCoopGameMode
 *
 * L_Arcade game mode for the 2-player co-op demo. Fixed roles: local player 0 spawns as Slick,
 * local player 1 as Flint. The role for a controller comes from UAC_CharacterRosterSubsystem
 * (populated by the character-select screen, with a 0->Slick / 1->Flint fallback); this class
 * only turns that role into the right PawnData.
 *
 * Reparent /Game/B_LyraGameMode onto this and assign SlickPawnData / FlintPawnData.
 */
UCLASS()
class LYRAGAME_API AAC_ArcadeCoopGameMode : public ALyraGameMode
{
	GENERATED_BODY()

public:
	//~ALyraGameMode
	virtual void InitGameState() override;
	virtual const ULyraPawnData* GetPawnDataForController(const AController* inController) const override;
	//~End

protected:
	/** PawnData for the Slick role (local player 0). Falls back to Super's lookup when unset. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arcade|Co-op")
	TObjectPtr<const ULyraPawnData> SlickPawnData;

	/** PawnData for the Flint role (local player 1). Falls back to Super's lookup when unset. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arcade|Co-op")
	TObjectPtr<const ULyraPawnData> FlintPawnData;
};
