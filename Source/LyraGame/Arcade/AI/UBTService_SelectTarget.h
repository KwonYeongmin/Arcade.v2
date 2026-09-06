// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "BehaviorTree/BTService.h"

#include "UBTService_SelectTarget.generated.h"

/**
 * UBTService_SelectTarget
 *
 * Writes the nearest live player pawn into the blackboard's TargetActor key. No AI Perception —
 * in a wave co-op arena the enemy always wants the closest player, and this stays fully
 * server-authoritative.
 */
UCLASS()
class LYRAGAME_API UBTService_SelectTarget : public UBTService
{
	GENERATED_BODY()

public:
	UBTService_SelectTarget();

	/** Blackboard Object key that receives the chosen player pawn. */
	UPROPERTY(EditAnywhere, Category = "Blackboard")
	struct FBlackboardKeySelector TargetActorKey;

protected:
	virtual void InitializeFromAsset(UBehaviorTree& asset) override;
	virtual void TickNode(UBehaviorTreeComponent& ownerComp, uint8* nodeMemory, float deltaSeconds) override;
};
