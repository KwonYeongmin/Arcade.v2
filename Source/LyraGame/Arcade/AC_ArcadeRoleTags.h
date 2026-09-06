// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NativeGameplayTags.h"

/**
 * Co-op character roles. In the fixed-role demo, local player 0 is Slick and local player 1 is
 * Flint; UAC_CharacterRosterSubsystem maps a local player index to one of these, and
 * AAC_ArcadeCoopGameMode turns the role into the matching PawnData.
 */
namespace ArcadeRoleTags
{
	LYRAGAME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Role_Slick);
	LYRAGAME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Role_Flint);
}
