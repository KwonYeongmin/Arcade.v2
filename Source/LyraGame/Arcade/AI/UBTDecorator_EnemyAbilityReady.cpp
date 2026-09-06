// Copyright Epic Games, Inc. All Rights Reserved.

#include "UBTDecorator_EnemyAbilityReady.h"

#include "AIController.h"
#include "Arcade/AI/UAC_ArcadeEnemyComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/BlackboardData.h"
#include "GameFramework/Pawn.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(UBTDecorator_EnemyAbilityReady)

UBTDecorator_EnemyAbilityReady::UBTDecorator_EnemyAbilityReady()
{
	NodeName = TEXT("Enemy Ability Ready");
	TargetActorKey.AddObjectFilter(this, GET_MEMBER_NAME_CHECKED(UBTDecorator_EnemyAbilityReady, TargetActorKey), AActor::StaticClass());
}

void UBTDecorator_EnemyAbilityReady::InitializeFromAsset(UBehaviorTree& asset)
{
	Super::InitializeFromAsset(asset);
	if (UBlackboardData* blackboardData = GetBlackboardAsset())
	{
		TargetActorKey.ResolveSelectedKey(*blackboardData);
	}
}

bool UBTDecorator_EnemyAbilityReady::CalculateRawConditionValue(UBehaviorTreeComponent& ownerComp, uint8* /*nodeMemory*/) const
{
	const AAIController* aiController = ownerComp.GetAIOwner();
	const APawn* selfPawn = aiController ? aiController->GetPawn() : nullptr;
	if (nullptr == selfPawn || false == AbilityTag.IsValid())
	{
		return false;
	}

	const UAC_ArcadeEnemyComponent* enemy = selfPawn->FindComponentByClass<UAC_ArcadeEnemyComponent>();
	if (nullptr == enemy || false == enemy->IsAbilityReady(AbilityTag))
	{
		return false;
	}

	if (MaxRange > 0.0f)
	{
		const UBlackboardComponent* blackboard = ownerComp.GetBlackboardComponent();
		const AActor* target = blackboard ? Cast<AActor>(blackboard->GetValueAsObject(TargetActorKey.SelectedKeyName)) : nullptr;
		if (nullptr == target || FVector::Dist(selfPawn->GetActorLocation(), target->GetActorLocation()) > MaxRange)
		{
			return false;
		}
	}

	return true;
}

FString UBTDecorator_EnemyAbilityReady::GetStaticDescription() const
{
	return FString::Printf(TEXT("%s ready%s"), *AbilityTag.ToString(),
		MaxRange > 0.0f ? *FString::Printf(TEXT(" & target < %.0f"), MaxRange) : TEXT(""));
}
