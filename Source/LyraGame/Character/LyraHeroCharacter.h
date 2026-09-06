// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Character/LyraCharacter.h"
#include "Feedback/ContextEffects/LyraContextEffectsInterface.h"

#include "LyraHeroCharacter.generated.h"

#define UE_API LYRAGAME_API

class UAnimMontage;

/**
 * ALyraHeroCharacter
 *
 * B_Hero_Default BP 로직(Ragdoll, OnDeathStarted)을 C++로 포팅한 캐릭터 클래스.
 * Character_Default BP의 Parent Class로 사용된다.
 */
UCLASS(MinimalAPI, Blueprintable)
class ALyraHeroCharacter : public ALyraCharacter, public ILyraContextEffectsInterface
{
    GENERATED_BODY()

public:
    UE_API ALyraHeroCharacter(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

    UFUNCTION(BlueprintCallable, Category = "Lyra|Character")
    UE_API void Ragdoll();

    UFUNCTION(BlueprintCallable, Category = "Lyra|Character")
    UE_API void HideEquippedWeapons();

    UFUNCTION(BlueprintCallable, Category = "Lyra|Character")
    UE_API void UpdateCustomStencilFromTeamID(int32 TeamId);

    UE_API virtual void AnimMotionEffect_Implementation(const FName Bone, const FGameplayTag MotionEffect,
        USceneComponent* StaticMeshComponent, const FVector LocationOffset, const FRotator RotationOffset,
        const UAnimSequenceBase* AnimationSequence, const bool bHitSuccess, const FHitResult HitResult,
        FGameplayTagContainer Contexts, FVector VFXScale = FVector(1), float AudioVolume = 1, float AudioPitch = 1) override;

protected:
    UE_API virtual void BeginPlay() override;

    UE_API virtual void OnDeathStarted(AActor* OwningActor) override;

    UE_API virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lyra|Combat")
    TArray<TObjectPtr<UAnimMontage>> DeathMontages;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lyra|Combat")
    float RagdollImpulseStrength = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lyra|Combat")
    FName RagdollImpulseBone = NAME_None;

    UPROPERTY(EditDefaultsOnly, Category = "Lyra|Combat")
    TSubclassOf<AActor> FootStepClass;

    UPROPERTY(BlueprintReadWrite, Category = "Lyra|Combat")
    TObjectPtr<AActor> FootStep;

private:
    FTimerHandle RagdollTimerHandle;
};

#undef UE_API
