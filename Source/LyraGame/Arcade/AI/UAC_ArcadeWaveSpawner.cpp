// Copyright Epic Games, Inc. All Rights Reserved.

#include "UAC_ArcadeWaveSpawner.h"

#include "Arcade/Data/AC_ArcadeDataRows.h"
#include "Arcade/Data/AC_ArcadeDataSubsystem.h"
#include "Arcade/Data/AC_ArcadeDataTags.h"
#include "Character/LyraHealthComponent.h"
#include "EngineUtils.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "Engine/World.h"
#include "NavigationSystem.h"
#include "TimerManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(UAC_ArcadeWaveSpawner)

UAC_ArcadeWaveSpawner::UAC_ArcadeWaveSpawner(const FObjectInitializer& objectInitializer)
	: Super(objectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UAC_ArcadeWaveSpawner::BeginPlay()
{
	Super::BeginPlay();

	if (bAutoStart && GetOwner() && GetOwner()->HasAuthority())
	{
		if (StartDelaySeconds > 0.0f)
		{
			FTimerHandle startDelay;
			GetWorld()->GetTimerManager().SetTimer(startDelay, this, &UAC_ArcadeWaveSpawner::StartWave, StartDelaySeconds, false);
		}
		else
		{
			StartWave();
		}
	}
}

void UAC_ArcadeWaveSpawner::EndPlay(const EEndPlayReason::Type endPlayReason)
{
	if (UWorld* world = GetWorld())
	{
		world->GetTimerManager().ClearTimer(SpawnTimer);
	}
	Super::EndPlay(endPlayReason);
}

void UAC_ArcadeWaveSpawner::StartWave()
{
	if (nullptr == GetOwner() || false == GetOwner()->HasAuthority())
	{
		return;
	}

	const UAC_ArcadeDataSubsystem* data = UAC_ArcadeDataSubsystem::Get(this);
	const FAC_WaveSpawnRow* row = data ? data->FindRow<FAC_WaveSpawnRow>(ArcadeDataTags::Table_WaveSpawns, WaveRowName) : nullptr;
	if (nullptr == row)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ArcadeWave] no DT_WaveSpawns row '%s'"), *WaveRowName.ToString());
		return;
	}

	SpawnQueue.Reset();
	for (const FAC_WaveEnemyEntry& entry : row->Enemies)
	{
		for (int32 i = 0; i < FMath::Max(entry.Count, 0); ++i)
		{
			SpawnQueue.Add(entry.EnemyRowName);
		}
	}
	SpawnInterval = FMath::Max(row->SpawnInterval, 0.1f);
	MaxConcurrent = FMath::Max(row->MaxConcurrent, 1);
	bWaveRunning = SpawnQueue.Num() > 0;

	if (bWaveRunning)
	{
		GetWorld()->GetTimerManager().SetTimer(SpawnTimer, this, &UAC_ArcadeWaveSpawner::SpawnTick, SpawnInterval, true, 0.0f);
	}
}

int32 UAC_ArcadeWaveSpawner::GetRemainingCount() const
{
	return SpawnQueue.Num() + AliveEnemies.Num();
}

void UAC_ArcadeWaveSpawner::SpawnTick()
{
	AliveEnemies.RemoveAll([](const AActor* enemy) { return nullptr == enemy || false == IsValid(enemy); });

	if (SpawnQueue.Num() == 0)
	{
		GetWorld()->GetTimerManager().ClearTimer(SpawnTimer);
		CheckWaveCleared();
		return;
	}

	if (AliveEnemies.Num() >= MaxConcurrent)
	{
		return;
	}

	const FName next = SpawnQueue[0];
	SpawnQueue.RemoveAt(0);
	SpawnOne(next);
}

void UAC_ArcadeWaveSpawner::SpawnOne(FName enemyRowName)
{
	UWorld* world = GetWorld();
	const UAC_ArcadeDataSubsystem* data = UAC_ArcadeDataSubsystem::Get(this);
	const FAC_EnemyStatRow* row = data ? data->FindRow<FAC_EnemyStatRow>(ArcadeDataTags::Table_Enemies, enemyRowName) : nullptr;
	if (nullptr == world || nullptr == row)
	{
		return;
	}

	UClass* enemyClass = row->EnemyClass.LoadSynchronous();
	if (nullptr == enemyClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ArcadeWave] enemy row '%s' has no EnemyClass"), *enemyRowName.ToString());
		return;
	}

	const FTransform spawnTransform = PickSpawnTransform();

	FActorSpawnParameters spawnParams;
	spawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	AActor* enemy = world->SpawnActor<AActor>(enemyClass, spawnTransform, spawnParams);
	if (nullptr == enemy)
	{
		return;
	}

	AliveEnemies.Add(enemy);

	if (ULyraHealthComponent* health = ULyraHealthComponent::FindHealthComponent(enemy))
	{
		health->OnDeathStarted.AddDynamic(this, &UAC_ArcadeWaveSpawner::OnEnemyDeathStarted);
	}
}

FTransform UAC_ArcadeWaveSpawner::PickSpawnTransform() const
{
	const UWorld* world = GetWorld();

	// 1) tagged spawn-point actors
	TArray<AActor*> points;
	for (TActorIterator<AActor> it(const_cast<UWorld*>(world)); it; ++it)
	{
		if (it->ActorHasTag(SpawnPointActorTag))
		{
			points.Add(*it);
		}
	}
	if (points.Num() > 0)
	{
		return points[FMath::RandHelper(points.Num())]->GetActorTransform();
	}

	// 2) ring around the average player position
	FVector centre = FVector::ZeroVector;
	int32 playerCount = 0;
	if (const AGameStateBase* gameState = world->GetGameState())
	{
		for (const APlayerState* playerState : gameState->PlayerArray)
		{
			if (const APawn* pawn = playerState ? playerState->GetPawn() : nullptr)
			{
				centre += pawn->GetActorLocation();
				++playerCount;
			}
		}
	}
	if (playerCount > 0)
	{
		centre /= playerCount;
	}

	const float angle = FMath::FRandRange(0.0f, 2.0f * PI);
	FVector spawnLocation = centre + FVector(FMath::Cos(angle), FMath::Sin(angle), 0.0f) * FallbackSpawnRingRadius;
	spawnLocation.Z = centre.Z + 200.0f;

	if (const UNavigationSystemV1* nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(const_cast<UWorld*>(world)))
	{
		FNavLocation projected;
		if (nav->ProjectPointToNavigation(spawnLocation, projected, FVector(300.0f, 300.0f, 500.0f)))
		{
			spawnLocation = projected.Location + FVector(0.0f, 0.0f, 90.0f);
		}
	}

	return FTransform((centre - spawnLocation).GetSafeNormal2D().Rotation(), spawnLocation);
}

void UAC_ArcadeWaveSpawner::OnEnemyDeathStarted(AActor* owningActor)
{
	HandleEnemyDeath(owningActor);
}

void UAC_ArcadeWaveSpawner::HandleEnemyDeath(AActor* deadEnemy)
{
	AliveEnemies.Remove(deadEnemy);
	CheckWaveCleared();
}

void UAC_ArcadeWaveSpawner::CheckWaveCleared()
{
	if (false == bWaveRunning)
	{
		return;
	}

	AliveEnemies.RemoveAll([](const AActor* enemy) { return nullptr == enemy || false == IsValid(enemy); });

	if (SpawnQueue.Num() == 0 && AliveEnemies.Num() == 0)
	{
		bWaveRunning = false;
		OnWaveCleared.Broadcast();
	}
}
