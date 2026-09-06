// Copyright Epic Games, Inc. All Rights Reserved.

#include "UBTTask_FlyingApproachOrbit.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "AIController.h"
#include "Arcade/AI/UAC_ArcadeEnemyComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/BlackboardData.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameplayEffect.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(UBTTask_FlyingApproachOrbit)

namespace
{
	enum class EFlyPhase : uint8
	{
		Orbit,
		Dive,
		Recover,
	};

	struct FOrbitTaskMemory
	{
		float OrbitAngle = 0.0f;
		float PhaseTimer = 0.0f;
		EFlyPhase Phase = EFlyPhase::Orbit;
	};
}

UBTTask_FlyingApproachOrbit::UBTTask_FlyingApproachOrbit()
{
	NodeName = TEXT("Flying Approach / Orbit");
	bNotifyTick = true;
	bNotifyTaskFinished = false;

	TargetActorKey.AddObjectFilter(this, GET_MEMBER_NAME_CHECKED(UBTTask_FlyingApproachOrbit, TargetActorKey), AActor::StaticClass());
}

void UBTTask_FlyingApproachOrbit::InitializeFromAsset(UBehaviorTree& asset)
{
	Super::InitializeFromAsset(asset);
	if (UBlackboardData* blackboardData = GetBlackboardAsset())
	{
		TargetActorKey.ResolveSelectedKey(*blackboardData);
	}
}

uint16 UBTTask_FlyingApproachOrbit::GetInstanceMemorySize() const
{
	return sizeof(FOrbitTaskMemory);
}

EBTNodeResult::Type UBTTask_FlyingApproachOrbit::ExecuteTask(UBehaviorTreeComponent& /*ownerComp*/, uint8* nodeMemory)
{
	FOrbitTaskMemory* memory = reinterpret_cast<FOrbitTaskMemory*>(nodeMemory);
	memory->OrbitAngle = FMath::FRandRange(0.0f, 2.0f * PI);
	memory->PhaseTimer = FMath::FRandRange(0.0f, 1.0f); // stagger dives across a swarm
	memory->Phase = EFlyPhase::Orbit;
	return EBTNodeResult::InProgress;
}

