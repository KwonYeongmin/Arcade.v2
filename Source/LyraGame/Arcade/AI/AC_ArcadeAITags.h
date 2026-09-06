// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NativeGameplayTags.h"

/**
 * Generic enemy ability slots. Each non-boss enemy's AbilitySet maps Primary / Secondary to two
 * concrete UAC_GameplayAbility_EnemyAttack instances, so the behaviour tree stays enemy-agnostic:
 * it activates by slot tag and gates on the per-pawn cooldown.
 */
namespace ArcadeAbilityTags
{
	LYRAGAME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Enemy_Primary);
	LYRAGAME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Enemy_Secondary);
}
