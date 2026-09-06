// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "BehaviorTree/BTTaskNode.h"
#include "GameplayTagContainer.h"

#include "UBTTask_FlyingApproachOrbit.generated.h"

class UGameplayEffect;

/**
 * UBTTask_FlyingApproachOrbit
 *
 * Flying enemy movement + attack in one node. Fly straight at the target until roughly
 * OrbitRadius away, then circle-strafe it at OrbitHeight. Every DiveInterval seconds it breaks
 * orbit, dives at the target (contact damage lands, or DiveDamage is applied on contact), then
 * pulls back and resumes orbiting.
 *
 * Drives UCharacterMovementComponent->Velocity directly (CMC in flying mode) — no NavMesh. Runs
 * until the target is lost, then fails so the tree re-selects. Orbit radius / height / speed come
 * from the pawn's UAC_ArcadeEnemyComponent (DT_Enemies row).
 */
UCLASS()
class LYRAGAME_API UBTTask_FlyingApproachOrbit : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UBTTask_FlyingApproachOrbit();

	UPROPERTY(EditAnywhere, Category = "Blackboard")
	struct FBlackboardKeySelector TargetActorKey;

	UPROPERTY(EditAnywhere, Category = "Orbit")
	bool bClockwise = true;

	/** Seconds of orbiting between dive attacks. 0 disables diving (pure circler). */
	UPROPERTY(EditAnywhere, Category = "Dive", meta = (ClampMin = "0.0"))
	float DiveInterval = 3.5f;

	/** Dive speed as a multiple of the enemy's normal move speed. */
	UPROPERTY(EditAnywhere, Category = "Dive", meta = (ClampMin = "1.0"))
	float DiveSpeedMultiplier = 2.2f;

	/** Distance at which the dive "connects" and flips to recovery. */
	UPROPERTY(EditAnywhere, Category = "Dive", meta = (ClampMin = "0.0"))
	float DiveHitRange = 140.0f;

	/** Hard cap on a single dive before bailing to recovery. */
	UPROPERTY(EditAnywhere, Category = "Dive", meta = (ClampMin = "0.1"))
	float MaxDiveSeconds = 1.5f;

	/** Seconds spent retreating after a dive before orbiting again. */
	UPROPERTY(EditAnywhere, Category = "Dive", meta = (ClampMin = "0.0"))
	float RecoverSeconds = 1.0f;

	/** Optional burst damage applied to the target when a dive connects (0 = rely on contact overlap). */
	UPROPERTY(EditAnywhere, Category = "Dive", meta = (ClampMin = "0.0"))
	float DiveDamage = 0.0f;

	UPROPERTY(EditAnywhere, Category = "Dive")
	TSubclassOf<UGameplayEffect> DiveDamageGameplayEffectClass;

	UPROPERTY(EditAnywhere, Category = "Dive")
	FGameplayTag DiveDamageSetByCallerTag;

protected:
	virtual void InitializeFromAsset(UBehaviorTree& asset) override;
	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& ownerComp, uint8* nodeMemory) override;
	virtual void TickTask(UBehaviorTreeComponent& ownerComp, uint8* nodeMemory, float deltaSeconds) override;
	virtual uint16 GetInstanceMemorySize() const override;
};
