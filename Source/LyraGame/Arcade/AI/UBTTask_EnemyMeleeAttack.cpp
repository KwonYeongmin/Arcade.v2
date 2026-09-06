// Copyright Epic Games, Inc. All Rights Reserved.

#include "UBTTask_EnemyMeleeAttack.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/BlackboardData.h"
#include "GameFramework/Pawn.h"
#include "GameplayEffect.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(UBTTask_EnemyMeleeAttack)

UBTTask_EnemyMeleeAttack::UBTTask_EnemyMeleeAttack()
{
	NodeName = TEXT("Enemy Melee Attack");
	TargetActorKey.AddObjectFilter(this, GET_MEMBER_NAME_CHECKED(UBTTask_EnemyMeleeAttack, TargetActorKey), AActor::StaticClass());
}

void UBTTask_EnemyMeleeAttack::InitializeFromAsset(UBehaviorTree& asset)
{
	Super::InitializeFromAsset(asset);
	if (UBlackboardData* blackboardData = GetBlackboardAsset())
	{
		TargetActorKey.ResolveSelectedKey(*blackboardData);
	}
}

EBTNodeResult::Type UBTTask_EnemyMeleeAttack::ExecuteTask(UBehaviorTreeComponent& ownerComp, uint8* /*nodeMemory*/)
{
	const AAIController* aiController = ownerComp.GetAIOwner();
	APawn* selfPawn = aiController ? aiController->GetPawn() : nullptr;
	const UBlackboardComponent* blackboard = ownerComp.GetBlackboardComponent();
	if (nullptr == selfPawn || nullptr == blackboard || nullptr == DamageGameplayEffectClass)
	{
		return EBTNodeResult::Failed;
	}

	AActor* target = Cast<AActor>(blackboard->GetValueAsObject(TargetActorKey.SelectedKeyName));
	if (nullptr == target)
	{
		return EBTNodeResult::Failed;
	}

	if (FVector::Dist(selfPawn->GetActorLocation(), target->GetActorLocation()) > MeleeRange)
	{
		return EBTNodeResult::Failed;
	}

	UAbilitySystemComponent* selfAsc = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(selfPawn);
	UAbilitySystemComponent* targetAsc = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(target);
	if (nullptr == selfAsc || nullptr == targetAsc)
	{
		return EBTNodeResult::Failed;
	}

	FGameplayEffectContextHandle context = selfAsc->MakeEffectContext();
	context.AddInstigator(selfPawn, selfPawn);

	const FGameplayEffectSpecHandle specHandle = selfAsc->MakeOutgoingSpec(DamageGameplayEffectClass, Damage, context);
	if (false == specHandle.IsValid())
	{
		return EBTNodeResult::Failed;
	}
	if (DamageSetByCallerTag.IsValid())
	{
		specHandle.Data->SetSetByCallerMagnitude(DamageSetByCallerTag, Damage);
	}
	selfAsc->ApplyGameplayEffectSpecToTarget(*specHandle.Data.Get(), targetAsc);

	return EBTNodeResult::Succeeded;
}
