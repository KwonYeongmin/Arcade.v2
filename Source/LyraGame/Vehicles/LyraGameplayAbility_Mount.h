// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AbilitySystem/Abilities/LyraGameplayAbility.h"

#include "LyraGameplayAbility_Mount.generated.h"

/**
 * ULyraGameplayAbility_Mount
 *
 * 탈 것 탑승 어빌리티. Lyra 상호작용 시스템이 GameplayEvent 로 트리거한다.
 *
 * 트리거 경로 (ULyraGameplayAbility_Interact::TriggerInteraction):
 *   Payload.EventTag   = "Ability.Interaction.Activate"
 *   Payload.Instigator = 캐릭터 (라이더)
 *   Payload.Target     = 탈 것
 *
 * 따라서 BP 파생 클래스는 Triggers 배열에 GameplayEvent 트리거를 반드시 설정해야 한다.
 * 없으면 TriggerEventData 가 null 로 들어온다.
 */
UCLASS(Abstract)
class ULyraGameplayAbility_Mount : public ULyraGameplayAbility
{
    GENERATED_BODY()

public:
    ULyraGameplayAbility_Mount(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

    virtual void ActivateAbility(
        const FGameplayAbilitySpecHandle Handle,
        const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayAbilityActivationInfo ActivationInfo,
        const FGameplayEventData* TriggerEventData) override;
};
