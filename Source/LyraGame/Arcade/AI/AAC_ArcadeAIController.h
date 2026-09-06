// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AIController.h"
#include "Teams/LyraTeamAgentInterface.h"

#include "AAC_ArcadeAIController.generated.h"

class UBehaviorTree;
class UBlackboardData;

namespace ArcadeAIKeys
{
	/** Object key — the player actor this enemy is currently chasing (set by UBTService_SelectTarget). */
	inline const FName TargetActor = TEXT("TargetActor");
}

/**
 * AAC_ArcadeAIController
 *
 * Runs the enemy behaviour tree. No AI Perception — a wave co-op arena enemy always targets the
 * nearest live player, which UBTService_SelectTarget computes from the GameState player list.
 * Assign BehaviorTree / BlackboardData on a BP subclass (BP_ArcadeAIController).
 *
 * Implements ILyraTeamAgentInterface: ALyraCharacter::SetGenericTeamId refuses direct calls once
 * a pawn is possessed ("driven by the associated controller") and instead pulls the team from
 * this controller in PossessedBy — but only if the controller implements Lyra's interface (the
 * plain engine AAIController does not). So the enemy team is set here, not on the pawn.
 */
UCLASS()
class LYRAGAME_API AAC_ArcadeAIController : public AAIController, public ILyraTeamAgentInterface
{
	GENERATED_BODY()

public:
	AAC_ArcadeAIController();

	//~ILyraTeamAgentInterface
	virtual void SetGenericTeamId(const FGenericTeamId& newTeamId) override;
	virtual FGenericTeamId GetGenericTeamId() const override;
	virtual FOnLyraTeamIndexChangedDelegate* GetOnTeamIndexChangedDelegate() override;
	//~End

protected:
	virtual void OnPossess(APawn* inPawn) override;
	virtual void OnUnPossess() override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arcade AI")
	TObjectPtr<UBehaviorTree> BehaviorTree;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arcade AI")
	TObjectPtr<UBlackboardData> BlackboardAsset;

private:
	FOnLyraTeamIndexChangedDelegate OnTeamChangedDelegate;
	FGenericTeamId MyTeamId = FGenericTeamId::NoTeam;
};
