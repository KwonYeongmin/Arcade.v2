// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/DataTable.h"
#include "GameplayTagContainer.h"

#include "AC_ArcadeDataRows.generated.h"

class AActor;
class UCameraShakeBase;
class USoundBase;
class UTexture2D;

/**
 * FAC_CharacterCardRow — one row of DT_CharacterCards (keyed under ArcadeData.Table.CharacterCards).
 *
 * Presentation data for one playable operative, reused anywhere a character is shown: the
 * character-select cards, roster screens, HUD nameplates. Keyed by RoleTag so a card widget can
 * look itself up from just its assigned role.
 */
USTRUCT(BlueprintType)
struct FAC_CharacterCardRow : public FTableRowBase
{
	GENERATED_BODY()

	/** Which co-op role this card represents (Arcade.Role.Slick / Arcade.Role.Flint). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Character", meta = (Categories = "Arcade.Role"))
	FGameplayTag RoleTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Character")
	FText DisplayName;

	/** Short one-line descriptor, e.g. "유체 사수". */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Character")
	FText Tagline;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Character")
	TSoftObjectPtr<UTexture2D> Portrait;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Character")
	TSoftObjectPtr<UTexture2D> WeaponIcon;

	/** Accent colour for the card (selection highlight, name text, etc.). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Character")
	FLinearColor AccentColor = FLinearColor(0.30f, 0.85f, 1.0f, 1.0f);
};

/**
 * FAC_EnemyStatRow — one row of DT_Enemies (keyed under ArcadeData.Table.Enemies).
 *
 * Per-enemy tuning read at spawn time by the enemy pawn / spawner. Movement fields below the
 * common ones only matter for the flying orbit-strafe behaviour; a pure ground enemy ignores
 * OrbitRadius / OrbitHeight.
 */
USTRUCT(BlueprintType)
struct FAC_EnemyStatRow : public FTableRowBase
{
	GENERATED_BODY()

	/** Pawn Blueprint to spawn for this enemy. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy")
	TSoftClassPtr<AActor> EnemyClass;

	/** Identity tag, e.g. "Arcade.Enemy.Gnat" — handy for UI / score / kill tracking. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy")
	FGameplayTag EnemyTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy", meta = (ClampMin = "1.0"))
	float MaxHealth = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy", meta = (ClampMin = "0.0"))
	float MoveSpeed = 600.0f;

	/** Contact / touch damage dealt to a player on overlap. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy", meta = (ClampMin = "0.0"))
	float ContactDamage = 10.0f;

	/**
	 * 0..1 multiplier that shrinks how much a Slick coat slows this enemy (0 = fully affected,
	 * 1 = immune). Applied on top of UAC_SlickReactionComponent's normal scaling.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float SlickResistance = 0.0f;

	/** Relative likelihood when a wave picks enemies at random. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy", meta = (ClampMin = "0.0"))
	float SpawnWeight = 1.0f;

	/** True for flying orbit-strafers (uses OrbitRadius / OrbitHeight); false for ground enemies. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Flying")
	bool bFlying = false;

	/** Preferred stand-off distance from the target while circling. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Flying", meta = (EditCondition = "bFlying", ClampMin = "0.0"))
	float OrbitRadius = 400.0f;

	/** Altitude above the target held while circling. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Flying", meta = (EditCondition = "bFlying", ClampMin = "0.0"))
	float OrbitHeight = 300.0f;
};

/**
 * FAC_DamageConfigRow — one row of DT_Combat (keyed under ArcadeData.Table.Combat).
 *
 * One row per damage source ("Slick.Explosion", "Flint.Direct", ...). The consumer looks its
 * row up by name and takes whichever fields it cares about; a missing row or an unready
 * subsystem leaves the consumer's own Class Default values in place.
 *
 *   final explosion damage = BaseDamage + PerSlickDamage * min(slickAmount, MaxSlickAmount)
 *   Flint direct hit       = BaseDamage   (the other fields are ignored)
 */
USTRUCT(BlueprintType)
struct FAC_DamageConfigRow : public FTableRowBase
{
	GENERATED_BODY()

