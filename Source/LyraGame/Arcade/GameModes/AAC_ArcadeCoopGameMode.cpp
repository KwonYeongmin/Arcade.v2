// Copyright Epic Games, Inc. All Rights Reserved.

#include "AAC_ArcadeCoopGameMode.h"

#include "Arcade/AC_ArcadeRoleTags.h"
#include "Arcade/Characters/UAC_CharacterRosterSubsystem.h"
#include "Arcade/Teams/UAC_ArcadeTeamCreation.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AAC_ArcadeCoopGameMode)

void AAC_ArcadeCoopGameMode::InitGameState()
{
	Super::InitGameState();

	// Lyra assigns teams through a GameState component supplied by the experience. The Arcade
	// experience has none, so add the co-op team setup here — no experience edit required.
	if (AGameStateBase* gameState = GameState)
	{
		if (nullptr == gameState->FindComponentByClass<ULyraTeamCreationComponent>())
		{
			UAC_ArcadeTeamCreation* teamCreation =
				NewObject<UAC_ArcadeTeamCreation>(gameState, TEXT("ArcadeTeamCreation"));
			teamCreation->RegisterComponent();
		}
	}
}

const ULyraPawnData* AAC_ArcadeCoopGameMode::GetPawnDataForController(const AController* inController) const
{
	// Role was chosen on the character-select map and stashed on the GameInstance roster,
	// keyed by the player's stable unique-id string (survives the ServerTravel here).
	const APlayerController* playerController = Cast<APlayerController>(inController);
	const APlayerState* playerState = playerController ? playerController->PlayerState : nullptr;
	if (playerState)
	{
		if (const UAC_CharacterRosterSubsystem* roster = UAC_CharacterRosterSubsystem::Get(this))
		{
			const FGameplayTag role = roster->GetRole(playerState->GetUniqueId().ToString());
			if (role == ArcadeRoleTags::Role_Flint && FlintPawnData)
			{
				return FlintPawnData;
			}
			if (role == ArcadeRoleTags::Role_Slick && SlickPawnData)
			{
				return SlickPawnData;
			}
		}
	}

	return Super::GetPawnDataForController(inController);
}
