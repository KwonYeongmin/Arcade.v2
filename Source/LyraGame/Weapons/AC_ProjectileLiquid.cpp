// Copyright Epic Games, Inc. All Rights Reserved.

#include "AC_ProjectileLiquid.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Character/AC_SlickReactionComponent.h"
#include "Components/SphereComponent.h"
#include "Engine/World.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "GameplayEffect.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AC_ProjectileLiquid)

AAC_ProjectileLiquid::AAC_ProjectileLiquid(const FObjectInitializer& objectInitializer)
	: Super(objectInitializer)
{
	ConfigureCollision();
}

void AAC_ProjectileLiquid::ConfigureCollision() const
{
	// Dedicated object channel (Config/DefaultEngine.ini "Arcade_ObjectChannel_LiquidBlob"):
	//  - BLOCK WorldStatic + WorldDynamic  -> stick to any wall / floor / prop
	//  - OVERLAP Pawn                      -> HandleOverlap applies the Slicked debuff
	//  - OVERLAP other blobs               -> they merge
	//  - IGNORE everything else (incl. weapon trace channels, so Flint hitscan passes through)
	CollisionComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	CollisionComponent->SetCollisionObjectType(LiquidBlobObjectChannel);
	CollisionComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
	CollisionComponent->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
	CollisionComponent->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
	CollisionComponent->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	CollisionComponent->SetCollisionResponseToChannel(LiquidBlobObjectChannel, ECR_Overlap);
	CollisionComponent->SetGenerateOverlapEvents(true);
	CollisionComponent->SetNotifyRigidBodyCollision(true);
}

void AAC_ProjectileLiquid::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AAC_ProjectileLiquid, LiquidState);
	DOREPLIFETIME(AAC_ProjectileLiquid, SizeMultiplier);
}

void AAC_ProjectileLiquid::BeginPlay()
{
	Super::BeginPlay();

	// Re-apply at runtime so a B_Projectile_* Blueprint's SCS component (which can carry serialized
	// collision overrides that shadow the constructor) cannot break the blob's collision rules.
	ConfigureCollision();

	if (HasAuthority() && false == FMath::IsNearlyEqual(InitialSizeMultiplierRange.X, InitialSizeMultiplierRange.Y))
	{
		SizeMultiplier = FMath::FRandRange(InitialSizeMultiplierRange.X, InitialSizeMultiplierRange.Y);
	}

	ApplyScaleFromSize();

	if (HasAuthority())
	{
		CollisionComponent->OnComponentBeginOverlap.AddDynamic(this, &AAC_ProjectileLiquid::HandleOverlap);
	}
}

void AAC_ProjectileLiquid::EndPlay(const EEndPlayReason::Type endPlayReason)
{
	// The only release site for GE_Slicked. Reaching here covers every death path — explosion
	// consumption, StuckLifeSeconds expiry, immediate Destroy() when bStickOnHit is false,
	// target death and level teardown — so the debuff can never outlive its blob.
	if (ActiveSlickedGEHandle.IsValid())
	{
		AActor* const target = SlickedTargetActor.Get();
		if (UAbilitySystemComponent* targetAsc = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(target))
		{
			targetAsc->RemoveActiveGameplayEffect(ActiveSlickedGEHandle);
		}
		ActiveSlickedGEHandle.Invalidate();

		// Remaining blobs (if any) mean a lower total — let the reaction component recompute.
		NotifySlickAmountChanged(target);
	}

	Super::EndPlay(endPlayReason);
}

void AAC_ProjectileLiquid::NotifySlickAmountChanged(AActor* target)
{
	if (nullptr == target)
	{
		return;
	}

	if (UAC_SlickReactionComponent* reaction = target->FindComponentByClass<UAC_SlickReactionComponent>())
	{
		reaction->RefreshMovementScale();
	}
}

float AAC_ProjectileLiquid::GetTotalSlickAmount(const AActor* Target)
{
	TArray<AAC_ProjectileLiquid*> blobs;
	GetSlickBlobsOn(Target, blobs);

	float total = 0.0f;
	for (const AAC_ProjectileLiquid* blob : blobs)
	{
		total += blob->SizeMultiplier;
	}
	return total;
}

void AAC_ProjectileLiquid::GetSlickBlobsOn(const AActor* target, TArray<AAC_ProjectileLiquid*>& outBlobs)
{
	outBlobs.Reset();
	if (nullptr == target)
	{
		return;
	}

	TArray<AActor*> attached;
	target->GetAttachedActors(attached);
	for (AActor* actor : attached)
	{
		if (AAC_ProjectileLiquid* blob = Cast<AAC_ProjectileLiquid>(actor))
		{
			outBlobs.Add(blob);
		}
	}
}

