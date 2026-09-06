// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "BehaviorTree/BTTaskNode.h"
#include "GameplayTagContainer.h"

#include "UBTTask_EnemyMeleeAttack.generated.h"

class UGameplayEffect;

/**
 * UBTTask_EnemyMeleeAttack
 *
 * Ground enemy melee: if the blackboard target is within MeleeRange, apply a damage GameplayEffect
 * from the enemy's ASC and succeed; otherwise fail so the tree goes back to MoveTo. Pace the loop
 * with a Wait node after this in the tree.
 */
UCLASS()
class LYRAGAME_API UBTTask_EnemyMeleeAttack : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UBTTask_EnemyMeleeAttack();

	UPROPERTY(EditAnywhere, Category = "Blackboard")
	struct FBlackboardKeySelector TargetActorKey;

	UPROPERTY(EditAnywhere, Category = "Melee", meta = (ClampMin = "0.0"))
	float MeleeRange = 180.0f;

	UPROPERTY(EditAnywhere, Category = "Melee", meta = (ClampMin = "0.0"))
	float Damage = 15.0f;

	UPROPERTY(EditAnywhere, Category = "Melee")
	TSubclassOf<UGameplayEffect> DamageGameplayEffectClass;

	UPROPERTY(EditAnywhere, Category = "Melee")
	FGameplayTag DamageSetByCallerTag;

protected:
	virtual void InitializeFromAsset(UBehaviorTree& asset) override;
	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& ownerComp, uint8* nodeMemory) override;
};
