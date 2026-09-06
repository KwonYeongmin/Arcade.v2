// Copyright Epic Games, Inc. All Rights Reserved.

#include "AbilitySystem/Abilities/LyraGameplayAbility_MoverReset.h"

#include "AbilitySystem/LyraAbilitySystemComponent.h"
#include "AbilitySystem/Abilities/LyraGameplayAbility_Reset.h"
#include "Character/Mover/AC_CharacterBase.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "LyraGameplayTags.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LyraGameplayAbility_MoverReset)

ULyraGameplayAbility_MoverReset::ULyraGameplayAbility_MoverReset(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerInitiated;
	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		FAbilityTriggerData TriggerData;
		TriggerData.TriggerTag = LyraGameplayTags::GameplayEvent_RequestReset;
		TriggerData.TriggerSource = EGameplayAbilityTriggerSource::GameplayEvent;
		AbilityTriggers.Add(TriggerData);
	}
}

void ULyraGameplayAbility_MoverReset::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	check(ActorInfo);
	ULyraAbilitySystemComponent* LyraASC = CastChecked<ULyraAbilitySystemComponent>(ActorInfo->AbilitySystemComponent.Get());
	FGameplayTagContainer AbilityTypesToIgnore;
	AbilityTypesToIgnore.AddTag(LyraGameplayTags::Ability_Behavior_SurvivesDeath);
	LyraASC->CancelAbilities(nullptr, &AbilityTypesToIgnore, this);
	SetCanBeCanceled(false);

	if (AAC_CharacterBase* MoverCharacter = Cast<AAC_CharacterBase>(ActorInfo->AvatarActor.Get()))
	{
		MoverCharacter->Reset();
	}

	FLyraPlayerResetMessage Message;
	Message.OwnerPlayerState = ActorInfo->OwnerActor.Get();
	UGameplayMessageSubsystem::Get(this).BroadcastMessage(LyraGameplayTags::GameplayEvent_Reset, Message);
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}
