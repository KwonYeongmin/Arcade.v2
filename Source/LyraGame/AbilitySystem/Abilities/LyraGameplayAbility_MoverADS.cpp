// Copyright Epic Games, Inc. All Rights Reserved.

#include "AbilitySystem/Abilities/LyraGameplayAbility_MoverADS.h"

#include "AbilitySystem/LyraAbilitySystemComponent.h"
#include "Camera/LyraCameraMode.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/PlayerController.h"
#include "InputMappingContext.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LyraGameplayAbility_MoverADS)

namespace
{
	constexpr TCHAR ADSCameraModePath[] = TEXT("/ShooterCore/Camera/CM_ThirdPersonADS.CM_ThirdPersonADS_C");
	constexpr TCHAR ADSInputMappingPath[] = TEXT("/ShooterCore/Input/Mappings/IMC_ADS_Speed.IMC_ADS_Speed");
}

ULyraGameplayAbility_MoverADS::ULyraGameplayAbility_MoverADS(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
}

void ULyraGameplayAbility_MoverADS::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
	ActiveADSTag = FGameplayTag::RequestGameplayTag(TEXT("Event.Movement.ADS"), false);
	if (ActiveADSTag.IsValid())
	{
		if (ULyraAbilitySystemComponent* LyraASC = GetLyraAbilitySystemComponentFromActorInfo())
		{
			LyraASC->AddLooseGameplayTag(ActiveADSTag);
		}
	}

	if (TSubclassOf<ULyraCameraMode> CameraMode = LoadClass<ULyraCameraMode>(nullptr, ADSCameraModePath))
	{
		SetCameraMode(CameraMode);
	}

	APlayerController* PlayerController = ActorInfo ? ActorInfo->PlayerController.Get() : nullptr;
	if (PlayerController && PlayerController->GetLocalPlayer())
	{
		ActiveADSInputMapping = LoadObject<UInputMappingContext>(nullptr, ADSInputMappingPath);
		if (ActiveADSInputMapping)
		{
			if (UEnhancedInputLocalPlayerSubsystem* InputSubsystem =
				PlayerController->GetLocalPlayer()->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
			{
				InputSubsystem->AddMappingContext(ActiveADSInputMapping, 1);
			}
		}
	}
}

void ULyraGameplayAbility_MoverADS::InputReleased(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo)
{
	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}

void ULyraGameplayAbility_MoverADS::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	APlayerController* PlayerController = ActorInfo ? ActorInfo->PlayerController.Get() : nullptr;
	if (ActiveADSInputMapping && PlayerController && PlayerController->GetLocalPlayer())
	{
		if (UEnhancedInputLocalPlayerSubsystem* InputSubsystem =
			PlayerController->GetLocalPlayer()->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
		{
			InputSubsystem->RemoveMappingContext(ActiveADSInputMapping);
		}
	}
	ActiveADSInputMapping = nullptr;
	if (ActiveADSTag.IsValid())
	{
		if (ULyraAbilitySystemComponent* LyraASC = GetLyraAbilitySystemComponentFromActorInfo())
		{
			LyraASC->RemoveLooseGameplayTag(ActiveADSTag);
		}
	}
	ActiveADSTag = FGameplayTag();
	ClearCameraMode();

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
