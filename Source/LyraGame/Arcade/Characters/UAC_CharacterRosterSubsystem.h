// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameplayTagContainer.h"
#include "Subsystems/GameInstanceSubsystem.h"

#include "UAC_CharacterRosterSubsystem.generated.h"

/**
 * UAC_CharacterRosterSubsystem
 *
 * Server-side carrier for each player's chosen co-op role (ArcadeRoleTags::Role_Slick /
 * Role_Flint) across the ServerTravel from the character-select map into L_Arcade. Keyed by the
 * PlayerState's stable unique-id string, which survives a (non-seamless) travel because the net
 * connection persists.
 *
 * Lives on the GameInstance (server), so it is untouched by the map change. If a controller has
 * no recorded role in L_Arcade (straight PIE launch, late joiner), the co-op game mode falls back
 * to Lyra's experience default.
 */
UCLASS()
class LYRAGAME_API UAC_CharacterRosterSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	static UAC_CharacterRosterSubsystem* Get(const UObject* worldContext);

	/** Record a player's chosen role, keyed by PlayerState unique-id string. */
	void SetRole(const FString& playerKey, const FGameplayTag& role);

	/** Role for a player key, or an invalid tag if none recorded. */
	FGameplayTag GetRole(const FString& playerKey) const;

	/** Clear all recorded roles (call when returning to the select screen). */
	void ResetRoster();

private:
	UPROPERTY(Transient)
	TMap<FString, FGameplayTag> Roles;
};
