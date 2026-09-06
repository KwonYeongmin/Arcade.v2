// Copyright Epic Games, Inc. All Rights Reserved.

#include "LyraGameplayAbility_ProjectileWeapon.h"

#include "AC_Projectile.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "LyraRangedWeaponInstance.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LyraGameplayAbility_ProjectileWeapon)

AActor* ULyraGameplayAbility_ProjectileWeapon::GetSpawnedWeaponActor() const
{
	if (const ULyraRangedWeaponInstance* WeaponInstance = GetWeaponInstance())
	{
		for (AActor* Actor : WeaponInstance->GetSpawnedActors())
		{
			if (IsValid(Actor))
			{
				return Actor;
			}
		}
	}
	return nullptr;
}

FVector ULyraGameplayAbility_ProjectileWeapon::ResolveMuzzleLocation() const
{
	if (const AActor* WeaponActor = GetSpawnedWeaponActor())
	{
		if (const USkeletalMeshComponent* Mesh = WeaponActor->FindComponentByClass<USkeletalMeshComponent>())
		{
			if (Mesh->DoesSocketExist(MuzzleSocketName))
			{
				return Mesh->GetSocketLocation(MuzzleSocketName);
			}
			return Mesh->GetComponentLocation();
		}
	}

	// No weapon mesh available — fall back to the same source the hitscan trace starts from.
	return GetWeaponTargetingSourceLocation();
}

void ULyraGameplayAbility_ProjectileWeapon::OnRangedWeaponTargetDataReady_Implementation(const FGameplayAbilityTargetDataHandle& TargetData)
{
	Super::OnRangedWeaponTargetDataReady_Implementation(TargetData);

	if (!ProjectileClass)
	{
		return;
	}

	// Only the server spawns the damaging projectile; it replicates to clients.
	APawn* const AvatarPawn = Cast<APawn>(GetAvatarActorFromActorInfo());
	if (!AvatarPawn || !AvatarPawn->HasAuthority())
	{
		return;
	}

	UWorld* const World = GetWorld();
	if (!World)
	{
		return;
	}

	AActor* const WeaponActor = GetSpawnedWeaponActor();
	const FVector MuzzleLocation = ResolveMuzzleLocation();

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = WeaponActor ? WeaponActor : Cast<AActor>(AvatarPawn);
	SpawnParams.Instigator = AvatarPawn;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	for (int32 Index = 0; Index < TargetData.Num(); ++Index)
	{
		const FGameplayAbilityTargetData* Data = TargetData.Get(Index);
		if (!Data)
		{
			continue;
		}

		const FHitResult* Hit = Data->GetHitResult();
		if (!Hit)
		{
			continue;
		}

		// The base ability always leaves an entry per bullet: a real hit, or a fake impact at
		// the end of the trace on a miss. Either way ImpactPoint is the aim point.
		const FVector AimPoint = Hit->bBlockingHit ? Hit->ImpactPoint : Hit->TraceEnd;
		const FVector Direction = (AimPoint - MuzzleLocation).GetSafeNormal();
		if (Direction.IsNearlyZero())
		{
			continue;
		}

		AAC_Projectile* const Projectile = World->SpawnActor<AAC_Projectile>(
			ProjectileClass, MuzzleLocation, Direction.Rotation(), SpawnParams);
		if (Projectile)
		{
			Projectile->LaunchInDirection(Direction);
		}
	}
}
