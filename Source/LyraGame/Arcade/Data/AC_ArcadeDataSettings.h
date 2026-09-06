// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/DeveloperSettings.h"

#include "AC_ArcadeDataSettings.generated.h"

class UAC_ArcadeDataRegistry;

/**
 * UAC_ArcadeDataSettings
 *
 * Project Settings > Game > Arcade Data. Points UAC_ArcadeDataSubsystem at its registry asset.
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Arcade Data"))
class LYRAGAME_API UAC_ArcadeDataSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetCategoryName() const override { return FName(TEXT("Game")); }

	/** Registry mapping table tags to DataTables. Loaded once at GameInstance init. */
	UPROPERTY(Config, EditAnywhere, Category = "Arcade Data", meta = (AllowedClasses = "/Script/LyraGame.AC_ArcadeDataRegistry"))
	TSoftObjectPtr<UAC_ArcadeDataRegistry> Registry;
};
