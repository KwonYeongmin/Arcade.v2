// Copyright Epic Games, Inc. All Rights Reserved.

#include "UAC_GameplayAbility_EnemyAttack.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "AIController.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameplayEffect.h"
#include "TimerManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(UAC_GameplayAbility_EnemyAttack)

UAC_GameplayAbility_EnemyAttack::UAC_GameplayAbility_EnemyAttack(const FObjectInitializer& objectInitializer)
	: Super(objectInitializer)
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
}

void UAC_GameplayAbility_EnemyAttack::ActivateAbility(const FGameplayAbilitySpecHandle handle, const FGameplayAbilityActorInfo* actorInfo,
	const FGameplayAbilityActivationInfo activationInfo, const FGameplayEventData* triggerEventData)
{
	Super::ActivateAbility(handle, actorInfo, activationInfo, triggerEventData);

	if (false == CommitAbility(handle, actorInfo, activationInfo))
	{
		EndAbility(handle, actorInfo, activationInfo, true, true);
		return;
	}

	CachedHandle = handle;

	if (Montage)
	{
		if (const APawn* pawn = Cast<APawn>(GetAvatarActorFromActorInfo()))
		{
			if (USkeletalMeshComponent* mesh = pawn->FindComponentByClass<USkeletalMeshComponent>())
			{
				if (UAnimInstance* anim = mesh->GetAnimInstance())
				{
					anim->Montage_Play(Montage);
				}
			}
		}
	}

	if (UWorld* world = GetWorld())
	{
		if (WindupSeconds > 0.0f)
		{
			world->GetTimerManager().SetTimer(WindupTimer, this, &UAC_GameplayAbility_EnemyAttack::ResolveHit, WindupSeconds, false);
		}
		else
		{
			ResolveHit();
		}
	}
}

AActor* UAC_GameplayAbility_EnemyAttack::GetTargetActor() const
{
	const APawn* pawn = Cast<APawn>(GetAvatarActorFromActorInfo());
	const AAIController* aiController = pawn ? Cast<AAIController>(pawn->GetController()) : nullptr;
	const UBlackboardComponent* blackboard = aiController ? aiController->GetBlackboardComponent() : nullptr;
	return blackboard ? Cast<AActor>(blackboard->GetValueAsObject(TEXT("TargetActor"))) : nullptr;
}

void UAC_GameplayAbility_EnemyAttack::ResolveHit()
{
	APawn* pawn = Cast<APawn>(GetAvatarActorFromActorInfo());
	UWorld* world = GetWorld();
	if (nullptr == pawn || nullptr == world)
	{
		EndAbility(CachedHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
		return;
	}

	AActor* target = GetTargetActor();

	switch (Shape)
	{
	case EAC_EnemyAttackShape::Projectile:
	{
		const FVector muzzle = pawn->GetActorLocation() + pawn->GetActorForwardVector() * Range + FVector(0, 0, 40.0f);
		if (ProjectileClass)
		{
			const FVector aimAt = target ? target->GetActorLocation() : (muzzle + pawn->GetActorForwardVector() * 2000.0f);
			FActorSpawnParameters params;
			params.Owner = pawn;
			params.Instigator = pawn;
			params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			world->SpawnActor<AActor>(ProjectileClass, muzzle, (aimAt - muzzle).Rotation(), params);
		}
		else if (target)
		{
			// Hitscan fallback.
			FHitResult hit;
			FCollisionQueryParams queryParams(SCENE_QUERY_STAT(EnemySpit), false, pawn);
			if (world->LineTraceSingleByChannel(hit, muzzle, target->GetActorLocation() + FVector(0, 0, 40.0f), ECC_Pawn, queryParams))
			{
				ApplyDamageTo(hit.GetActor());
			}
		}
		break;
	}
	case EAC_EnemyAttackShape::RadialAoE:
	case EAC_EnemyAttackShape::Melee:
	{
		const FVector centre = (Shape == EAC_EnemyAttackShape::Melee)
			? pawn->GetActorLocation() + pawn->GetActorForwardVector() * Range
			: pawn->GetActorLocation();

		TArray<FOverlapResult> overlaps;
		world->OverlapMultiByObjectType(overlaps, centre, FQuat::Identity,
			FCollisionObjectQueryParams(ECC_TO_BITFIELD(ECC_Pawn)), FCollisionShape::MakeSphere(Radius));

		TSet<AActor*> damaged;
		for (const FOverlapResult& overlap : overlaps)
		{
			AActor* other = overlap.GetActor();
			if (other && other != pawn && false == damaged.Contains(other))
			{
				damaged.Add(other);
				ApplyDamageTo(other);
			}
		}
		break;
	}
	}

	EndAbility(CachedHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UAC_GameplayAbility_EnemyAttack::ApplyDamageTo(AActor* target) const
{
	if (nullptr == target || nullptr == DamageGameplayEffectClass)
	{
		return;
	}

	UAbilitySystemComponent* selfAsc = GetAbilitySystemComponentFromActorInfo();
	UAbilitySystemComponent* targetAsc = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(target);
	if (nullptr == selfAsc || nullptr == targetAsc)
	{
		return;
	}

	AActor* avatar = GetAvatarActorFromActorInfo();
	FGameplayEffectContextHandle context = selfAsc->MakeEffectContext();
	context.AddInstigator(avatar, avatar);

	const FGameplayEffectSpecHandle specHandle = selfAsc->MakeOutgoingSpec(DamageGameplayEffectClass, Damage, context);
	if (false == specHandle.IsValid())
	{
		return;
	}
	if (DamageSetByCallerTag.IsValid())
	{
		specHandle.Data->SetSetByCallerMagnitude(DamageSetByCallerTag, Damage);
	}
	selfAsc->ApplyGameplayEffectSpecToTarget(*specHandle.Data.Get(), targetAsc);
}
