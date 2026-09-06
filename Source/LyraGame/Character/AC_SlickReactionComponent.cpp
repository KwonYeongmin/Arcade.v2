// Copyright Epic Games, Inc. All Rights Reserved.

#include "AC_SlickReactionComponent.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Character/LyraPawnExtensionComponent.h"
#include "Weapons/AC_ProjectileLiquid.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Character.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AC_SlickReactionComponent)

UAC_SlickReactionComponent::UAC_SlickReactionComponent(const FObjectInitializer& objectInitializer)
	: Super(objectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UAC_SlickReactionComponent::BeginPlay()
{
	Super::BeginPlay();

	if (const ACharacter* ownerCharacter = Cast<ACharacter>(GetOwner()))
	{
		CachedMovement = ownerCharacter->GetCharacterMovement();
	}

	if (CachedMovement.IsValid())
	{
		BaseMaxWalkSpeed = CachedMovement->MaxWalkSpeed;
		BaseGravityScale = CachedMovement->GravityScale;
		bBaseValuesCached = true;
	}
	else
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[Slick] UAC_SlickReactionComponent on %s: owner has no UCharacterMovementComponent; slow/gravity debuff will not apply."),
			*GetNameSafe(GetOwner()));
	}

	if (false == SlickedStatusTag.IsValid())
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[Slick] UAC_SlickReactionComponent on %s: SlickedStatusTag is unset — the component will never react."),
			*GetNameSafe(GetOwner()));
		return;
	}

	// A Lyra pawn's ASC is initialized after BeginPlay (possession / PlayerState replication), so
	// a direct lookup here would silently return null. Go through the pawn extension component,
	// which fires immediately if the ASC is already up.
	if (ULyraPawnExtensionComponent* pawnExtComp = ULyraPawnExtensionComponent::FindPawnExtensionComponent(GetOwner()))
	{
		pawnExtComp->OnAbilitySystemInitialized_RegisterAndCall(
			FSimpleMulticastDelegate::FDelegate::CreateUObject(this, &UAC_SlickReactionComponent::OnPawnAbilitySystemInitialized));
		return;
	}

	// Non-Lyra owner: fall back to a one-shot direct lookup.
	UAbilitySystemComponent* asc = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(GetOwner());
	if (nullptr == asc)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[Slick] UAC_SlickReactionComponent on %s: no LyraPawnExtensionComponent and no ASC at BeginPlay — the component will never react."),
			*GetNameSafe(GetOwner()));
		return;
	}

	RegisterSlickedTagEvent(asc);
}

void UAC_SlickReactionComponent::OnPawnAbilitySystemInitialized()
{
	UAbilitySystemComponent* asc = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(GetOwner());
	if (nullptr == asc)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[Slick] UAC_SlickReactionComponent on %s: ability system reported initialized but no ASC was found."),
			*GetNameSafe(GetOwner()));
		return;
	}

	RegisterSlickedTagEvent(asc);
}

void UAC_SlickReactionComponent::RegisterSlickedTagEvent(UAbilitySystemComponent* asc)
{
	if (nullptr == asc || false == SlickedStatusTag.IsValid() || TagChangedDelegateHandle.IsValid())
	{
		return;
	}

	TagChangedDelegateHandle = asc->RegisterGameplayTagEvent(SlickedStatusTag, EGameplayTagEventType::NewOrRemoved)
		.AddUObject(this, &UAC_SlickReactionComponent::OnSlickedTagChanged);
	RegisteredASC = asc;

	// The tag may already be present (blob landed before this registration) — apply it once.
	OnSlickedTagChanged(SlickedStatusTag, asc->GetTagCount(SlickedStatusTag));
}

void UAC_SlickReactionComponent::EndPlay(const EEndPlayReason::Type endPlayReason)
{
	if (SlickedStatusTag.IsValid() && TagChangedDelegateHandle.IsValid())
	{
		if (UAbilitySystemComponent* asc = RegisteredASC.Get())
		{
			asc->RegisterGameplayTagEvent(SlickedStatusTag, EGameplayTagEventType::NewOrRemoved).Remove(TagChangedDelegateHandle);
		}
		TagChangedDelegateHandle.Reset();
		RegisteredASC.Reset();
	}

	Super::EndPlay(endPlayReason);
}

void UAC_SlickReactionComponent::OnSlickedTagChanged(const FGameplayTag /*tag*/, int32 newCount)
{
	if (newCount > 0)
	{
		RefreshMovementScale();
	}
	else
	{
		RestoreBaseMovement();
	}
}

void UAC_SlickReactionComponent::RefreshMovementScale()
{
	if (false == CachedMovement.IsValid() || false == bBaseValuesCached)
	{
		return;
	}

	// Only slow while Status.Slicked is actually present. If the debuff has expired the tag count
	// is 0 even though blobs may still be stuck (StuckLifeSeconds) — restore in that case.
	const bool bSlicked = RegisteredASC.IsValid() && RegisteredASC->GetTagCount(SlickedStatusTag) > 0;
	const float amount = bSlicked ? AAC_ProjectileLiquid::GetTotalSlickAmount(GetOwner()) : 0.0f;
	if (amount <= 0.0f)
	{
		RestoreBaseMovement();
		return;
	}

	const float alpha = FMath::Clamp(amount / AmountForFullSlow, 0.0f, 1.0f);
	CachedMovement->MaxWalkSpeed = BaseMaxWalkSpeed * FMath::Lerp(1.0f, MinSpeedMultiplier, alpha);
	CachedMovement->GravityScale = BaseGravityScale * FMath::Lerp(1.0f, MaxGravityMultiplier, alpha);
}

void UAC_SlickReactionComponent::RestoreBaseMovement()
{
	if (false == CachedMovement.IsValid() || false == bBaseValuesCached)
	{
		return;
	}

	CachedMovement->MaxWalkSpeed = BaseMaxWalkSpeed;
	CachedMovement->GravityScale = BaseGravityScale;
}
