// Copyright Epic Games, Inc. All Rights Reserved.

#include "AbilitySystem/Abilities/LyraGameplayAbility_MoverJump.h"

#include "Character/Mover/AC_CharacterBase.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LyraGameplayAbility_MoverJump)

ULyraGameplayAbility_MoverJump::ULyraGameplayAbility_MoverJump(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
}

bool ULyraGameplayAbility_MoverJump::CanActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayTagContainer* SourceTags,
	const FGameplayTagContainer* TargetTags,
	FGameplayTagContainer* OptionalRelevantTags) const
{
	const AAC_CharacterBase* MoverPawn = ActorInfo
		? Cast<AAC_CharacterBase>(ActorInfo->AvatarActor.Get())
		: nullptr;

	return MoverPawn && MoverPawn->CanStartMoverJump()
		&& Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags);
}

void ULyraGameplayAbility_MoverJump::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	if (AAC_CharacterBase* MoverPawn = ActorInfo
		? Cast<AAC_CharacterBase>(ActorInfo->AvatarActor.Get())
		: nullptr)
	{
		MoverPawn->StartMoverJump();
		return;
	}

	EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
}

void ULyraGameplayAbility_MoverJump::InputReleased(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo)
{
	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}

void ULyraGameplayAbility_MoverJump::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	if (AAC_CharacterBase* MoverPawn = ActorInfo
		? Cast<AAC_CharacterBase>(ActorInfo->AvatarActor.Get())
		: nullptr)
	{
		MoverPawn->StopMoverJump();
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
