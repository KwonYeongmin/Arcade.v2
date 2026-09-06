// Copyright Epic Games, Inc. All Rights Reserved.

#include "AAC_ArcadeEnemyCharacter.h"

#include "AAC_ArcadeAIController.h"
#include "Arcade/AI/UAC_ArcadeEnemyComponent.h"
#include "Character/AC_SlickReactionComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AAC_ArcadeEnemyCharacter)

AAC_ArcadeEnemyCharacter::AAC_ArcadeEnemyCharacter(const FObjectInitializer& objectInitializer)
	: Super(objectInitializer)
{
	AIControllerClass = AAC_ArcadeAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

	bUseControllerRotationYaw = false;

	EnemyComponent = CreateDefaultSubobject<UAC_ArcadeEnemyComponent>(TEXT("EnemyComponent"));
	SlickReactionComponent = CreateDefaultSubobject<UAC_SlickReactionComponent>(TEXT("SlickReactionComponent"));
}
