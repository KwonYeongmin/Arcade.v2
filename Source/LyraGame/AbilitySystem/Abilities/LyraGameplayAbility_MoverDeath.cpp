// Copyright Epic Games, Inc. All Rights Reserved.

#include "AbilitySystem/Abilities/LyraGameplayAbility_MoverDeath.h"

#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "TimerManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LyraGameplayAbility_MoverDeath)

void ULyraGameplayAbility_MoverDeath::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	if (DeathToDestroySeconds > 0.0f)
	{
		if (const AActor* avatar = GetAvatarActorFromActorInfo())
		{
			if (UWorld* world = avatar->GetWorld())
			{
				world->GetTimerManager().SetTimer(DestroyTimeoutHandle, this,
					&ULyraGameplayAbility_MoverDeath::EndDeathOnTimeout, DeathToDestroySeconds, false);
			}
		}
	}
}

void ULyraGameplayAbility_MoverDeath::EndAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	if (const AActor* avatar = GetAvatarActorFromActorInfo())
	{
		if (UWorld* world = avatar->GetWorld())
		{
			world->GetTimerManager().ClearTimer(DestroyTimeoutHandle);
		}
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void ULyraGameplayAbility_MoverDeath::EndDeathOnTimeout()
{
	if (true == IsActive())
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
	}
}
