// Copyright Epic Games, Inc. All Rights Reserved.

#include "Character/LyraHeroCharacter.h"

#include "Animation/AnimMontage.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Equipment/LyraEquipmentInstance.h"
#include "Equipment/LyraEquipmentManagerComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Math/UnrealMathUtility.h"
#include "Perception/AIPerceptionStimuliSourceComponent.h"
#include "Feedback/ContextEffects/LyraContextEffectsInterface.h"
#include "UObject/UnrealType.h"

ALyraHeroCharacter::ALyraHeroCharacter(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
}

void ALyraHeroCharacter::AnimMotionEffect_Implementation(
    const FName Bone, const FGameplayTag MotionEffect,
    USceneComponent* StaticMeshComponent, const FVector LocationOffset, const FRotator RotationOffset,
    const UAnimSequenceBase* AnimationSequence, const bool bHitSuccess, const FHitResult HitResult,
    FGameplayTagContainer Contexts, FVector VFXScale, float AudioVolume, float AudioPitch)
{
    if (IsValid(FootStep) && FootStep->Implements<ULyraContextEffectsInterface>())
    {
        ILyraContextEffectsInterface::Execute_AnimMotionEffect(
            FootStep,
            Bone, MotionEffect, StaticMeshComponent,
            LocationOffset, RotationOffset, AnimationSequence,
            bHitSuccess, HitResult, Contexts,
            VFXScale, AudioVolume, AudioPitch);
    }
}

void ALyraHeroCharacter::BeginPlay()
{
    Super::BeginPlay();

    if (!HasAuthority() || IsValid(FootStep) || !FootStepClass)
    {
        return;
    }

    AActor* SpawnedActor = GetWorld()->SpawnActorDeferred<AActor>(
        FootStepClass, FTransform::Identity, this, GetInstigator());
    if (!SpawnedActor)
    {
        return;
    }

    USkeletalMeshComponent* SkeletalMesh = GetMesh();
    // Expose-on-spawn BP vars: set before FinishSpawning so B_FootStep BeginPlay sees them
    if (FObjectProperty* MeshProp = FindFProperty<FObjectProperty>(SpawnedActor->GetClass(), TEXT("Skeletal Mesh Component")))
    {
        MeshProp->SetObjectPropertyValue_InContainer(SpawnedActor, SkeletalMesh);
    }
    if (FNameProperty* BoneProp = FindFProperty<FNameProperty>(SpawnedActor->GetClass(), TEXT("FootStepRightFootBone")))
    {
        BoneProp->SetPropertyValue_InContainer(SpawnedActor, FName("foot_r"));
    }

    SpawnedActor->FinishSpawning(FTransform::Identity);
    FootStep = SpawnedActor;
    FootStep->AttachToComponent(SkeletalMesh, FAttachmentTransformRules::KeepRelativeTransform);
}

void ALyraHeroCharacter::Ragdoll()
{
    USkeletalMeshComponent* SkeletalMesh = GetMesh();
    if (!IsValid(SkeletalMesh))
    {
        return;
    }

    SkeletalMesh->SetCollisionProfileName(TEXT("Ragdoll"), true);
    SkeletalMesh->SetAllBodiesBelowSimulatePhysics(NAME_None, true, true);

    // vector+vector B-pin unknown (linked_to absent); default 0,0,0 → identity, so GetSafeNormal only
    const FVector LastVelocity = GetCharacterMovement()->GetLastUpdateVelocity();
    const FVector ImpulseDir = LastVelocity.GetSafeNormal();
    const FVector Impulse = ImpulseDir * RagdollImpulseStrength;
    SkeletalMesh->AddImpulse(Impulse, RagdollImpulseBone, true);
}

void ALyraHeroCharacter::HideEquippedWeapons()
{
    ULyraEquipmentManagerComponent* EquipManager = FindComponentByClass<ULyraEquipmentManagerComponent>();
    if (!IsValid(EquipManager))
    {
        return;
    }

    for (ULyraEquipmentInstance* Instance : EquipManager->GetEquipmentInstancesOfType(ULyraEquipmentInstance::StaticClass()))
    {
        for (AActor* SpawnedActor : Instance->GetSpawnedActors())
        {
            if (SpawnedActor)
            {
                SpawnedActor->SetActorHiddenInGame(true);
            }
        }
    }
}

void ALyraHeroCharacter::OnDeathStarted(AActor* OwningActor)
{
    Super::OnDeathStarted(OwningActor);

    HideEquippedWeapons();

    if (DeathMontages.Num() > 0)
    {
        const int32 Index = FMath::RandRange(0, DeathMontages.Num() - 1);
        if (IsValid(DeathMontages[Index]))
        {
            PlayAnimMontage(DeathMontages[Index]);
        }
    }

    // BP calls UnregisterFromSense(SenseClass) but sense class unknown (linked_to absent in MCP data).
    // Using UnregisterFromPerceptionSystem() — broader than BP intent, safe for a dead character.
    if (UAIPerceptionStimuliSourceComponent* PerceptionSource =
            FindComponentByClass<UAIPerceptionStimuliSourceComponent>())
    {
        PerceptionSource->UnregisterFromPerceptionSystem();
    }

    // Delay duration from BP: RandFloat(0.1, 0.6) — confirmed by pin_defaults in B_Hero_Default_EventGraph.json
    const float Delay = FMath::FRandRange(0.1f, 0.6f);
    GetWorldTimerManager().SetTimer(
        RagdollTimerHandle, this,
        &ALyraHeroCharacter::Ragdoll,
        Delay, false
    );
}

void ALyraHeroCharacter::UpdateCustomStencilFromTeamID(int32 TeamId)
{
    const int32 StencilValue = (IsLocallyControlled() && IsPlayerControlled()) ? 0 : TeamId;

    TArray<UPrimitiveComponent*> PrimitiveComponents;
    GetComponents<UPrimitiveComponent>(PrimitiveComponents, true);
    for (UPrimitiveComponent* PrimComp : PrimitiveComponents)
    {
        PrimComp->SetCustomDepthStencilValue(StencilValue);
    }
}

void ALyraHeroCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    GetWorldTimerManager().ClearTimer(RagdollTimerHandle); // must precede Super — world may be torn down after
    Super::EndPlay(EndPlayReason);
}
