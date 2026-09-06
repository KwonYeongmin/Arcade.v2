// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AbilitySystem/Abilities/LyraGameplayAbility.h"
#include "GameplayTagContainer.h"

#include "UAC_GameplayAbility_EnemyAttack.generated.h"

class AActor;
class UAnimMontage;
class UGameplayEffect;

UENUM(BlueprintType)
enum class EAC_EnemyAttackShape : uint8
{
	/** Sphere check in front of the enemy. */
	Melee,
	/** Spawn ProjectileClass aimed at the target (falls back to a hitscan if unset). */
	Projectile,
	/** Sphere check centred on the enemy (a slam / stomp). */
	RadialAoE,
};

/**
 * UAC_GameplayAbility_EnemyAttack
 *
 * One generic, config-driven enemy skill. A BP child fills in the shape / range / damage / GE /
 * montage, and an AbilitySet grants it under Ability.Enemy.Primary or Ability.Enemy.Secondary so
 * the behaviour tree can fire it by slot. Target is read from the AI controller's blackboard
 * TargetActor key (nearest player).
 */
UCLASS()
class LYRAGAME_API UAC_GameplayAbility_EnemyAttack : public ULyraGameplayAbility
{
	GENERATED_BODY()

public:
	UAC_GameplayAbility_EnemyAttack(const FObjectInitializer& objectInitializer);

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle handle, const FGameplayAbilityActorInfo* actorInfo,
		const FGameplayAbilityActivationInfo activationInfo, const FGameplayEventData* triggerEventData) override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy Attack")
	EAC_EnemyAttackShape Shape = EAC_EnemyAttackShape::Melee;

	/** Melee: forward reach. Projectile: muzzle offset. RadialAoE: unused. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy Attack", meta = (ClampMin = "0.0"))
	float Range = 200.0f;

	/** Melee / RadialAoE hit sphere radius. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy Attack", meta = (ClampMin = "0.0"))
	float Radius = 200.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy Attack", meta = (ClampMin = "0.0"))
	float Damage = 15.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy Attack")
	TSubclassOf<UGameplayEffect> DamageGameplayEffectClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy Attack")
	FGameplayTag DamageSetByCallerTag;

	/** Projectile shape only. If unset, a hitscan trace to the target is used instead. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy Attack")
	TSubclassOf<AActor> ProjectileClass;

	/** Delay between activation and the hit landing (windup). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy Attack", meta = (ClampMin = "0.0"))
	float WindupSeconds = 0.25f;

	/** Optional montage played on activation (does not gate the hit — WindupSeconds does). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy Attack")
	TObjectPtr<UAnimMontage> Montage;

private:
	void ResolveHit();
	AActor* GetTargetActor() const;
	void ApplyDamageTo(AActor* target) const;

	FGameplayAbilitySpecHandle CachedHandle;
	FTimerHandle WindupTimer;
};
