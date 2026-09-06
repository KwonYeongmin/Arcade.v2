// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "BehaviorTree/BTDecorator.h"
#include "GameplayTagContainer.h"

#include "UBTDecorator_EnemyAbilityReady.generated.h"

/**
 * UBTDecorator_EnemyAbilityReady
 *
 * Passes when the enemy ability slot AbilityTag is off cooldown (per UAC_ArcadeEnemyComponent)
 * and, optionally, only when the target is within MaxRange. Put it on the branch that fires that
 * ability so the tree naturally prioritises whichever skill is ready.
 */
UCLASS()
class LYRAGAME_API UBTDecorator_EnemyAbilityReady : public UBTDecorator
{
	GENERATED_BODY()

public:
	UBTDecorator_EnemyAbilityReady();

	UPROPERTY(EditAnywhere, Category = "Ability", meta = (Categories = "Ability.Enemy"))
	FGameplayTag AbilityTag;

	UPROPERTY(EditAnywhere, Category = "Ability")
	struct FBlackboardKeySelector TargetActorKey;

	/** 0 = no range check. Otherwise the target must be within this many cm. */
	UPROPERTY(EditAnywhere, Category = "Ability", meta = (ClampMin = "0.0"))
	float MaxRange = 0.0f;

protected:
	virtual void InitializeFromAsset(UBehaviorTree& asset) override;
	virtual bool CalculateRawConditionValue(UBehaviorTreeComponent& ownerComp, uint8* nodeMemory) const override;
	virtual FString GetStaticDescription() const override;
};
