// Copyright Epic Games, Inc. All Rights Reserved.

#include "UAC_ArcadeEnemyComponent.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/Attributes/LyraHealthSet.h"
#include "AbilitySystem/LyraAbilitySet.h"
#include "AbilitySystem/LyraAbilitySystemComponent.h"
#include "Arcade/Data/AC_ArcadeDataRows.h"
#include "Arcade/Data/AC_ArcadeDataSubsystem.h"
#include "Arcade/Data/AC_ArcadeDataTags.h"
#include "Arcade/Teams/UAC_ArcadeTeamCreation.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameplayEffect.h"
#include "GenericTeamAgentInterface.h"
#include "Net/UnrealNetwork.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(UAC_ArcadeEnemyComponent)

UAC_ArcadeEnemyComponent::UAC_ArcadeEnemyComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UAC_ArcadeEnemyComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ThisClass, MoveSpeed);
	DOREPLIFETIME(ThisClass, OrbitRadius);
	DOREPLIFETIME(ThisClass, OrbitHeight);
	DOREPLIFETIME(ThisClass, bFlying);
}

void UAC_ArcadeEnemyComponent::BeginPlay()
{
	Super::BeginPlay();

	if (GetOwnerRole() != ROLE_Authority)
	{
		return;
	}

	ApplyStatRow();

	// Enemy team, so ULyraDamageExecution's CanCauseDamage lets players hurt it (and it hurt them).
	// ALyraCharacter::SetGenericTeamId refuses once possessed ("driven by the associated
	// controller") -- the AIController owns it instead, and the character picks it up via
	// ILyraTeamAgentInterface's team-changed delegate (bound in ALyraCharacter::PossessedBy).
	const APawn* ownerPawn = Cast<APawn>(GetOwner());
	AController* ownerController = ownerPawn ? ownerPawn->GetController() : nullptr;
	if (IGenericTeamAgentInterface* teamAgent = Cast<IGenericTeamAgentInterface>(ownerController))
	{
		teamAgent->SetGenericTeamId(FGenericTeamId((uint8)UAC_ArcadeTeamCreation::EnemyTeamId));
	}

	if (const ACharacter* character = Cast<ACharacter>(GetOwner()))
	{
		if (UCapsuleComponent* capsule = character->GetCapsuleComponent())
		{
			capsule->OnComponentBeginOverlap.AddDynamic(this, &UAC_ArcadeEnemyComponent::HandleCapsuleOverlap);
		}
	}

	if (AbilitySet)
	{
		if (ULyraAbilitySystemComponent* lyraAsc = Cast<ULyraAbilitySystemComponent>(
			UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(GetOwner())))
		{
			AbilitySet->GiveToAbilitySystem(lyraAsc, /*OutGrantedHandles=*/nullptr, GetOwner());
		}
	}
}

void UAC_ArcadeEnemyComponent::StartAbilityCooldown(const FGameplayTag& abilityTag, float seconds)
{
	if (abilityTag.IsValid() && GetWorld())
	{
		AbilityReadyAt.Add(abilityTag, GetWorld()->GetTimeSeconds() + FMath::Max(seconds, 0.0f));
	}
}

bool UAC_ArcadeEnemyComponent::IsAbilityReady(const FGameplayTag& abilityTag) const
{
	const double* readyAt = AbilityReadyAt.Find(abilityTag);
	return nullptr == readyAt || (GetWorld() && GetWorld()->GetTimeSeconds() >= *readyAt);
}

void UAC_ArcadeEnemyComponent::ApplyStatRow()
{
	const UAC_ArcadeDataSubsystem* data = UAC_ArcadeDataSubsystem::Get(this);
	const FAC_EnemyStatRow* row = data ? data->FindRow<FAC_EnemyStatRow>(ArcadeDataTags::Table_Enemies, StatRowName) : nullptr;
	if (row)
	{
		MoveSpeed = row->MoveSpeed;
		OrbitRadius = row->OrbitRadius;
		OrbitHeight = row->OrbitHeight;
		bFlying = row->bFlying;
		ContactDamage = row->ContactDamage;
	}
	else
	{
		// Missing data should degrade to the component's own defaults, not silently break
		// movement — the flying-mode switch below still has to run either way.
		UE_LOG(LogTemp, Warning, TEXT("[ArcadeEnemy] %s: no DT_Enemies row '%s' (subsystem ready=%d) — using component defaults"),
			*GetNameSafe(GetOwner()), *StatRowName.ToString(), nullptr != data);
	}

	if (ACharacter* character = Cast<ACharacter>(GetOwner()))
	{
		if (UCharacterMovementComponent* movement = character->GetCharacterMovement())
		{
			movement->MaxWalkSpeed = MoveSpeed;
			movement->MaxFlySpeed = MoveSpeed;
			movement->MaxAcceleration = MoveSpeed * 8.0f;
			if (bFlying)
			{
				movement->SetMovementMode(MOVE_Flying);
				movement->GravityScale = 0.0f;
				movement->bOrientRotationToMovement = false;
			}
		}
	}

	if (row)
	{
		if (UAbilitySystemComponent* asc = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(GetOwner()))
		{
			asc->SetNumericAttributeBase(ULyraHealthSet::GetMaxHealthAttribute(), row->MaxHealth);
			asc->SetNumericAttributeBase(ULyraHealthSet::GetHealthAttribute(), row->MaxHealth);
		}
	}
}

void UAC_ArcadeEnemyComponent::HandleCapsuleOverlap(UPrimitiveComponent* /*overlappedComp*/, AActor* otherActor,
	UPrimitiveComponent* /*otherComp*/, int32 /*otherBodyIndex*/, bool /*bFromSweep*/, const FHitResult& /*sweepResult*/)
{
	if (GetOwnerRole() != ROLE_Authority || nullptr == otherActor || otherActor == GetOwner())
	{
		return;
	}
	if (nullptr == ContactDamageGameplayEffectClass || ContactDamage <= 0.0f)
	{
		return;
	}

	UAbilitySystemComponent* targetAsc = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(otherActor);
	UAbilitySystemComponent* selfAsc = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(GetOwner());
	if (nullptr == targetAsc || nullptr == selfAsc)
	{
		return;
	}

	const float now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	if (const float* last = LastContactDamageTime.Find(otherActor))
	{
		if (now - *last < ContactDamageInterval)
		{
			return;
		}
	}
	LastContactDamageTime.Add(otherActor, now);

	FGameplayEffectContextHandle context = selfAsc->MakeEffectContext();
	context.AddInstigator(GetOwner(), GetOwner());

	const FGameplayEffectSpecHandle specHandle = selfAsc->MakeOutgoingSpec(ContactDamageGameplayEffectClass, ContactDamage, context);
	if (false == specHandle.IsValid())
	{
		return;
	}
	if (ContactDamageSetByCallerTag.IsValid())
	{
		specHandle.Data->SetSetByCallerMagnitude(ContactDamageSetByCallerTag, ContactDamage);
	}
	selfAsc->ApplyGameplayEffectSpecToTarget(*specHandle.Data.Get(), targetAsc);
}
