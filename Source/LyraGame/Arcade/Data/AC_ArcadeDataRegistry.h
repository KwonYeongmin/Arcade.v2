// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"

#include "AC_ArcadeDataRegistry.generated.h"

class UDataTable;

/**
 * UAC_ArcadeDataRegistry
 *
 * One asset that maps a GameplayTag to the DataTable driving that slice of Arcade content
 * (enemy stats, wave / spawn tables, phase config). Assigned once in Project Settings and
 * read by UAC_ArcadeDataSubsystem — no ConstructorHelpers, no per-table C++.
 *
 * Single-instance tuning (explosion radius, GE durations, one weapon's damage) does NOT
 * belong here — keep it on the owning Blueprint's Class Defaults.
 */
UCLASS(BlueprintType)
class LYRAGAME_API UAC_ArcadeDataRegistry : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** Tag -> DataTable, e.g. "ArcadeData.Table.Enemies" -> DT_Enemies. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arcade Data",
		meta = (Categories = "ArcadeData.Table", ForceInlineRow))
	TMap<FGameplayTag, TObjectPtr<UDataTable>> DataTables;
};
