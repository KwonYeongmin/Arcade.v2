// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Character/LyraCharacterWithAbilities.h"

#include "AAC_ArcadeEnemyCharacter.generated.h"

class UAC_ArcadeEnemyComponent;
class UAC_SlickReactionComponent;

/**
 * AAC_ArcadeEnemyCharacter
 *
 * Self-contained-ASC character used for Arcade enemies. Carries the enemy stat / contact-damage
 * component and the Slick reaction component; the actual mesh, animation, and asset references
 * live on the BP subclass (B_Gnat, ...). Its stats come from DT_Enemies via
 * UAC_ArcadeEnemyComponent::StatRowName.
 *
 * Uses CMC (not Mover) — server-authoritative swarm AI, and it reuses the same damage / Slick
 * path already proven on the shooting-target dummy.
 */
UCLASS(Blueprintable)
class LYRAGAME_API AAC_ArcadeEnemyCharacter : public ALyraCharacterWithAbilities
{
	GENERATED_BODY()

public:
	AAC_ArcadeEnemyCharacter(const FObjectInitializer& objectInitializer);

	UAC_ArcadeEnemyComponent* GetEnemyComponent() const { return EnemyComponent; }

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Arcade Enemy", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UAC_ArcadeEnemyComponent> EnemyComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Arcade Enemy", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UAC_SlickReactionComponent> SlickReactionComponent;
};
