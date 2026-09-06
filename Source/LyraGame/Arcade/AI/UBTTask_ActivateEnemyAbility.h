// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "BehaviorTree/BTTaskNode.h"
#include "GameplayTagContainer.h"

#include "UBTTask_ActivateEnemyAbility.generated.h"

/**
 * UBTTask_ActivateEnemyAbility
 *
 * Activates the enemy ability tagged AbilityTag (Ability.Enemy.Primary / .Secondary) on the
 * pawn's ASC and starts its cooldown on UAC_ArcadeEnemyComponent. Synchronous — succeeds if the
 * ability activated, fails otherwise. Gate it with UBTDecorator_EnemyAbilityReady and pace with a
 * Wait node.
 */
UCLASS()
class LYRAGAME_API UBTTask_ActivateEnemyAbility : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UBTTask_ActivateEnemyAbility();

	UPROPERTY(EditAnywhere, Category = "Ability", meta = (Categories = "Ability.Enemy"))
	FGameplayTag AbilityTag;

	UPROPERTY(EditAnywhere, Category = "Ability", meta = (ClampMin = "0.0"))
	float CooldownSeconds = 3.0f;

protected:
	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& ownerComp, uint8* nodeMemory) override;
	virtual FString GetStaticDescription() const override;
};
