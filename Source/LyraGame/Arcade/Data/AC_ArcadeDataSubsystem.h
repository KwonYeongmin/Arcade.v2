// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/DataTable.h"
#include "GameplayTagContainer.h"
#include "Subsystems/GameInstanceSubsystem.h"

#include "AC_ArcadeDataSubsystem.generated.h"

class UAC_ArcadeDataRegistry;

/**
 * UAC_ArcadeDataSubsystem
 *
 * Read-only access to the Arcade DataTables. The registry (UAC_ArcadeDataRegistry, assigned in
 * Project Settings > Game > Arcade Data) is loaded once at GameInstance init; query rows by
 * (table tag, row name).
 *
 * Use this for tabular, repeated-shape, bulk-tuned data — enemy stats, wave / spawn tables,
 * phase config. Single-instance tuning (an explosion's radius, a GE's duration, one weapon's
 * damage number) stays on the owning Blueprint's Class Defaults, not in a DataTable.
 *
 * Usage:
 *   if (const UAC_ArcadeDataSubsystem* data = UAC_ArcadeDataSubsystem::Get(this))
 *   {
 *       if (const FAC_EnemyStatRow* row = data->FindRow<FAC_EnemyStatRow>(ArcadeDataTags::Table_Enemies, TEXT("Gnat")))
 *       {
 *           MaxHealth = row->MaxHealth;
 *       }
 *   }
 */
UCLASS()
class LYRAGAME_API UAC_ArcadeDataSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	//~UGameInstanceSubsystem
	virtual void Initialize(FSubsystemCollectionBase& collection) override;
	virtual void Deinitialize() override;
	//~End

	/** GameInstance-scoped subsystem accessor. Null outside a running game/PIE world. */
	static UAC_ArcadeDataSubsystem* Get(const UObject* worldContext);

	/** DataTable registered under tableTag in the registry, or null. */
	UFUNCTION(BlueprintCallable, Category = "Arcade Data", meta = (AutoCreateRefTerm = "tableTag"))
	UDataTable* GetDataTable(const FGameplayTag& tableTag) const;

	/** True once the registry asset has loaded. */
	UFUNCTION(BlueprintCallable, Category = "Arcade Data")
	bool IsReady() const { return Registry != nullptr; }

	/**
	 * Typed row lookup. Returns a const pointer into the DataTable's own memory (do not store
	 * long-term, do not mutate), or null if the table or row is missing.
	 */
	template <typename TRow>
	const TRow* FindRow(const FGameplayTag& tableTag, FName rowName) const;

private:
	UPROPERTY(Transient)
	TObjectPtr<UAC_ArcadeDataRegistry> Registry;
};

template <typename TRow>
const TRow* UAC_ArcadeDataSubsystem::FindRow(const FGameplayTag& tableTag, FName rowName) const
{
	static_assert(std::is_base_of_v<FTableRowBase, TRow>, "TRow must derive from FTableRowBase");

	const UDataTable* table = GetDataTable(tableTag);
	if (nullptr == table)
	{
		return nullptr;
	}
	return table->FindRow<TRow>(rowName, TEXT("UAC_ArcadeDataSubsystem::FindRow"), /*bWarnIfRowMissing=*/false);
}
