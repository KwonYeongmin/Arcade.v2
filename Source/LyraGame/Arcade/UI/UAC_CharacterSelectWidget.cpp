// Copyright Epic Games, Inc. All Rights Reserved.

#include "UAC_CharacterSelectWidget.h"

#include "Arcade/AC_ArcadeRoleTags.h"
#include "Arcade/Player/UAC_CharacterSelectComponent.h"
#include "Arcade/UI/UAC_CharacterCardWidget.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(UAC_CharacterSelectWidget)

void UAC_CharacterSelectWidget::BindToLocalComponent(UAC_CharacterSelectComponent* localComponent)
{
	LocalComponent = localComponent;

	if (SlickCard)
	{
		SlickCard->RoleTag = ArcadeRoleTags::Role_Slick;
		SlickCard->OnCardClicked.RemoveAll(this);
		SlickCard->OnCardClicked.AddDynamic(this, &UAC_CharacterSelectWidget::HandleCardClicked);
		SlickCard->RefreshFromData();
	}
	if (FlintCard)
	{
		FlintCard->RoleTag = ArcadeRoleTags::Role_Flint;
		FlintCard->OnCardClicked.RemoveAll(this);
		FlintCard->OnCardClicked.AddDynamic(this, &UAC_CharacterSelectWidget::HandleCardClicked);
		FlintCard->RefreshFromData();
	}

	UAC_CharacterSelectComponent::OnAnyStateChanged.RemoveAll(this);
	UAC_CharacterSelectComponent::OnAnyStateChanged.AddUObject(this, &UAC_CharacterSelectWidget::RefreshFromState);

	RefreshFromState();
}

void UAC_CharacterSelectWidget::NativeDestruct()
{
	UAC_CharacterSelectComponent::OnAnyStateChanged.RemoveAll(this);
	Super::NativeDestruct();
}

void UAC_CharacterSelectWidget::HandleCardClicked(FGameplayTag roleTag)
{
	if (nullptr == LocalComponent || false == roleTag.IsValid())
	{
		return;
	}

	if (roleTag == LocalComponent->ChosenRole)
	{
		// Same card: toggle ready.
		LocalComponent->ServerSetReady(false == LocalComponent->bReady);
	}
	else
	{
		// Different card: switch role (server clears ready), then confirm.
		LocalComponent->ServerPickRole(roleTag);
		LocalComponent->ServerSetReady(true);
	}
}

void UAC_CharacterSelectWidget::RefreshFromState()
{
	const AActor* localPlayerState = LocalComponent ? LocalComponent->GetOwner() : nullptr;
	const UWorld* world = GetWorld();
	const AGameStateBase* gameState = world ? world->GetGameState() : nullptr;

	auto ApplyCard = [&](UAC_CharacterCardWidget* card, const FGameplayTag& role)
	{
		if (nullptr == card)
		{
			return;
		}

		if (gameState)
		{
			for (const APlayerState* playerState : gameState->PlayerArray)
			{
				const UAC_CharacterSelectComponent* comp = playerState ? playerState->FindComponentByClass<UAC_CharacterSelectComponent>() : nullptr;
				if (comp && comp->ChosenRole == role)
				{
					card->SetOccupancy(true, FText::FromString(playerState->GetPlayerName()),
						playerState == localPlayerState, comp->bReady);
					return;
				}
			}
		}
		card->SetOccupancy(false, FText::GetEmpty(), false, false);
	};

	ApplyCard(SlickCard, ArcadeRoleTags::Role_Slick);
	ApplyCard(FlintCard, ArcadeRoleTags::Role_Flint);

	// "Everyone ready" = at least one player and every player's component is ready.
	bool bEveryoneReady = gameState && gameState->PlayerArray.Num() > 0;
	if (bEveryoneReady)
	{
		for (const APlayerState* playerState : gameState->PlayerArray)
		{
			const UAC_CharacterSelectComponent* comp = playerState ? playerState->FindComponentByClass<UAC_CharacterSelectComponent>() : nullptr;
			if (nullptr == comp || false == comp->bReady)
			{
				bEveryoneReady = false;
				break;
			}
		}
	}
	OnSelectionStateChanged(bEveryoneReady);
}
