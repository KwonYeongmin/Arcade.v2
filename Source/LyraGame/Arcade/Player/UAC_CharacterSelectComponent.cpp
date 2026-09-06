// Copyright Epic Games, Inc. All Rights Reserved.

#include "UAC_CharacterSelectComponent.h"

#include "Arcade/AC_ArcadeRoleTags.h"
#include "Arcade/GameModes/AAC_CharacterSelectGameMode.h"
#include "Arcade/UI/UAC_CharacterSelectWidget.h"
#include "Blueprint/UserWidget.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(UAC_CharacterSelectComponent)

FOnAnyCharacterSelectStateChanged UAC_CharacterSelectComponent::OnAnyStateChanged;

UAC_CharacterSelectComponent::UAC_CharacterSelectComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UAC_CharacterSelectComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ThisClass, SelectWidgetClass);
	DOREPLIFETIME(ThisClass, ChosenRole);
	DOREPLIFETIME(ThisClass, bReady);
}

APlayerController* UAC_CharacterSelectComponent::GetOwningPlayerController() const
{
	const APlayerState* playerState = Cast<APlayerState>(GetOwner());
	return playerState ? playerState->GetPlayerController() : nullptr;
}

void UAC_CharacterSelectComponent::BeginPlay()
{
	Super::BeginPlay();
	TryShowWidget();
}

void UAC_CharacterSelectComponent::OnRep_SelectWidgetClass()
{
	TryShowWidget();
}

void UAC_CharacterSelectComponent::TryShowWidget()
{
	if (ActiveWidget)
	{
		return;
	}

	APlayerController* playerController = GetOwningPlayerController();
	const bool bLocal = playerController && playerController->IsLocalController();

	if (false == bLocal || nullptr == SelectWidgetClass)
	{
		if (UWorld* world = GetWorld())
		{
			if (++ShowWidgetAttempts <= 40)
			{
				world->GetTimerManager().SetTimer(ShowWidgetRetryTimer, this,
					&UAC_CharacterSelectComponent::TryShowWidget, 0.25f, false);
			}
			else
			{
				UE_LOG(LogTemp, Warning,
					TEXT("[CharSelect] gave up showing the select widget (local=%d class=%s)"),
					(int32)bLocal, *GetNameSafe(SelectWidgetClass));
			}
		}
		return;
	}

	ActiveWidget = CreateWidget<UAC_CharacterSelectWidget>(playerController, SelectWidgetClass);
	if (nullptr == ActiveWidget)
	{
		return;
	}
	ActiveWidget->AddToViewport();
	ActiveWidget->BindToLocalComponent(this);

	FInputModeUIOnly inputMode;
	inputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	playerController->SetInputMode(inputMode);
	playerController->SetShowMouseCursor(true);
}

void UAC_CharacterSelectComponent::EndPlay(const EEndPlayReason::Type endPlayReason)
{
	if (UWorld* world = GetWorld())
	{
		world->GetTimerManager().ClearTimer(ShowWidgetRetryTimer);
	}

	if (ActiveWidget)
	{
		ActiveWidget->RemoveFromParent();
		ActiveWidget = nullptr;

		// Hand input back to the game before we travel — a stale UI-only mode leaves the next
		// level unresponsive.
		if (APlayerController* playerController = GetOwningPlayerController())
		{
			if (playerController->IsLocalController())
			{
				playerController->SetInputMode(FInputModeGameOnly());
				playerController->SetShowMouseCursor(false);
			}
		}
	}

	Super::EndPlay(endPlayReason);
}

void UAC_CharacterSelectComponent::OnRep_State()
{
	OnAnyStateChanged.Broadcast();
}

void UAC_CharacterSelectComponent::NotifyServerSelectionChanged()
{
	// Refresh the listen-server host's own UI (no OnRep for locally-authored changes)...
	OnAnyStateChanged.Broadcast();

	// ...and let the game mode check whether everyone is ready.
	if (AAC_CharacterSelectGameMode* gameMode = GetWorld() ? GetWorld()->GetAuthGameMode<AAC_CharacterSelectGameMode>() : nullptr)
	{
		gameMode->NotifySelectionChanged();
	}
}

void UAC_CharacterSelectComponent::ServerPickRole_Implementation(FGameplayTag role)
{
	if (false == role.IsValid())
	{
		return;
	}

	const FGameplayTag requested = (role == ArcadeRoleTags::Role_Flint) ? ArcadeRoleTags::Role_Flint : ArcadeRoleTags::Role_Slick;
	const FGameplayTag other = (requested == ArcadeRoleTags::Role_Flint) ? ArcadeRoleTags::Role_Slick : ArcadeRoleTags::Role_Flint;

	bool bRequestedTaken = false;
	if (const UWorld* world = GetWorld())
	{
		if (const AGameStateBase* gameState = world->GetGameState())
		{
			for (const APlayerState* playerState : gameState->PlayerArray)
			{
				if (nullptr == playerState || playerState == GetOwner())
				{
					continue;
				}
				const UAC_CharacterSelectComponent* otherComp = playerState->FindComponentByClass<UAC_CharacterSelectComponent>();
				if (otherComp && otherComp->ChosenRole == requested)
				{
					bRequestedTaken = true;
					break;
				}
			}
		}
	}

	ChosenRole = bRequestedTaken ? other : requested;
	bReady = false;
	NotifyServerSelectionChanged();
}

void UAC_CharacterSelectComponent::ServerSetReady_Implementation(bool bInReady)
{
	if (bInReady && false == ChosenRole.IsValid())
	{
		return;
	}

	bReady = bInReady;
	NotifyServerSelectionChanged();
}
