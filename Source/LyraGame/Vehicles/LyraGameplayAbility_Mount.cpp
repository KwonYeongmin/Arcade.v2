// Copyright Epic Games, Inc. All Rights Reserved.

#include "Vehicles/LyraGameplayAbility_Mount.h"

#include "GameFramework/Controller.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Vehicles/AC_MotorcyclePawn.h"
#include "AbilitySystem/LyraAbilitySystemComponent.h"
#include "Character/Mover/AC_CharacterBase.h"
#include "Interaction/Abilities/LyraGameplayAbility_Interact.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LyraGameplayAbility_Mount)

ULyraGameplayAbility_Mount::ULyraGameplayAbility_Mount(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
}

void ULyraGameplayAbility_Mount::ActivateAbility(
    const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo,
    const FGameplayEventData* TriggerEventData)
{
    Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

    if (!TriggerEventData)
    {
        UE_LOG(LogTemp, Error, TEXT("[Vehicle] GA_Mount: TriggerEventData 없음 — BP 의 Triggers 에 GameplayEvent(Ability.Interaction.Activate) 를 설정했는지 확인"));
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    APawn* RiderPawn = Cast<APawn>(const_cast<AActor*>(ToRawPtr(TriggerEventData->Instigator)));
    AAC_MotorcyclePawn* Bike = Cast<AAC_MotorcyclePawn>(const_cast<AActor*>(ToRawPtr(TriggerEventData->Target)));

    if (!RiderPawn || !Bike)
    {
        UE_LOG(LogTemp, Error, TEXT("[Vehicle] GA_Mount: Rider=%s Bike=%s — 캐스팅 실패"),
            *GetNameSafe(RiderPawn), *GetNameSafe(Bike));
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    AController* PC = RiderPawn->GetController();
    if (!PC)
    {
        UE_LOG(LogTemp, Error, TEXT("[Vehicle] GA_Mount: Controller 없음"));
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    // Possess 보다 먼저 붙인다. Possess 가 라이더의 컨트롤러를 떼면서
    // 이동 상태를 건드릴 수 있으므로 이동을 먼저 정지시켜 둔다.
    Bike->AttachRider(RiderPawn);

    // 라이더가 오토바이가 아닌 ALyraCharacter 였다면 ULyraHeroComponent 가 폰이 바뀔 때마다
    // ASC 의 InitAbilityActorInfo 를 새 아바타로 다시 불러준다. Possess(Bike) 는 그 경로를
    // 타지 않는다 — 오토바이는 ALyraCharacter 가 아니다. 그 결과 라이더가 언포제스되어도
    // 상호작용 어빌리티(ON_SPAWN 이라 늘 스캔 중이다)는 EndAbility 를 통보받지 못하고
    // 계속 활성 상태로 남아, 이미 탑승했는데도 Interact 프롬프트가 계속 뜨는 원인이 된다.
    // 라이더의 아바타를 잃기 전에 여기서 직접 취소한다.
    if (AAC_CharacterBase* MoverRider = Cast<AAC_CharacterBase>(RiderPawn))
    {
        if (ULyraAbilitySystemComponent* RiderASC = MoverRider->GetLyraAbilitySystemComponent())
        {
            RiderASC->CancelAbilitiesByFunc(
                [](const ULyraGameplayAbility* Ability, FGameplayAbilitySpecHandle)
                {
                    return Ability->IsA(ULyraGameplayAbility_Interact::StaticClass());
                },
                /*bReplicateCancelAbility=*/ true);
        }
    }

    // Possess() 가 ViewTarget 을 새로 빙의한 폰(Bike)으로 자동 전환한다. 예전에 카메라
    // 테스트용으로 라이더에게 강제로 되돌리는 코드가 있었는데, 그게 Possess 의 전환을
    // 도로 뒤집어서 탑승 후에도 계속 라이더의 카메라(와 그 카메라가 켠 상호작용 스캔)를
    // 보고 있었다. 지금은 Possess 의 기본 동작을 그대로 둔다.
    PC->Possess(Bike);

    UE_LOG(LogTemp, Warning, TEXT("[Vehicle] 탑승 — Rider=%s Bike=%s"), *GetNameSafe(RiderPawn), *GetNameSafe(Bike));

    EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}
