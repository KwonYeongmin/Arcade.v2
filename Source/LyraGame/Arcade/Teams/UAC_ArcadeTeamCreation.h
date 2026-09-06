// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Teams/LyraTeamCreationComponent.h"

#include "UAC_ArcadeTeamCreation.generated.h"

class ALyraPlayerState;

/**
 * UAC_ArcadeTeamCreation
 *
 * Co-op team setup: creates team 1 (players) and team 2 (enemies), and puts every non-spectator
 * player on team 1 instead of round-robin splitting them. Enemies assign themselves team 2 when
 * they spawn.
 *
 * AAC_ArcadeCoopGameMode adds this to the game state in InitGameState, so no experience edit is
 * needed — just reparent the project game mode onto AAC_ArcadeCoopGameMode.
 */
UCLASS()
class LYRAGAME_API UAC_ArcadeTeamCreation : public ULyraTeamCreationComponent
{
	GENERATED_BODY()

public:
	UAC_ArcadeTeamCreation();

	/** Team every player is placed on. Enemies use PlayerTeamId + 1. */
	static constexpr int32 PlayerTeamId = 1;
	static constexpr int32 EnemyTeamId = 2;

#if WITH_SERVER_CODE
protected:
	virtual void ServerChooseTeamForPlayer(ALyraPlayerState* PS) override;
#endif
};
