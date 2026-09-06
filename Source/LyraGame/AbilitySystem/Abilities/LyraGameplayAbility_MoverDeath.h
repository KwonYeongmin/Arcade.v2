// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AbilitySystem/Abilities/LyraGameplayAbility_Death.h"
#include "Engine/TimerHandle.h"
#include "LyraGameplayAbility_MoverDeath.generated.h"

/** Death flow for the UAF Mover pawn. AutoRespawn ends this ability after its respawn delay.
 *  As a safety net this ability also ends itself DeathToDestroySeconds after it starts, so the
 *  ragdolled pawn is always cleaned up (Super::EndAbility -> FinishDeath -> OnDeathFinished ->
 *  DestroyDueToDeath) even when nothing else ends it. */
UCLASS()
class LYRAGAME_API ULyraGameplayAbility_MoverDeath : public ULyraGameplayAbility_Death
{
	GENERATED_BODY()

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility,
		bool bWasCancelled) override;

private:
	void EndDeathOnTimeout();

	/** Seconds after the death starts before the ragdolled pawn is force-destroyed if nothing
	 *  else has ended this ability yet. Zero disables the safety net. */
	UPROPERTY(EditDefaultsOnly, Category = "Lyra|Death", meta = (ClampMin = "0.0"))
	float DeathToDestroySeconds = 4.0f;

	FTimerHandle DestroyTimeoutHandle;
};