void AAC_ProjectileLiquid::ConsumeSlickOn(AActor* Target)
{
	// Destroying blobs and removing their GEs is only meaningful on the server.
	if (nullptr == Target || false == Target->HasAuthority())
	{
		return;
	}

	TArray<AAC_ProjectileLiquid*> blobs;
	GetSlickBlobsOn(Target, blobs);
	for (AAC_ProjectileLiquid* blob : blobs)
	{
		blob->ConsumeForExplosion();
	}
}

bool AAC_ProjectileLiquid::ShouldAbsorb(const AAC_ProjectileLiquid* other) const
{
	if (nullptr == other)
	{
		return false;
	}
	// Larger blob wins; deterministic tie-break so both instances agree on the same survivor.
	if (false == FMath::IsNearlyEqual(SizeMultiplier, other->SizeMultiplier))
	{
		return SizeMultiplier > other->SizeMultiplier;
	}
	return GetUniqueID() > other->GetUniqueID();
}

void AAC_ProjectileLiquid::AbsorbAndGrow(AAC_ProjectileLiquid* other)
{
	if (false == IsValid(other))
	{
		return;
	}

	// Momentum conservation while still moving: size-weighted average of the two velocities.
	if (LiquidState == EACLiquidState::Flying && ProjectileMovement && other->ProjectileMovement)
	{
		const float totalMass = SizeMultiplier + other->SizeMultiplier;
		if (totalMass > KINDA_SMALL_NUMBER)
		{
			const FVector momentum =
				ProjectileMovement->Velocity * SizeMultiplier + other->ProjectileMovement->Velocity * other->SizeMultiplier;
			ProjectileMovement->Velocity = momentum / totalMass;
		}
	}

	SizeMultiplier = FMath::Min(MaxSizeMultiplier, SizeMultiplier + other->SizeMultiplier * AbsorbRatio);
	ApplyScaleFromSize();

	// Transfer (or drop, if redundant) ownership of any GE_Slicked the absorbed blob was holding.
	if (other->ActiveSlickedGEHandle.IsValid())
	{
		if (ActiveSlickedGEHandle.IsValid())
		{
			if (UAbilitySystemComponent* targetAsc =
					UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(other->SlickedTargetActor.Get()))
			{
				targetAsc->RemoveActiveGameplayEffect(other->ActiveSlickedGEHandle);
			}
		}
		else
		{
			ActiveSlickedGEHandle = other->ActiveSlickedGEHandle;
			SlickedTargetActor = other->SlickedTargetActor;
		}
		other->ActiveSlickedGEHandle.Invalidate();
	}

	// Merging changes the total Slick amount without touching the GE, so no tag event fires.
	// Push the update to the target's reaction component directly.
	NotifySlickAmountChanged(SlickedTargetActor.Get());

	other->Destroy();
}

void AAC_ProjectileLiquid::HandleOverlap(UPrimitiveComponent* /*overlappedComp*/, AActor* otherActor,
	UPrimitiveComponent* otherComp, int32 /*otherBodyIndex*/, bool bFromSweep, const FHitResult& sweepResult)
{
	if (false == HasAuthority() || nullptr == otherActor || otherActor == this || otherActor == GetInstigator() || otherActor == GetOwner())
	{
		return;
	}

	// Blob vs blob: the deterministically-chosen survivor absorbs the other.
	if (AAC_ProjectileLiquid* otherLiquid = Cast<AAC_ProjectileLiquid>(otherActor))
	{
		if (ShouldAbsorb(otherLiquid))
		{
			AbsorbAndGrow(otherLiquid);
		}
		return;
	}

	// Blob vs pawn: apply the Slicked debuff once, then stick or destroy. No direct damage.
	if (bHasAppliedToPawn)
	{
		return;
	}

	if (UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(otherActor))
	{
		bHasAppliedToPawn = true;

		ApplySlickedTo(otherActor);

		FHitResult hit = bFromSweep ? sweepResult : FHitResult(otherActor, otherComp, GetActorLocation(), -GetActorForwardVector());
		OnProjectileImpact(hit);

		if (bStickOnHit)
		{
			EnterStuckState(otherComp, GetActorLocation());
		}
		else
		{
			Destroy();
		}
	}
}

void AAC_ProjectileLiquid::HandleBlockingHit(AActor* otherActor, const FHitResult& hit)
{
	// A blocking hit normally comes from world geometry. But an enemy may have a collision
	// primitive that isn't Pawn-typed (a WorldStatic hitbox mesh), so the blob can block-hit a
	// pawn instead of overlapping it. Treat any blocked ASC actor exactly like the overlap path:
	// apply the Slicked debuff, don't just splat on it.
	if (false == bHasAppliedToPawn && otherActor
		&& UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(otherActor))
	{
		bHasAppliedToPawn = true;
		ApplySlickedTo(otherActor);
		OnProjectileImpact(hit);
		if (bStickOnHit)
		{
			EnterStuckState(hit.GetComponent(), hit.ImpactPoint);
		}
		else
		{
			Destroy();
		}
		return;
	}

	// Plain world geometry: splat FX and either stick or destroy. Base impl intentionally not called.
	OnProjectileImpact(hit);

	if (bStickOnHit)
	{
		EnterStuckState(hit.GetComponent(), hit.ImpactPoint);
	}
	else
	{
		Destroy();
	}
}

