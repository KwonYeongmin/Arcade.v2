// Copyright Epic Games, Inc. All Rights Reserved.

#include "UAC_CharacterRosterSubsystem.h"

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(UAC_CharacterRosterSubsystem)

UAC_CharacterRosterSubsystem* UAC_CharacterRosterSubsystem::Get(const UObject* worldContext)
{
	if (nullptr == worldContext || nullptr == GEngine)
	{
		return nullptr;
	}

	const UWorld* world = GEngine->GetWorldFromContextObject(worldContext, EGetWorldErrorMode::ReturnNull);
	const UGameInstance* gameInstance = world ? world->GetGameInstance() : nullptr;
	return gameInstance ? gameInstance->GetSubsystem<UAC_CharacterRosterSubsystem>() : nullptr;
}

void UAC_CharacterRosterSubsystem::SetRole(const FString& playerKey, const FGameplayTag& role)
{
	if (false == playerKey.IsEmpty())
	{
		Roles.Add(playerKey, role);
	}
}

FGameplayTag UAC_CharacterRosterSubsystem::GetRole(const FString& playerKey) const
{
	if (const FGameplayTag* found = Roles.Find(playerKey))
	{
		return *found;
	}
	return FGameplayTag();
}

void UAC_CharacterRosterSubsystem::ResetRoster()
{
	Roles.Reset();
}
