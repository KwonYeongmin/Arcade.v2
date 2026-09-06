// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AbilitySystem/Abilities/LyraGameplayAbility.h"

#include "LyraGameplayAbility_MoverADS.generated.h"

class UInputMappingContext;

/** ADS ability for APawn-based Mover avatars without CharacterMovement dependencies. */
UCLASS()
class LYRAGAME_API ULyraGameplayAbility_MoverADS : public ULyraGameplayAbility
{
	GENERATED_BODY()

public:
	ULyraGameplayAbility_MoverADS(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	virtual void InputReleased(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo) override;

	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility,
		bool bWasCancelled) override;

private:
	FGameplayTag ActiveADSTag;

	UPROPERTY(Transient)
	TObjectPtr<const UInputMappingContext> ActiveADSInputMapping;
};