void UBTTask_FlyingApproachOrbit::TickTask(UBehaviorTreeComponent& ownerComp, uint8* nodeMemory, float deltaSeconds)
{
	FOrbitTaskMemory* memory = reinterpret_cast<FOrbitTaskMemory*>(nodeMemory);

	const AAIController* aiController = ownerComp.GetAIOwner();
	ACharacter* selfCharacter = aiController ? Cast<ACharacter>(aiController->GetPawn()) : nullptr;
	const UBlackboardComponent* blackboard = ownerComp.GetBlackboardComponent();
	UCharacterMovementComponent* movement = selfCharacter ? selfCharacter->GetCharacterMovement() : nullptr;
	if (nullptr == selfCharacter || nullptr == blackboard || nullptr == movement)
	{
		FinishLatentTask(ownerComp, EBTNodeResult::Failed);
		return;
	}

	AActor* target = Cast<AActor>(blackboard->GetValueAsObject(TargetActorKey.SelectedKeyName));
	if (nullptr == target)
	{
		movement->Velocity = FVector::ZeroVector;
		FinishLatentTask(ownerComp, EBTNodeResult::Failed);
		return;
	}

	float orbitRadius = 450.0f;
	float orbitHeight = 300.0f;
	float moveSpeed = 700.0f;
	if (const UAC_ArcadeEnemyComponent* enemy = selfCharacter->FindComponentByClass<UAC_ArcadeEnemyComponent>())
	{
		orbitRadius = FMath::Max(enemy->GetOrbitRadius(), 50.0f);
		orbitHeight = enemy->GetOrbitHeight();
		moveSpeed = FMath::Max(enemy->GetMoveSpeed(), 50.0f);
	}

	const FVector selfLocation = selfCharacter->GetActorLocation();
	const FVector targetLocation = target->GetActorLocation();
	const FVector hoverCentre(targetLocation.X, targetLocation.Y, targetLocation.Z + orbitHeight);
	const float distanceToTarget = FVector::Dist(selfLocation, targetLocation);

	memory->PhaseTimer += deltaSeconds;

	FVector desiredLocation = selfLocation;
	float speed = moveSpeed;

	switch (memory->Phase)
	{
	case EFlyPhase::Orbit:
	{
		const float horizontalDistance = FVector::Dist2D(selfLocation, targetLocation);
		if (horizontalDistance > orbitRadius * 1.25f)
		{
			const FVector inbound = (selfLocation - targetLocation).GetSafeNormal2D();
			desiredLocation = hoverCentre + inbound * orbitRadius;
		}
		else
		{
			const float angularSpeed = (moveSpeed / orbitRadius) * (bClockwise ? 1.0f : -1.0f);
			memory->OrbitAngle += angularSpeed * deltaSeconds;
			const FVector offset(FMath::Cos(memory->OrbitAngle) * orbitRadius, FMath::Sin(memory->OrbitAngle) * orbitRadius, 0.0f);
			desiredLocation = hoverCentre + offset;
		}

		if (DiveInterval > 0.0f && memory->PhaseTimer >= DiveInterval)
		{
			memory->Phase = EFlyPhase::Dive;
			memory->PhaseTimer = 0.0f;
		}
		break;
	}
	case EFlyPhase::Dive:
	{
		desiredLocation = targetLocation;
		speed = moveSpeed * DiveSpeedMultiplier;

		if (distanceToTarget <= DiveHitRange || memory->PhaseTimer >= MaxDiveSeconds)
		{
			if (distanceToTarget <= DiveHitRange && DiveDamage > 0.0f && DiveDamageGameplayEffectClass)
			{
				UAbilitySystemComponent* selfAsc = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(selfCharacter);
				UAbilitySystemComponent* targetAsc = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(target);
				if (selfAsc && targetAsc)
				{
					FGameplayEffectContextHandle context = selfAsc->MakeEffectContext();
					context.AddInstigator(selfCharacter, selfCharacter);
					const FGameplayEffectSpecHandle spec = selfAsc->MakeOutgoingSpec(DiveDamageGameplayEffectClass, DiveDamage, context);
					if (spec.IsValid())
					{
						if (DiveDamageSetByCallerTag.IsValid())
						{
							spec.Data->SetSetByCallerMagnitude(DiveDamageSetByCallerTag, DiveDamage);
						}
						selfAsc->ApplyGameplayEffectSpecToTarget(*spec.Data.Get(), targetAsc);
					}
				}
			}
			memory->Phase = EFlyPhase::Recover;
			memory->PhaseTimer = 0.0f;
		}
		break;
	}
	case EFlyPhase::Recover:
	{
		const FVector away = (selfLocation - targetLocation).GetSafeNormal();
		desiredLocation = hoverCentre + away * orbitRadius * 1.4f;
		if (memory->PhaseTimer >= RecoverSeconds)
		{
			memory->Phase = EFlyPhase::Orbit;
			memory->PhaseTimer = 0.0f;
		}
		break;
	}
	}

	const FVector toDesired = desiredLocation - selfLocation;
	movement->Velocity = toDesired.GetClampedToMaxSize(speed * deltaSeconds) / FMath::Max(deltaSeconds, KINDA_SMALL_NUMBER);

	const FVector faceDir = (targetLocation - selfLocation).GetSafeNormal2D();
	if (false == faceDir.IsNearlyZero())
	{
		const FRotator current = selfCharacter->GetActorRotation();
		const FRotator wanted(0.0f, faceDir.Rotation().Yaw, 0.0f);
		selfCharacter->SetActorRotation(FMath::RInterpTo(current, wanted, deltaSeconds, 8.0f));
	}
}
