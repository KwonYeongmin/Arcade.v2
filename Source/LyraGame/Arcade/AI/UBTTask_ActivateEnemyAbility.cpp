// Copyright Epic Games, Inc. All Rights Reserved.

#include "UBTTask_ActivateEnemyAbility.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "AIController.h"
#include "Arcade/AI/UAC_ArcadeEnemyComponent.h"
#include "GameFramework/Pawn.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(UBTTask_ActivateEnemyAbility)

UBTTask_ActivateEnemyAbility::UBTTask_ActivateEnemyAbility()
{
	NodeName = TEXT("Activate Enemy Ability");
}

EBTNodeResult::Type UBTTask_ActivateEnemyAbility::ExecuteTask(UBehaviorTreeComponent& ownerComp, uint8* /*nodeMemory*/)
{
	const AAIController* aiController = ownerComp.GetAIOwner();
	APawn* selfPawn = aiController ? aiController->GetPawn() : nullptr;
	if (nullptr == selfPawn || false == AbilityTag.IsValid())
	{
		return EBTNodeResult::Failed;
	}

	UAbilitySystemComponent* asc = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(selfPawn);
	if (nullptr == asc)
	{
		return EBTNodeResult::Failed;
	}

	const bool bActivated = asc->TryActivateAbilitiesByTag(FGameplayTagContainer(AbilityTag));
	if (false == bActivated)
	{
		return EBTNodeResult::Failed;
	}

	if (UAC_ArcadeEnemyComponent* enemy = selfPawn->FindComponentByClass<UAC_ArcadeEnemyComponent>())
	{
		enemy->StartAbilityCooldown(AbilityTag, CooldownSeconds);
	}

	return EBTNodeResult::Succeeded;
}

FString UBTTask_ActivateEnemyAbility::GetStaticDescription() const
{
	return FString::Printf(TEXT("Activate %s (cd %.1fs)"), *AbilityTag.ToString(), CooldownSeconds);
}
