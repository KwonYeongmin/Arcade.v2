// Copyright Epic Games, Inc. All Rights Reserved.

#include "AC_ArcadeDataSubsystem.h"

#include "AC_ArcadeDataRegistry.h"
#include "AC_ArcadeDataSettings.h"
#include "Engine/DataTable.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AC_ArcadeDataSubsystem)

void UAC_ArcadeDataSubsystem::Initialize(FSubsystemCollectionBase& collection)
{
	Super::Initialize(collection);

	const TSoftObjectPtr<UAC_ArcadeDataRegistry>& registryPtr = GetDefault<UAC_ArcadeDataSettings>()->Registry;
	if (registryPtr.IsNull())
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[ArcadeData] No registry assigned — set one in Project Settings > Game > Arcade Data."));
		return;
	}

	// The registry is a tiny asset (a tag -> DataTable map). Synchronous load at GameInstance
	// init is fine; the DataTables it references cook alongside it.
	Registry = registryPtr.LoadSynchronous();
	if (nullptr == Registry)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ArcadeData] Failed to load registry '%s'."), *registryPtr.ToString());
	}
}

void UAC_ArcadeDataSubsystem::Deinitialize()
{
	Registry = nullptr;
	Super::Deinitialize();
}

UAC_ArcadeDataSubsystem* UAC_ArcadeDataSubsystem::Get(const UObject* worldContext)
{
	if (nullptr == worldContext || nullptr == GEngine)
	{
		return nullptr;
	}

	const UWorld* world = GEngine->GetWorldFromContextObject(worldContext, EGetWorldErrorMode::ReturnNull);
	const UGameInstance* gameInstance = world ? world->GetGameInstance() : nullptr;
	return gameInstance ? gameInstance->GetSubsystem<UAC_ArcadeDataSubsystem>() : nullptr;
}

UDataTable* UAC_ArcadeDataSubsystem::GetDataTable(const FGameplayTag& tableTag) const
{
	if (nullptr == Registry)
	{
		return nullptr;
	}

	const TObjectPtr<UDataTable>* found = Registry->DataTables.Find(tableTag);
	return found ? found->Get() : nullptr;
}
