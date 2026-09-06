// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AbilitySystem/Abilities/LyraGameplayAbility.h"
#include "LyraGameplayAbility_MoverReset.generated.h"

/** Reset event handler that targets AAC_CharacterBase rather than ALyraCharacter. */
UCLASS()
class LYRAGAME_API ULyraGameplayAbility_MoverReset : public ULyraGameplayAbility
{
	GENERATED_BODY()

public:
	ULyraGameplayAbility_MoverReset(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;
};
