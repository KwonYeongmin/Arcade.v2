// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/GameStateComponent.h"
#include "GameplayTagContainer.h"

#include "UAC_ArcadeWaveSpawner.generated.h"

class AActor;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnArcadeWaveCleared);

/**
 * UAC_ArcadeWaveSpawner
 *
 * GameState component (server). Reads a FAC_WaveSpawnRow from DT_WaveSpawns and drips its enemies
 * into the level: each entry names a FAC_EnemyStatRow (whose EnemyClass is the pawn to spawn) and
 * a count; enemies come in every SpawnInterval while fewer than MaxConcurrent are alive. When the
 * queue is empty and every spawned enemy is dead the wave is cleared — OnWaveCleared fires and an
 * Arcade.Message.WaveCleared gameplay message is broadcast for the HUD / phase system.
 *
 * Add via the experience (GameFeatureAction_AddComponents -> LyraGameState). Drives itself on
 * BeginPlay while bAutoStart is true; a phase ability can call StartWave() instead.
 */
UCLASS(ClassGroup = (Arcade), meta = (BlueprintSpawnableComponent))
class LYRAGAME_API UAC_ArcadeWaveSpawner : public UGameStateComponent
{
	GENERATED_BODY()

public:
	UAC_ArcadeWaveSpawner(const FObjectInitializer& objectInitializer = FObjectInitializer::Get());

	UPROPERTY(BlueprintAssignable, Category = "Arcade Wave")
	FOnArcadeWaveCleared OnWaveCleared;

	/** Row of DT_WaveSpawns to run. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arcade Wave")
	FName WaveRowName;

	/** Start the wave automatically once the game state begins play (handy for solo testing). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arcade Wave")
	bool bAutoStart = true;

	/** Delay after BeginPlay before an auto-started wave begins (lets players finish loading in). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arcade Wave", meta = (ClampMin = "0.0", EditCondition = "bAutoStart"))
	float StartDelaySeconds = 3.0f;

	/** Enemies spawn at actors carrying this tag; if none exist, on a ring around the players. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arcade Wave")
	FName SpawnPointActorTag = TEXT("EnemySpawn");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arcade Wave", meta = (ClampMin = "200.0"))
	float FallbackSpawnRingRadius = 1500.0f;

	/** Begin (or restart) the configured wave. */
	UFUNCTION(BlueprintCallable, Category = "Arcade Wave")
	void StartWave();

	/** Kill-tracking: enemies remaining (queued + alive). */
	UFUNCTION(BlueprintPure, Category = "Arcade Wave")
	int32 GetRemainingCount() const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type endPlayReason) override;

private:
	void SpawnTick();
	void SpawnOne(FName enemyRowName);
	FTransform PickSpawnTransform() const;
	void HandleEnemyDeath(AActor* deadEnemy);
	void CheckWaveCleared();

	UFUNCTION()
	void OnEnemyDeathStarted(AActor* owningActor);

	UPROPERTY(Transient)
	TArray<FName> SpawnQueue;

	UPROPERTY(Transient)
	TArray<TObjectPtr<AActor>> AliveEnemies;

	float SpawnInterval = 1.0f;
	int32 MaxConcurrent = 6;
	bool bWaveRunning = false;

	FTimerHandle SpawnTimer;
};
