// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NativeGameplayTags.h"

/**
 * Native tags for keying UAC_ArcadeDataRegistry::DataTables. Add a new table = add a tag here
 * and an entry in the registry asset; no other C++ changes.
 */
namespace ArcadeDataTags
{
	LYRAGAME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Table_Enemies);
	LYRAGAME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Table_WaveSpawns);
	LYRAGAME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Table_Phases);
	LYRAGAME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Table_Combat);
	LYRAGAME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Table_CharacterCards);
}
