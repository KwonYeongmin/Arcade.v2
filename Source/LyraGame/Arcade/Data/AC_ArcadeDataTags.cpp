// Copyright Epic Games, Inc. All Rights Reserved.

#include "AC_ArcadeDataTags.h"

namespace ArcadeDataTags
{
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Table_Enemies, "ArcadeData.Table.Enemies", "DataTable of per-enemy stats (health, speed, orbit radius, contact damage, slick resistance, spawn weight).");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Table_WaveSpawns, "ArcadeData.Table.WaveSpawns", "DataTable of wave / spawn definitions per phase.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Table_Phases, "ArcadeData.Table.Phases", "DataTable of game-phase config (duration, BGM cue, spawn table, next phase).");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Table_Combat, "ArcadeData.Table.Combat", "DataTable of damage tuning, one row per damage source (Slick.Explosion, Flint.Direct, ...).");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Table_CharacterCards, "ArcadeData.Table.CharacterCards", "DataTable of per-operative presentation data (name, tagline, portrait, weapon icon), keyed by role.");
}
