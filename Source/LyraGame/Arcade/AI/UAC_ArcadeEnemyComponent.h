// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/ActorComponent.h"
#include "GameplayEffectTypes.h"
#include "GameplayTagContainer.h"

#include "UAC_ArcadeEnemyComponent.generated.h"

class ULyraAbilitySet;
class UGameplayEffect;
class UPrimitiveComponent;

/**
 * UAC_ArcadeEnemyComponent
 *
 * Turns a plain ALyraCharacterWithAbilities into an Arcade enemy: on the server it reads its
 * FAC_EnemyStatRow from DT_Enemies (health, speed, orbit radius / height, contact damage, flying
 * flag), pushes those onto the pawn (health attributes, movement speed), puts it on the enemy
 * team, and applies contact damage to players it touches. The behaviour-tree tasks read the
 * cached orbit / speed values back off this component.
 */
UCLASS(ClassGroup = (Arcade), meta = (BlueprintSpawnableComponent))
class LYRAGAME_API UAC_ArcadeEnemyComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UAC_ArcadeEnemyComponent();

	/** Row of DT_Enemies (ArcadeData.Table.Enemies) this enemy pulls its stats from. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Arcade Enemy")
	FName StatRowName = TEXT("Gnat");

	/** GE applied to a touched player (SetByCaller magnitude = ContactDamage). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arcade Enemy")
	TSubclassOf<UGameplayEffect> ContactDamageGameplayEffectClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arcade Enemy")
	FGameplayTag ContactDamageSetByCallerTag;

	/** Minimum seconds between contact-damage hits on the same target. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arcade Enemy", meta = (ClampMin = "0.1"))
	float ContactDamageInterval = 1.0f;

	//~Cached stats (valid after BeginPlay on the server; replicated defaults on clients until then)
	UFUNCTION(BlueprintPure, Category = "Arcade Enemy")
	float GetMoveSpeed() const { return MoveSpeed; }

	UFUNCTION(BlueprintPure, Category = "Arcade Enemy")
	float GetOrbitRadius() const { return OrbitRadius; }

	UFUNCTION(BlueprintPure, Category = "Arcade Enemy")
	float GetOrbitHeight() const { return OrbitHeight; }

	UFUNCTION(BlueprintPure, Category = "Arcade Enemy")
	bool IsFlyingEnemy() const { return bFlying; }

	/** Marks an ability slot (Ability.Enemy.Primary/Secondary) as on cooldown for `seconds`. */
	void StartAbilityCooldown(const FGameplayTag& abilityTag, float seconds);

	/** True if that ability slot's cooldown has elapsed (or was never used). */
	bool IsAbilityReady(const FGameplayTag& abilityTag) const;

	/** AbilitySet granting this enemy's two skills (Primary / Secondary). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arcade Enemy")
	TObjectPtr<ULyraAbilitySet> AbilitySet;

protected:
	virtual void BeginPlay() override;

private:
	UFUNCTION()
	void HandleCapsuleOverlap(UPrimitiveComponent* overlappedComp, AActor* otherActor,
		UPrimitiveComponent* otherComp, int32 otherBodyIndex, bool bFromSweep, const FHitResult& sweepResult);

	void ApplyStatRow();

	UPROPERTY(Replicated)
	float MoveSpeed = 600.0f;

	UPROPERTY(Replicated)
	float OrbitRadius = 400.0f;

	UPROPERTY(Replicated)
	float OrbitHeight = 300.0f;

	UPROPERTY(Replicated)
	bool bFlying = true;

	float ContactDamage = 10.0f;

	TMap<TWeakObjectPtr<AActor>, float> LastContactDamageTime;
	TMap<FGameplayTag, double> AbilityReadyAt;

public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
