// Copyright Epic Games, Inc. All Rights Reserved.

#include "AAC_CharacterSelectGameMode.h"

#include "Arcade/Characters/UAC_CharacterRosterSubsystem.h"
#include "Arcade/Player/UAC_CharacterSelectComponent.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/SpectatorPawn.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AAC_CharacterSelectGameMode)

namespace
{
	UAC_CharacterSelectComponent* SelectCompOf(const APlayerState* playerState)
	{
		return playerState ? playerState->FindComponentByClass<UAC_CharacterSelectComponent>() : nullptr;
	}
}

AAC_CharacterSelectGameMode::AAC_CharacterSelectGameMode()
{
	// Menu map — no gameplay pawn, just the fixed camera and each client's widget.
	DefaultPawnClass = ASpectatorPawn::StaticClass();
}

void AAC_CharacterSelectGameMode::PostLogin(APlayerController* newPlayer)
{
	Super::PostLogin(newPlayer);

	// Component lives on the PlayerState so it replicates to every client (each client can see
	// what all players picked).
	APlayerState* playerState = newPlayer ? newPlayer->PlayerState : nullptr;
	if (nullptr == playerState || playerState->FindComponentByClass<UAC_CharacterSelectComponent>())
	{
		return;
	}

	UAC_CharacterSelectComponent* selectComp = NewObject<UAC_CharacterSelectComponent>(playerState, TEXT("CharacterSelect"));
	selectComp->SelectWidgetClass = SelectWidgetClass;
	selectComp->RegisterComponent();
}

void AAC_CharacterSelectGameMode::NotifySelectionChanged()
{
	if (false == bTravelStarted && EveryoneReadyWithDistinctRoles())
	{
		bTravelStarted = true;
		WriteRosterAndTravel();
	}
}

bool AAC_CharacterSelectGameMode::EveryoneReadyWithDistinctRoles() const
{
	const AGameStateBase* gameState = GameState;
	if (nullptr == gameState || gameState->PlayerArray.Num() == 0)
	{
		return false;
	}

	TSet<FGameplayTag> seenRoles;
	for (const APlayerState* playerState : gameState->PlayerArray)
	{
		const UAC_CharacterSelectComponent* selectComp = SelectCompOf(playerState);
		if (nullptr == selectComp || false == selectComp->ChosenRole.IsValid() || false == selectComp->bReady)
		{
			return false;
		}
		seenRoles.Add(selectComp->ChosenRole);
	}

	return seenRoles.Num() == gameState->PlayerArray.Num();
}

void AAC_CharacterSelectGameMode::WriteRosterAndTravel()
{
	if (UAC_CharacterRosterSubsystem* roster = UAC_CharacterRosterSubsystem::Get(this))
	{
		roster->ResetRoster();
		for (const APlayerState* playerState : GameState->PlayerArray)
		{
			if (const UAC_CharacterSelectComponent* selectComp = SelectCompOf(playerState))
			{
				roster->SetRole(playerState->GetUniqueId().ToString(), selectComp->ChosenRole);
			}
		}
	}

	if (UWorld* world = GetWorld())
	{
		world->ServerTravel(GameplayLevelPath);
	}
}
