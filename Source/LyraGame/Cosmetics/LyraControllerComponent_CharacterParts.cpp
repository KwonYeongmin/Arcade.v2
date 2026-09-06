// Copyright Epic Games, Inc. All Rights Reserved.

#include "Cosmetics/LyraControllerComponent_CharacterParts.h"
#include "Cosmetics/LyraCharacterPartTypes.h"
#include "Cosmetics/LyraPawnComponent_CharacterParts.h"
#include "GameFramework/CheatManagerDefines.h"
#include "LyraCosmeticDeveloperSettings.h"
#include "GameFramework/Pawn.h"
#include "Vehicles/AC_MotorcyclePawn.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LyraControllerComponent_CharacterParts)

//////////////////////////////////////////////////////////////////////

ULyraControllerComponent_CharacterParts::ULyraControllerComponent_CharacterParts(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void ULyraControllerComponent_CharacterParts::BeginPlay()
{
	Super::BeginPlay();

	// Listen for pawn possession changed events
	if (HasAuthority())
	{
		if (AController* OwningController = GetController<AController>())
		{
			OwningController->OnPossessedPawnChanged.AddDynamic(this, &ThisClass::OnPossessedPawnChanged);

			if (APawn* ControlledPawn = GetPawn<APawn>())
			{
				OnPossessedPawnChanged(nullptr, ControlledPawn);
			}
		}

		ApplyDeveloperSettings();
	}
}

void ULyraControllerComponent_CharacterParts::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	RemoveAllCharacterParts();
	Super::EndPlay(EndPlayReason);
}

ULyraPawnComponent_CharacterParts* ULyraControllerComponent_CharacterParts::GetPawnCustomizer() const
{
	if (APawn* ControlledPawn = GetPawn<APawn>())
	{
		if (ULyraPawnComponent_CharacterParts* Customizer = ControlledPawn->FindComponentByClass<ULyraPawnComponent_CharacterParts>())
		{
			return Customizer;
		}
	}
	if (APawn* PreservedPawn = VehiclePreservedPawn.Get())
	{
		return PreservedPawn->FindComponentByClass<ULyraPawnComponent_CharacterParts>();
	}
	return nullptr;
}

void ULyraControllerComponent_CharacterParts::AddCharacterPart(const FLyraCharacterPart& NewPart)
{
	AddCharacterPartInternal(NewPart, ECharacterPartSource::Natural);
}

void ULyraControllerComponent_CharacterParts::AddCharacterPartInternal(const FLyraCharacterPart& NewPart, ECharacterPartSource Source)
{
	FLyraControllerCharacterPartEntry& NewEntry = CharacterParts.AddDefaulted_GetRef();
	NewEntry.Part = NewPart;
	NewEntry.Source = Source;

	if (ULyraPawnComponent_CharacterParts* PawnCustomizer = GetPawnCustomizer())
	{
		if (NewEntry.Source != ECharacterPartSource::NaturalSuppressedViaCheat)
		{
			NewEntry.Handle = PawnCustomizer->AddCharacterPart(NewPart);
		}
	}

}

void ULyraControllerComponent_CharacterParts::RemoveCharacterPart(const FLyraCharacterPart& PartToRemove)
{
	for (auto EntryIt = CharacterParts.CreateIterator(); EntryIt; ++EntryIt)
	{
		if (FLyraCharacterPart::AreEquivalentParts(EntryIt->Part, PartToRemove))
		{
			if (ULyraPawnComponent_CharacterParts* PawnCustomizer = GetPawnCustomizer())
			{
				PawnCustomizer->RemoveCharacterPart(EntryIt->Handle);
			}

			EntryIt.RemoveCurrent();
			break;
		}
	}
}

void ULyraControllerComponent_CharacterParts::RemoveAllCharacterParts()
{
	if (ULyraPawnComponent_CharacterParts* PawnCustomizer = GetPawnCustomizer())
	{
		for (FLyraControllerCharacterPartEntry& Entry : CharacterParts)
		{
			PawnCustomizer->RemoveCharacterPart(Entry.Handle);
		}
	}

	CharacterParts.Reset();
}

