// Copyright Epic Games, Inc. All Rights Reserved.

#include "AAC_ArcadeAIController.h"

#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/BlackboardData.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AAC_ArcadeAIController)

AAC_ArcadeAIController::AAC_ArcadeAIController()
{
	bWantsPlayerState = false;
	bAttachToPawn = true;
}

void AAC_ArcadeAIController::OnPossess(APawn* inPawn)
{
	Super::OnPossess(inPawn);

	if (nullptr == BehaviorTree)
	{
		return;
	}

	UBlackboardComponent* blackboardComp = Blackboard;
	if (BlackboardAsset)
	{
		UseBlackboard(BlackboardAsset, blackboardComp);
	}
	else if (BehaviorTree->BlackboardAsset)
	{
		UseBlackboard(BehaviorTree->BlackboardAsset, blackboardComp);
	}

	RunBehaviorTree(BehaviorTree);
}

void AAC_ArcadeAIController::OnUnPossess()
{
	if (UBrainComponent* brain = GetBrainComponent())
	{
		brain->StopLogic(TEXT("unpossessed"));
	}
	Super::OnUnPossess();
}

void AAC_ArcadeAIController::SetGenericTeamId(const FGenericTeamId& newTeamId)
{
	const FGenericTeamId oldTeamId = MyTeamId;
	MyTeamId = newTeamId;
	ILyraTeamAgentInterface::ConditionalBroadcastTeamChanged(this, oldTeamId, MyTeamId);
}

FGenericTeamId AAC_ArcadeAIController::GetGenericTeamId() const
{
	return MyTeamId;
}

FOnLyraTeamIndexChangedDelegate* AAC_ArcadeAIController::GetOnTeamIndexChangedDelegate()
{
	return &OnTeamChangedDelegate;
}
