// Copyright Epic Games, Inc. All Rights Reserved.

#include "AC_SlickExplosion.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Arcade/Data/AC_ArcadeDataRows.h"
#include "Arcade/Data/AC_ArcadeDataSubsystem.h"
#include "Arcade/Data/AC_ArcadeDataTags.h"
#include "Camera/CameraShakeBase.h"
#include "Components/SphereComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameplayCueFunctionLibrary.h"
#include "GameplayEffect.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AC_SlickExplosion)

AAC_SlickExplosion::AAC_SlickExplosion(const FObjectInitializer& objectInitializer)
	: Super(objectInitializer)
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	ExplosionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("ExplosionSphere"));
	ExplosionSphere->InitSphereRadius(ExplosionRadius);
	ExplosionSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SetRootComponent(ExplosionSphere);
}

void AAC_SlickExplosion::ApplyDamageTuningFromData()
{
	if (CombatRowName.IsNone())
	{
		return;
	}

	const UAC_ArcadeDataSubsystem* data = UAC_ArcadeDataSubsystem::Get(this);
	if (nullptr == data)
	{
		return;
	}

	const FAC_DamageConfigRow* row = data->FindRow<FAC_DamageConfigRow>(ArcadeDataTags::Table_Combat, CombatRowName);
	if (nullptr == row)
	{
		return;
	}

	ExplosionBaseDamage = row->BaseDamage;
	PerSlickDamage = row->PerSlickDamage;
	MaxSlickAmountForDamage = row->MaxSlickAmount;
	bFalloff = row->bFalloff;
	FalloffFloor = row->FalloffFloor;
	if (row->Radius > 0.0f)
	{
		ExplosionRadius = row->Radius;
	}
}

void AAC_SlickExplosion::Detonate(float SlickAmount, AActor* InstigatorPawn)
{
	if (false == HasAuthority())
	{
		return;
	}

	ApplyDamageTuningFromData();

	// ULyraDamageExecution resolves this actor's team through its Instigator. Without one,
	// CanCauseDamage filters every target out and the explosion silently deals zero damage.
	if (APawn* instigatorAsPawn = Cast<APawn>(InstigatorPawn))
	{
		if (GetInstigator() == nullptr)
		{
			SetInstigator(instigatorAsPawn);
		}
	}
	ensureMsgf(GetInstigator() != nullptr,
		TEXT("AAC_SlickExplosion::Detonate with no Instigator — damage will be filtered to zero"));

	if (DamageGameplayEffectClass)
	{
		UAbilitySystemComponent* sourceAsc = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(InstigatorPawn);

		// ExplosionSphere never has collision enabled (it only exists to hold ExplosionRadius /
		// debug draw), so do a manual sphere overlap query instead of relying on generated overlaps.
		TArray<FOverlapResult> overlaps;
		FCollisionShape sphere = FCollisionShape::MakeSphere(ExplosionRadius);
		GetWorld()->OverlapMultiByObjectType(overlaps, GetActorLocation(), FQuat::Identity,
			FCollisionObjectQueryParams(ECC_TO_BITFIELD(ECC_Pawn) | ECC_TO_BITFIELD(ECC_WorldDynamic)), sphere);

		const float clampedSlickAmount = FMath::Min(SlickAmount, MaxSlickAmountForDamage);
		const float fullDamage = ExplosionBaseDamage + PerSlickDamage * clampedSlickAmount;

		TSet<AActor*> damaged;
		for (const FOverlapResult& overlap : overlaps)
		{
			AActor* target = overlap.GetActor();
			if (nullptr == target || target == this || target == InstigatorPawn || target == GetInstigator() || damaged.Contains(target))
			{
				continue;
			}

			UAbilitySystemComponent* targetAsc = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(target);
			if (nullptr == targetAsc)
			{
				continue;
			}
			// Pure centre-to-centre distance. OverlapMultiByObjectType can return actors slightly
			// past the query radius, so enforce the radius explicitly here. Using the actor origin
			// (not a bounds-adjusted distance) keeps a bystander one blast-radius away out of it.
			const float distance = (target->GetActorLocation() - GetActorLocation()).Size();
			if (distance > ExplosionRadius)
			{
				continue;
			}
			damaged.Add(target);

			float finalDamage = fullDamage;
			if (bFalloff)
			{
				// Gentle falloff with a floor, so anything genuinely inside the radius still hits hard.
				const float falloffScale = FMath::Lerp(FalloffFloor, 1.0f, FMath::Clamp(1.0f - distance / FMath::Max(ExplosionRadius, 1.0f), 0.0f, 1.0f));
				finalDamage *= falloffScale;
			}

			UAbilitySystemComponent* effectCauserAsc = sourceAsc ? sourceAsc : targetAsc;
			FGameplayEffectContextHandle context = effectCauserAsc->MakeEffectContext();
			context.AddInstigator(InstigatorPawn, this);

			FHitResult syntheticHit(target, Cast<UPrimitiveComponent>(target->GetRootComponent()), target->GetActorLocation(), FVector::UpVector);
			context.AddHitResult(syntheticHit);

			const FGameplayEffectSpecHandle specHandle =
				effectCauserAsc->MakeOutgoingSpec(DamageGameplayEffectClass, finalDamage, context);
			if (false == specHandle.IsValid())
			{
				continue;
			}

			if (DamageSetByCallerTag.IsValid())
			{
				specHandle.Data->SetSetByCallerMagnitude(DamageSetByCallerTag, finalDamage);
			}

			effectCauserAsc->ApplyGameplayEffectSpecToTarget(*specHandle.Data.Get(), targetAsc);
		}
	}

	if (ExplosionGameplayCueTag.IsValid() && nullptr == InstigatorPawn)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[Slick] AAC_SlickExplosion::Detonate has no InstigatorPawn — explosion GameplayCue will not play."));
	}
	else if (ExplosionGameplayCueTag.IsValid())
	{
		FGameplayCueParameters cueParams;
		cueParams.Location = GetActorLocation();
		cueParams.Instigator = InstigatorPawn;
		cueParams.EffectCauser = this;
		// This actor has no ASC, so executing the cue on it would only ever play locally on the
		// server. Route it through the instigator pawn's ASC instead — CueParams.Location still
		// carries the impact point, so the FX plays where the explosion happened.
		UGameplayCueFunctionLibrary::ExecuteGameplayCueOnActor(InstigatorPawn, ExplosionGameplayCueTag, cueParams);
	}

	Multicast_OnExploded();

	// Long enough for the multicast to reach remote clients before the actor is torn down.
	SetLifeSpan(0.5f);
}