bool AAC_ProjectileLiquid::ApplySlickedTo(AActor* target)
{
	if (nullptr == SlickedGameplayEffectClass || nullptr == target)
	{
		return false;
	}

	UAbilitySystemComponent* targetAsc = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(target);
	if (nullptr == targetAsc)
	{
		return false;
	}

	// At most one GE_Slicked may be active on a target. Drop any handle this blob already carries
	// (it may have inherited one through AbsorbAndGrow) before applying a new one, otherwise the
	// old effect is orphaned and never removed.
	if (ActiveSlickedGEHandle.IsValid())
	{
		if (UAbilitySystemComponent* previousAsc =
				UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(SlickedTargetActor.Get()))
		{
			previousAsc->RemoveActiveGameplayEffect(ActiveSlickedGEHandle);
		}
		ActiveSlickedGEHandle.Invalidate();
	}

	// Then clear every other blob's stray handle on this target — this blob becomes the sole
	// owner and refreshes the duration.
	TArray<AAC_ProjectileLiquid*> existingBlobs;
	GetSlickBlobsOn(target, existingBlobs);
	for (AAC_ProjectileLiquid* blob : existingBlobs)
	{
		if (blob != this && blob->ActiveSlickedGEHandle.IsValid())
		{
			targetAsc->RemoveActiveGameplayEffect(blob->ActiveSlickedGEHandle);
			blob->ActiveSlickedGEHandle.Invalidate();
		}
	}

	// GE_Slicked is a pure debuff (grants Status.Slicked, no damage) — no source attribution needed.
	// Apply it as the target on itself: a source->target apply from the player's PlayerState ASC
	// was returning a valid handle but not actually granting the tag.
	AActor* damageInstigator = GetInstigator() ? static_cast<AActor*>(GetInstigator()) : GetOwner();

	FGameplayEffectContextHandle context = targetAsc->MakeEffectContext();
	context.AddInstigator(damageInstigator ? damageInstigator : target, this);

	const FGameplayEffectSpecHandle specHandle = targetAsc->MakeOutgoingSpec(SlickedGameplayEffectClass, 1.0f, context);
	if (false == specHandle.IsValid())
	{
		return false;
	}

	ActiveSlickedGEHandle = targetAsc->ApplyGameplayEffectSpecToSelf(*specHandle.Data.Get());
	SlickedTargetActor = target;
	return ActiveSlickedGEHandle.IsValid();
}

void AAC_ProjectileLiquid::ConsumeForExplosion()
{
	// EndPlay releases GE_Slicked for every death path, this one included.
	Destroy();
}

void AAC_ProjectileLiquid::EnterStuckState(USceneComponent* attachTo, const FVector& worldLocation)
{
	if (LiquidState == EACLiquidState::Stuck)
	{
		return;
	}

	LiquidState = EACLiquidState::Stuck;

	// Do NOT touch tick-function registration here (Deactivate / SetComponentTickEnabled /
	// unregister). This runs inside the ProjectileMovementComponent's own tick via a collision
	// callback; changing tick registration mid-tick corrupts the tick task list and crashes with
	// "Pure virtual not implemented" (EngineBaseTypes.h). Instead just neutralise the movement —
	// a still-ticking PMC with zero velocity and zero gravity computes a zero delta and does nothing.
	if (ProjectileMovement)
	{
		ProjectileMovement->StopMovementImmediately();
		ProjectileMovement->Velocity = FVector::ZeroVector;
		ProjectileMovement->ProjectileGravityScale = 0.0f;
	}
	SetActorLocation(worldLocation);
	if (attachTo)
	{
		AttachToComponent(attachTo, FAttachmentTransformRules::KeepWorldTransform);
	}

	// Keep collision on so still-flying blobs can merge into this one.

	if (StuckLifeSeconds > 0.0f)
	{
		SetLifeSpan(StuckLifeSeconds);
	}

	OnEnterStuckState();
}

void AAC_ProjectileLiquid::ApplyScaleFromSize()
{
	SetActorScale3D(FVector(SizeMultiplier));
	OnSizeChanged(SizeMultiplier);
}

void AAC_ProjectileLiquid::OnRep_LiquidState()
{
	if (LiquidState == EACLiquidState::Stuck)
	{
		if (ProjectileMovement)
		{
			ProjectileMovement->StopMovementImmediately();
			ProjectileMovement->Velocity = FVector::ZeroVector;
			ProjectileMovement->ProjectileGravityScale = 0.0f;
		}
		OnEnterStuckState();
	}
}

void AAC_ProjectileLiquid::OnRep_SizeMultiplier()
{
	ApplyScaleFromSize();
}
