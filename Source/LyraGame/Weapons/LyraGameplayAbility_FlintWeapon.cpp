// Copyright Epic Games, Inc. All Rights Reserved.

#include "LyraGameplayAbility_FlintWeapon.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "AC_ProjectileLiquid.h"
#include "AC_SlickExplosion.h"
#include "Arcade/Data/AC_ArcadeDataRows.h"
#include "Arcade/Data/AC_ArcadeDataSubsystem.h"
#include "Arcade/Data/AC_ArcadeDataTags.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LyraGameplayAbility_FlintWeapon)

void ULyraGameplayAbility_FlintWeapon::OnRangedWeaponTargetDataReady_Implementation(const FGameplayAbilityTargetDataHandle& targetData)
{
	Super::OnRangedWeaponTargetDataReady_Implementation(targetData);

	APawn* const avatarPawn = Cast<APawn>(GetAvatarActorFromActorInfo());
	if (nullptr == avatarPawn || false == avatarPawn->HasAuthority())
	{
		return;
	}

	for (int32 index = 0; index < targetData.Num(); ++index)
	{
		const FGameplayAbilityTargetData* data = targetData.Get(index);
		if (nullptr == data)
		{
			continue;
		}

		const FHitResult* hit = data->GetHitResult();
		if (nullptr == hit || false == hit->bBlockingHit)
		{
			continue;
		}

		AActor* hitActor = hit->GetActor();
		if (nullptr == hitActor)
		{
			continue;
		}

		ApplyDirectDamage(hitActor, *hit);
		TrySpawnExplosion(hitActor, *hit);
	}
}

float ULyraGameplayAbility_FlintWeapon::ResolveDirectDamage() const
{
	if (CombatRowName.IsNone())
	{
		return DirectDamage;
	}

	const UAC_ArcadeDataSubsystem* data = UAC_ArcadeDataSubsystem::Get(this);
	if (nullptr == data)
	{
		return DirectDamage;
	}

	const FAC_DamageConfigRow* row = data->FindRow<FAC_DamageConfigRow>(ArcadeDataTags::Table_Combat, CombatRowName);
	return row ? row->BaseDamage : DirectDamage;
}

void ULyraGameplayAbility_FlintWeapon::ApplyDirectDamage(AActor* hitActor, const FHitResult& hit)
{
	if (nullptr == DirectDamageGameplayEffectClass)
	{
		return;
	}

	UAbilitySystemComponent* targetAsc = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(hitActor);
	if (nullptr == targetAsc)
	{
		return;
	}

	APawn* const avatarPawn = Cast<APawn>(GetAvatarActorFromActorInfo());
	UAbilitySystemComponent* sourceAsc = GetAbilitySystemComponentFromActorInfo();
	UAbilitySystemComponent* effectCauserAsc = sourceAsc ? sourceAsc : targetAsc;

	FGameplayEffectContextHandle context = effectCauserAsc->MakeEffectContext();
	context.AddInstigator(avatarPawn, avatarPawn);
	context.AddHitResult(hit);

	const float directDamage = ResolveDirectDamage();

	const FGameplayEffectSpecHandle specHandle = effectCauserAsc->MakeOutgoingSpec(DirectDamageGameplayEffectClass, directDamage, context);
	if (false == specHandle.IsValid())
	{
		return;
	}

	if (DirectDamageSetByCallerTag.IsValid())
	{
		specHandle.Data->SetSetByCallerMagnitude(DirectDamageSetByCallerTag, directDamage);
	}

	effectCauserAsc->ApplyGameplayEffectSpecToTarget(*specHandle.Data.Get(), targetAsc);
}

void ULyraGameplayAbility_FlintWeapon::TrySpawnExplosion(AActor* hitActor, const FHitResult& hit)
{
	if (nullptr == ExplosionClass)
	{
		return;
	}

	const float slickAmount = AAC_ProjectileLiquid::GetTotalSlickAmount(hitActor);
	if (slickAmount <= 0.0f)
	{
		return;
	}

	UWorld* const world = GetWorld();
	APawn* const avatarPawn = Cast<APawn>(GetAvatarActorFromActorInfo());
	if (nullptr == world || nullptr == avatarPawn)
	{
		return;
	}

	FActorSpawnParameters spawnParams;
	spawnParams.Owner = avatarPawn;
	spawnParams.Instigator = avatarPawn;
	spawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	// Centre the blast on the detonated target, not the raw ray impact point — a hitscan grazing the
	// silhouette can land its impact well off the actor, and the explosion should be about the victim.
	const FVector explosionLocation = hitActor->GetActorLocation();
	AAC_SlickExplosion* explosion =
		world->SpawnActor<AAC_SlickExplosion>(ExplosionClass, explosionLocation, FRotator::ZeroRotator, spawnParams);
	if (explosion)
	{
		explosion->Detonate(slickAmount, avatarPawn);
	}

	AAC_ProjectileLiquid::ConsumeSlickOn(hitActor);
}

bool ULyraGameplayAbility_FlintWeapon::IsAimingAtSlickedTarget(APawn* SourcePawn, float Range)
{
	// Static so the reticle widget — which only has the weapon instance and the local pawn, never a
	// live ability instance — can call it every frame without tripping the ability-instance ensures.
	if (nullptr == SourcePawn || Range <= 0.0f)
	{
		return false;
	}

	UWorld* const world = SourcePawn->GetWorld();
	if (nullptr == world)
	{
		return false;
	}

	// Mirrors ELyraAbilityTargetingSource::CameraTowardsFocus without needing the ability instance.
	FVector viewLocation = FVector::ZeroVector;
	FRotator viewRotation = FRotator::ZeroRotator;
	if (APlayerController* pc = Cast<APlayerController>(SourcePawn->GetController()))
	{
		pc->GetPlayerViewPoint(viewLocation, viewRotation);
	}
	else
	{
		SourcePawn->GetActorEyesViewPoint(viewLocation, viewRotation);
	}

	const FVector start = viewLocation;
	const FVector end = start + viewRotation.Vector() * Range;

	FCollisionQueryParams queryParams(SCENE_QUERY_STAT(FlintAimDetection), /*bTraceComplex=*/false);
	queryParams.AddIgnoredActor(SourcePawn);

	FHitResult hit;
	if (false == world->LineTraceSingleByChannel(hit, start, end, ECC_Pawn, queryParams))
	{
		return false;
	}

	return AAC_ProjectileLiquid::GetTotalSlickAmount(hit.GetActor()) > 0.0f;
}

bool ULyraGameplayAbility_FlintWeapon::IsAimingAtSlickedTargetNow() const
{
	if (false == IsInstantiated() || nullptr == CurrentActorInfo)
	{
		return false;
	}

	return IsAimingAtSlickedTarget(Cast<APawn>(GetAvatarActorFromActorInfo()), DetectionRange);
}
