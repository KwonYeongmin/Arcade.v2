// Copyright Epic Games, Inc. All Rights Reserved.

#include "UAC_CharacterCardWidget.h"

#include "Arcade/Data/AC_ArcadeDataRows.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Engine/DataTable.h"
#include "Engine/Texture2D.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(UAC_CharacterCardWidget)

void UAC_CharacterCardWidget::NativePreConstruct()
{
	Super::NativePreConstruct();
	RefreshFromData();
}

void UAC_CharacterCardWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (ReadyButton)
	{
		ReadyButton->OnClicked.RemoveAll(this);
		ReadyButton->OnClicked.AddDynamic(this, &UAC_CharacterCardWidget::HandleReadyButtonClicked);
	}

	SetOccupancy(false, FText::GetEmpty(), false, false);
}

void UAC_CharacterCardWidget::RefreshFromData()
{
	if (nullptr == CardTable || false == RoleTag.IsValid())
	{
		return;
	}

	const FAC_CharacterCardRow* match = nullptr;
	TArray<FAC_CharacterCardRow*> rows;
	CardTable->GetAllRows<FAC_CharacterCardRow>(TEXT("UAC_CharacterCardWidget::RefreshFromData"), rows);
	for (const FAC_CharacterCardRow* row : rows)
	{
		if (row && row->RoleTag == RoleTag)
		{
			match = row;
			break;
		}
	}

	if (nullptr == match)
	{
		return;
	}

	if (NameText)
	{
		NameText->SetText(match->DisplayName);
		NameText->SetColorAndOpacity(FSlateColor(match->AccentColor));
	}
	if (TaglineText)
	{
		TaglineText->SetText(match->Tagline);
	}
	if (PortraitImage)
	{
		if (UTexture2D* portrait = match->Portrait.LoadSynchronous())
		{
			PortraitImage->SetBrushFromTexture(portrait);
		}
	}
	if (WeaponIconImage)
	{
		if (UTexture2D* icon = match->WeaponIcon.LoadSynchronous())
		{
			WeaponIconImage->SetBrushFromTexture(icon);
		}
	}
}

void UAC_CharacterCardWidget::SetOccupancy(bool bOccupied, const FText& occupantName, bool bLocalPlayer, bool bReady)
{
	bIsOccupied = bOccupied;

	if (StatusText)
	{
		if (false == bOccupied)
		{
			StatusText->SetText(EmptyStatusText);
		}
		else
		{
			const FText& format = bReady ? ReadyStatusFormat : SelectingStatusFormat;
			StatusText->SetText(FText::Format(format, occupantName));
		}
	}

	OnOccupancyChanged(bOccupied, bLocalPlayer, bReady);
}

void UAC_CharacterCardWidget::HandleReadyButtonClicked()
{
	OnCardClicked.Broadcast(RoleTag);
}