	/** Flat damage before any Slick scaling. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Damage", meta = (ClampMin = "0.0"))
	float BaseDamage = 0.0f;

	/** Extra damage per unit of Slick amount on the target (explosion only; 0 = no scaling). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Damage", meta = (ClampMin = "0.0"))
	float PerSlickDamage = 0.0f;

	/** Slick amount is clamped to this before the PerSlickDamage multiply (explosion only). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Damage", meta = (ClampMin = "0.0"))
	float MaxSlickAmount = 6.0f;

	/** Blast radius in cm; 0 = keep the actor's own default (explosion only). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Damage", meta = (ClampMin = "0.0"))
	float Radius = 0.0f;

	/** Scale damage down toward the blast edge (explosion only). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Damage")
	bool bFalloff = true;

	/** Damage multiplier at the very edge of the radius when bFalloff is true (0..1). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Damage", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float FalloffFloor = 0.4f;

	/** Camera shake played on every viewer when this detonates (explosion only; unset = none). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Damage|Feel")
	TSoftClassPtr<UCameraShakeBase> CameraShake;

	/** Shake is full strength within this cm of the blast, zero past CameraShakeOuterRadius. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Damage|Feel", meta = (ClampMin = "0.0"))
	float CameraShakeInnerRadius = 200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Damage|Feel", meta = (ClampMin = "0.0"))
	float CameraShakeOuterRadius = 1500.0f;

	/** Global time-dilation applied for the hit-stop (1 = no hit-stop). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Damage|Feel", meta = (ClampMin = "0.01", ClampMax = "1.0"))
	float HitStopTimeScale = 1.0f;

	/** Real-time duration of the hit-stop. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Damage|Feel", meta = (ClampMin = "0.0"))
	float HitStopSeconds = 0.0f;

	/** Scale handed to the OnExploded Blueprint event for sizing the Niagara burst. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Damage|Feel", meta = (ClampMin = "0.0"))
	float FXScale = 1.0f;
};

/**
 * FAC_WaveEnemyEntry — one line item inside a wave: "spawn Count of the enemy in row EnemyRowName".
 * EnemyRowName is a row of DT_Enemies.
 */
USTRUCT(BlueprintType)
struct FAC_WaveEnemyEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wave")
	FName EnemyRowName = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wave", meta = (ClampMin = "1"))
	int32 Count = 1;
};

/**
 * FAC_WaveSpawnRow — one row of DT_WaveSpawns (keyed under ArcadeData.Table.WaveSpawns).
 *
 * The full spawn list for one wave. A phase points at one of these rows by name.
 */
USTRUCT(BlueprintType)
struct FAC_WaveSpawnRow : public FTableRowBase
{
	GENERATED_BODY()

	/** Every enemy this wave spawns, as (enemy row, count) pairs. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wave", meta = (TitleProperty = "EnemyRowName"))
	TArray<FAC_WaveEnemyEntry> Enemies;

	/** Seconds between individual spawns while the wave drains its list. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wave", meta = (ClampMin = "0.0"))
	float SpawnInterval = 1.0f;

	/** Cap on enemies alive at once; the spawner waits under this before adding more. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wave", meta = (ClampMin = "1"))
	int32 MaxConcurrent = 6;
};

/**
 * FAC_PhaseRow — one row of DT_Phases (keyed under ArcadeData.Table.Phases).
 *
 * Config for a single GamePhase. The phase advances when its wave is cleared, or when
 * TimeLimit elapses (if > 0), whichever comes first.
 */
USTRUCT(BlueprintType)
struct FAC_PhaseRow : public FTableRowBase
{
	GENERATED_BODY()

	/** The GamePhase tag this row configures, e.g. "GamePhase.Playing.Wave1". */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Phase")
	FGameplayTag PhaseTag;

	/** Row of DT_WaveSpawns to run during this phase; NAME_None for a non-combat phase. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Phase")
	FName WaveRowName = NAME_None;

	/** Phase to transition to once this one ends. Invalid tag = end of the run. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Phase")
	FGameplayTag NextPhaseTag;

	/** Auto-advance after this many seconds; 0 = only advance on wave clear / explicit trigger. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Phase", meta = (ClampMin = "0.0"))
	float TimeLimit = 0.0f;

	/** Music bed for the phase. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Phase")
	TSoftObjectPtr<USoundBase> Bgm;
};
