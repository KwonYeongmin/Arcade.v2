// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"

#include "UAC_CharacterSelectComponent.generated.h"

class APlayerController;
class UAC_CharacterSelectWidget;

/** Fires (on every client) whenever any player's select state replicates in. */
DECLARE_MULTICAST_DELEGATE(FOnAnyCharacterSelectStateChanged);

/**
 * UAC_CharacterSelectComponent
 *
 * One per PlayerState on the character-select map (added by AAC_CharacterSelectGameMode at
 * login). Because it lives on the PlayerState it replicates to every client, so each client can
 * see what all players picked. Holds this player's role + ready flag, routes the owning client's
 * choices to the server, and on the owning client creates / tears down the select widget.
 */
UCLASS(ClassGroup = (Arcade), meta = (BlueprintSpawnableComponent))
class LYRAGAME_API UAC_CharacterSelectComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UAC_CharacterSelectComponent();

	/** Broadcast on every client when any select component's replicated state changes. */
	static FOnAnyCharacterSelectStateChanged OnAnyStateChanged;

	/** Widget class the owning client shows. Set by the game mode before RegisterComponent. */
	UPROPERTY(ReplicatedUsing = OnRep_SelectWidgetClass)
	TSubclassOf<UAC_CharacterSelectWidget> SelectWidgetClass;

	/** This player's currently-held role (server-authoritative, replicated to all). */
	UPROPERTY(ReplicatedUsing = OnRep_State, BlueprintReadOnly, Category = "Character Select")
	FGameplayTag ChosenRole;

	UPROPERTY(ReplicatedUsing = OnRep_State, BlueprintReadOnly, Category = "Character Select")
	bool bReady = false;

	/** Client -> server: claim a role. Server resolves clashes by handing back the other role. */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Character Select")
	void ServerPickRole(FGameplayTag role);

	/** Client -> server: set the ready flag (a role change clears it server-side). */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Character Select")
	void ServerSetReady(bool bInReady);

	/** The PlayerController that owns this component's PlayerState, or null. */
	APlayerController* GetOwningPlayerController() const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type endPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	UFUNCTION()
	void OnRep_State();

	UFUNCTION()
	void OnRep_SelectWidgetClass();

	/** Idempotent: shows the widget on the owning client once its PC is local and the class is known. */
	void TryShowWidget();

	void NotifyServerSelectionChanged();

	UPROPERTY(Transient)
	TObjectPtr<UAC_CharacterSelectWidget> ActiveWidget;

	FTimerHandle ShowWidgetRetryTimer;
	int32 ShowWidgetAttempts = 0;
};
