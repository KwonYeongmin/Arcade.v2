// Copyright Epic Games, Inc. All Rights Reserved.

#include "UAC_ArcadeTeamCreation.h"

#include "Player/LyraPlayerState.h"
#include "Teams/LyraTeamAgentInterface.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(UAC_ArcadeTeamCreation)

UAC_ArcadeTeamCreation::UAC_ArcadeTeamCreation()
{
	// Register both teams (display assets optional — left unset for the demo).
	TeamsToCreate.Add((uint8)PlayerTeamId, nullptr);
	TeamsToCreate.Add((uint8)EnemyTeamId, nullptr);
}

#if WITH_SERVER_CODE
void UAC_ArcadeTeamCreation::ServerChooseTeamForPlayer(ALyraPlayerState* PS)
{
	if (nullptr == PS)
	{
		return;
	}

	if (PS->IsOnlyASpectator())
	{
		PS->SetGenericTeamId(FGenericTeamId::NoTeam);
		return;
	}

	// Co-op: everyone on the player team, regardless of join order.
	PS->SetGenericTeamId(IntegerToGenericTeamId(PlayerTeamId));
}
#endif