void AAC_SlickExplosion::Multicast_OnExploded_Implementation()
{
	PlayExplosionFeel();
}

void AAC_SlickExplosion::PlayExplosionFeel()
{
	float fxScale = 1.0f;

	// Runs on every viewer; the data subsystem's registry is loaded client-side too, and
	// CombatRowName is the same on every instance (BP class default), so re-read the row here.
	if (false == CombatRowName.IsNone())
	{
		if (const UAC_ArcadeDataSubsystem* data = UAC_ArcadeDataSubsystem::Get(this))
		{
			if (const FAC_DamageConfigRow* row = data->FindRow<FAC_DamageConfigRow>(ArcadeDataTags::Table_Combat, CombatRowName))
			{
				fxScale = row->FXScale;

				if (UClass* shakeClass = row->CameraShake.LoadSynchronous())
				{
					UGameplayStatics::PlayWorldCameraShake(this, shakeClass, GetActorLocation(),
						row->CameraShakeInnerRadius, row->CameraShakeOuterRadius, /*Falloff=*/1.0f);
				}

				if (row->HitStopSeconds > 0.0f && row->HitStopTimeScale < 1.0f)
				{
					if (UWorld* world = GetWorld())
					{
						UGameplayStatics::SetGlobalTimeDilation(world, row->HitStopTimeScale);
						bHitStopActive = true;
						// TimerManager ticks on dilated time, so scale the wait to land at HitStopSeconds real.
						const float dilatedWait = row->HitStopSeconds / FMath::Max(row->HitStopTimeScale, 0.01f);
						world->GetTimerManager().SetTimer(HitStopTimer, this, &AAC_SlickExplosion::EndHitStop, dilatedWait, false);
					}
				}
			}
		}
	}

	OnExploded(fxScale);
}

void AAC_SlickExplosion::EndHitStop()
{
	if (bHitStopActive)
	{
		bHitStopActive = false;
		if (UWorld* world = GetWorld())
		{
			UGameplayStatics::SetGlobalTimeDilation(world, 1.0f);
		}
	}
}

void AAC_SlickExplosion::EndPlay(const EEndPlayReason::Type endPlayReason)
{
	// Never leave time dilation stuck if the actor is torn down mid hit-stop.
	if (UWorld* world = GetWorld())
	{
		world->GetTimerManager().ClearTimer(HitStopTimer);
	}
	EndHitStop();
	Super::EndPlay(endPlayReason);
}