void ULyraControllerComponent_CharacterParts::OnPossessedPawnChanged(APawn* OldPawn, APawn* NewPawn)
{
	// AController::Possess first calls UnPossess and broadcasts OldPawn -> nullptr,
	// then broadcasts OldPawn -> NewPawn after the new possession succeeds.  Preserve
	// parts across that intermediate notification in both mount and dismount flows.
	if (NewPawn == nullptr)
	{
		if (VehiclePreservedPawn.IsValid())
		{
			return;
		}

		if (OldPawn)
		{
			if (const AAC_MotorcyclePawn* MountedMotorcycle =
				Cast<AAC_MotorcyclePawn>(OldPawn->GetAttachParentActor()))
			{
				if (MountedMotorcycle->GetRider() == OldPawn)
				{
					VehiclePreservedPawn = OldPawn;
					return;
				}
			}
		}
	}

	// A mounted rider remains visible while the controller temporarily possesses the
	// motorcycle.  Moving its parts to a pawn with no character-parts component would
	// clear the rider's body mesh and recreate its AnimInstance on dismount.
	if (const AAC_MotorcyclePawn* NewMotorcycle = Cast<AAC_MotorcyclePawn>(NewPawn))
	{
		if (OldPawn && NewMotorcycle->GetRider() == OldPawn)
		{
			VehiclePreservedPawn = OldPawn;
			return;
		}
	}

	if (APawn* PreservedPawn = VehiclePreservedPawn.Get())
	{
		if (NewPawn == PreservedPawn)
		{
			VehiclePreservedPawn.Reset();
			return;
		}

		// The controller left the vehicle flow without returning to its rider.  Clean up
		// the preserved pawn before applying the parts to an unrelated pawn.
		if (ULyraPawnComponent_CharacterParts* PreservedCustomizer =
			PreservedPawn->FindComponentByClass<ULyraPawnComponent_CharacterParts>())
		{
			for (FLyraControllerCharacterPartEntry& Entry : CharacterParts)
			{
				PreservedCustomizer->RemoveCharacterPart(Entry.Handle);
				Entry.Handle.Reset();
			}
		}
		VehiclePreservedPawn.Reset();
	}

	// Remove from the old pawn
	if (ULyraPawnComponent_CharacterParts* OldCustomizer = OldPawn ? OldPawn->FindComponentByClass<ULyraPawnComponent_CharacterParts>() : nullptr)
	{
		for (FLyraControllerCharacterPartEntry& Entry : CharacterParts)
		{
			OldCustomizer->RemoveCharacterPart(Entry.Handle);
			Entry.Handle.Reset();
		}
	}

	// Apply to the new pawn
	if (ULyraPawnComponent_CharacterParts* NewCustomizer = NewPawn ? NewPawn->FindComponentByClass<ULyraPawnComponent_CharacterParts>() : nullptr)
	{
		for (FLyraControllerCharacterPartEntry& Entry : CharacterParts)
		{
			// Don't readd if it's already there, this can get called with a null oldpawn
			if (!Entry.Handle.IsValid() && Entry.Source != ECharacterPartSource::NaturalSuppressedViaCheat)
			{
				Entry.Handle = NewCustomizer->AddCharacterPart(Entry.Part);
			}
		}
	}
}

void ULyraControllerComponent_CharacterParts::ApplyDeveloperSettings()
{
#if UE_WITH_CHEAT_MANAGER
	const ULyraCosmeticDeveloperSettings* Settings = GetDefault<ULyraCosmeticDeveloperSettings>();

	// Suppress or unsuppress natural parts if needed
	const bool bSuppressNaturalParts = (Settings->CheatMode == ECosmeticCheatMode::ReplaceParts) && (Settings->CheatCosmeticCharacterParts.Num() > 0);
	SetSuppressionOnNaturalParts(bSuppressNaturalParts);

	// Remove anything added by developer settings and re-add it
	ULyraPawnComponent_CharacterParts* PawnCustomizer = GetPawnCustomizer();
	for (auto It = CharacterParts.CreateIterator(); It; ++It)
	{
		if (It->Source == ECharacterPartSource::AppliedViaDeveloperSettingsCheat)
		{
			if (PawnCustomizer != nullptr)
			{
				PawnCustomizer->RemoveCharacterPart(It->Handle);
			}
			It.RemoveCurrent();
		}
	}

	// Add new parts
	for (const FLyraCharacterPart& PartDesc : Settings->CheatCosmeticCharacterParts)
	{
		AddCharacterPartInternal(PartDesc, ECharacterPartSource::AppliedViaDeveloperSettingsCheat);
	}
#endif
}


void ULyraControllerComponent_CharacterParts::AddCheatPart(const FLyraCharacterPart& NewPart, bool bSuppressNaturalParts)
{
#if UE_WITH_CHEAT_MANAGER
	SetSuppressionOnNaturalParts(bSuppressNaturalParts);
	AddCharacterPartInternal(NewPart, ECharacterPartSource::AppliedViaCheatManager);
#endif
}

void ULyraControllerComponent_CharacterParts::ClearCheatParts()
{
#if UE_WITH_CHEAT_MANAGER
	ULyraPawnComponent_CharacterParts* PawnCustomizer = GetPawnCustomizer();

	// Remove anything added by cheat manager cheats
	for (auto It = CharacterParts.CreateIterator(); It; ++It)
	{
		if (It->Source == ECharacterPartSource::AppliedViaCheatManager)
		{
			if (PawnCustomizer != nullptr)
			{
				PawnCustomizer->RemoveCharacterPart(It->Handle);
			}
			It.RemoveCurrent();
		}
	}

	ApplyDeveloperSettings();
#endif
}

void ULyraControllerComponent_CharacterParts::SetSuppressionOnNaturalParts(bool bSuppressed)
{
#if UE_WITH_CHEAT_MANAGER
	ULyraPawnComponent_CharacterParts* PawnCustomizer = GetPawnCustomizer();

	for (FLyraControllerCharacterPartEntry& Entry : CharacterParts)
	{
		if ((Entry.Source == ECharacterPartSource::Natural) && bSuppressed)
		{
			// Suppress
			if (PawnCustomizer != nullptr)
			{
				PawnCustomizer->RemoveCharacterPart(Entry.Handle);
				Entry.Handle.Reset();
			}
			Entry.Source = ECharacterPartSource::NaturalSuppressedViaCheat;
		}
		else if ((Entry.Source == ECharacterPartSource::NaturalSuppressedViaCheat) && !bSuppressed)
		{
			// Unsuppress
			if (PawnCustomizer != nullptr)
			{
				Entry.Handle = PawnCustomizer->AddCharacterPart(Entry.Part);
			}
			Entry.Source = ECharacterPartSource::Natural;
		}
	}
#endif
}

