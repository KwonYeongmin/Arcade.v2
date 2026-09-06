// Copyright Epic Games, Inc. All Rights Reserved.

#include "UBTService_SelectTarget.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/Attributes/LyraHealthSet.h"
#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/BlackboardData.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(UBTService_SelectTarget)

namespace
{
	bool IsPawnAlive(const APawn* pawn)
	{
		if (nullptr == pawn)
		{
			return false;
		}
		const UAbilitySystemComponent* asc = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(const_cast<APawn*>(pawn));
		if (nullptr == asc)
		{
			return true; // no ASC to consult — assume alive rather than never target it
		}
		return asc->GetNumericAttribute(ULyraHealthSet::GetHealthAttribute()) > 0.0f;
	}
}

UBTService_SelectTarget::UBTService_SelectTarget()
{
	NodeName = TEXT("Select Nearest Player");
	Interval = 0.5f;
	RandomDeviation = 0.1f;
	bNotifyTick = true;

	TargetActorKey.AddObjectFilter(this, GET_MEMBER_NAME_CHECKED(UBTService_SelectTarget, TargetActorKey), AActor::StaticClass());
}

void UBTService_SelectTarget::InitializeFromAsset(UBehaviorTree& asset)
{
	Super::InitializeFromAsset(asset);
	if (UBlackboardData* blackboardData = GetBlackboardAsset())
	{
		TargetActorKey.ResolveSelectedKey(*blackboardData);
	}
}

void UBTService_SelectTarget::TickNode(UBehaviorTreeComponent& ownerComp, uint8* nodeMemory, float deltaSeconds)
{
	Super::TickNode(ownerComp, nodeMemory, deltaSeconds);

	const AAIController* aiController = ownerComp.GetAIOwner();
	APawn* selfPawn = aiController ? aiController->GetPawn() : nullptr;
	UBlackboardComponent* blackboard = ownerComp.GetBlackboardComponent();
	if (nullptr == selfPawn || nullptr == blackboard)
	{
		return;
	}

	const FVector selfLocation = selfPawn->GetActorLocation();
	AActor* best = nullptr;
	float bestDistanceSq = TNumericLimits<float>::Max();

	const UWorld* world = selfPawn->GetWorld();
	const AGameStateBase* gameState = world ? world->GetGameState() : nullptr;
	if (gameState)
	{
		for (const APlayerState* playerState : gameState->PlayerArray)
		{
			APawn* playerPawn = playerState ? playerState->GetPawn() : nullptr;
			if (playerPawn == selfPawn || false == IsPawnAlive(playerPawn))
			{
				continue;
			}
			const float distanceSq = FVector::DistSquared(selfLocation, playerPawn->GetActorLocation());
			if (distanceSq < bestDistanceSq)
			{
				bestDistanceSq = distanceSq;
				best = playerPawn;
			}
		}
	}

	blackboard->SetValueAsObject(TargetActorKey.SelectedKeyName, best);
}
