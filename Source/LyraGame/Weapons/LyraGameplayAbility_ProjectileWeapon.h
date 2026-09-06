// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "LyraGameplayAbility_RangedWeapon.h"

#include "LyraGameplayAbility_ProjectileWeapon.generated.h"

class AAC_Projectile;

/**
 * ULyraGameplayAbility_ProjectileWeapon
 *
 * A ranged weapon ability that fires a visible-travel projectile instead of applying
 * instant hitscan damage. Reuses the base class's activation / ammo / spread / montage
 * and its camera-to-aim targeting, then — per resolved bullet — spawns an AAC_Projectile
 * at the weapon muzzle aimed at the resolved aim point. The projectile owns travel,
 * collision and damage; this ability does not apply hit effects itself.
 */
UCLASS()
class ULyraGameplayAbility_ProjectileWeapon : public ULyraGameplayAbility_RangedWeapon
{
	GENERATED_BODY()

protected:
	/** Projectile spawned per resolved bullet. Set on the weapon's fire ability. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile Weapon")
	TSubclassOf<AAC_Projectile> ProjectileClass;

	/** Socket on the spawned weapon mesh used as the muzzle. Falls back to the targeting source location. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile Weapon")
	FName MuzzleSocketName = FName(TEXT("Muzzle"));

	//~ULyraGameplayAbility_RangedWeapon
	virtual void OnRangedWeaponTargetDataReady_Implementation(const FGameplayAbilityTargetDataHandle& TargetData) override;
	//~End

private:
	/** World-space muzzle location: MuzzleSocketName on the first spawned weapon mesh, else the targeting source. */
	FVector ResolveMuzzleLocation() const;

	/** First actor spawned by the equipped weapon instance (the visible weapon), or null. */
	AActor* GetSpawnedWeaponActor() const;
};
