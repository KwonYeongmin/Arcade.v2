// Copyright Epic Games, Inc. All Rights Reserved.

#include "AC_Projectile.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Components/SphereComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "GameplayEffect.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AC_Projectile)

AAC_Projectile::AAC_Projectile(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = false;

	bReplicates = true;
	SetReplicateMovement(true);

	CollisionComponent = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionComponent"));
	CollisionComponent->InitSphereRadius(CollisionRadius);
	CollisionComponent->SetCollisionProfileName(TEXT("BlockAllDynamic"));
	CollisionComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	CollisionComponent->SetNotifyRigidBodyCollision(true);
	CollisionComponent->CanCharacterStepUpOn = ECB_No;
	SetRootComponent(CollisionComponent);

	ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovement"));
	ProjectileMovement->SetUpdatedComponent(CollisionComponent);
	ProjectileMovement->InitialSpeed = InitialSpeed;
	ProjectileMovement->MaxSpeed = InitialSpeed;
	ProjectileMovement->bRotationFollowsVelocity = true;
	ProjectileMovement->bShouldBounce = false;
	ProjectileMovement->ProjectileGravityScale = GravityScale;
}

void AAC_Projectile::BeginPlay()
{
	Super::BeginPlay();

	// 에디터에서 바뀐 디폴트값을 런타임 컴포넌트에 반영한다.
	CollisionComponent->SetSphereRadius(CollisionRadius);
	ProjectileMovement->InitialSpeed = InitialSpeed;
	ProjectileMovement->MaxSpeed = InitialSpeed;
	ProjectileMovement->ProjectileGravityScale = GravityScale;

	// 쏜 폰 / 무기와는 충돌하지 않는다.
	if (AActor* MyInstigator = GetInstigator())
	{
		CollisionComponent->IgnoreActorWhenMoving(MyInstigator, true);
	}
	if (AActor* MyOwner = GetOwner())
	{
		CollisionComponent->IgnoreActorWhenMoving(MyOwner, true);
	}

	CollisionComponent->OnComponentHit.AddDynamic(this, &AAC_Projectile::HandleHit);

	if (MaxLifeSeconds > 0.0f)
	{
		SetLifeSpan(MaxLifeSeconds);
	}
}

void AAC_Projectile::LaunchInDirection(const FVector& Direction)
{
	const FVector LaunchDir = Direction.GetSafeNormal();
	if (LaunchDir.IsNearlyZero())
	{
		return;
	}

	// ProjectileMovement 는 Velocity 를 직접 읽어 비행한다.
	ProjectileMovement->Velocity = LaunchDir * InitialSpeed;
	SetActorRotation(LaunchDir.Rotation());
}

void AAC_Projectile::HandleHit(UPrimitiveComponent* /*HitComp*/, AActor* OtherActor, UPrimitiveComponent* /*OtherComp*/,
	FVector /*NormalImpulse*/, const FHitResult& Hit)
{
	// Never react to the shooter, its weapon, or anything attached to either — the projectile
	// spawns overlapping the muzzle. Do NOT consume bHasHit here so a later real hit still lands.
	if (OtherActor
		&& (OtherActor == GetInstigator()
			|| OtherActor == GetOwner()
			|| (GetOwner() && OtherActor->IsAttachedTo(GetOwner()))
			|| (GetInstigator() && OtherActor->IsAttachedTo(GetInstigator()))))
	{
		return;
	}

	if (bHasHit)
	{
		return;
	}
	bHasHit = true;

	HandleBlockingHit(OtherActor, Hit);
}

void AAC_Projectile::HandleBlockingHit(AActor* OtherActor, const FHitResult& Hit)
{
	// 데미지는 서버 권한에서만. 자기 자신 / 쏜 폰 은 무시.
	if (HasAuthority() && OtherActor && OtherActor != this && OtherActor != GetInstigator() && OtherActor != GetOwner())
	{
		ApplyDamage(OtherActor, Hit);
	}

	// 코스메틱 훅은 모든 인스턴스에서.
	OnProjectileImpact(Hit);

	if (bDestroyOnHit)
	{
		Destroy();
	}
}

float AAC_Projectile::GetDamageAmount() const
{
	return Damage;
}

void AAC_Projectile::ApplyDamage(AActor* Target, const FHitResult& Hit)
{
	UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Target);
	if (!TargetASC || !DamageGameplayEffectClass)
	{
		return;
	}

	const float DamageAmount = GetDamageAmount();

	AActor* DamageInstigator = GetInstigator() ? static_cast<AActor*>(GetInstigator()) : GetOwner();
	UAbilitySystemComponent* SourceASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(DamageInstigator);
	UAbilitySystemComponent* EffectCauserASC = SourceASC ? SourceASC : TargetASC;

	FGameplayEffectContextHandle Context = EffectCauserASC->MakeEffectContext();
	Context.AddInstigator(DamageInstigator, this);
	Context.AddHitResult(Hit);

	const FGameplayEffectSpecHandle SpecHandle =
		EffectCauserASC->MakeOutgoingSpec(DamageGameplayEffectClass, DamageAmount, Context);
	if (!SpecHandle.IsValid())
	{
		return;
	}

	if (DamageSetByCallerTag.IsValid())
	{
		SpecHandle.Data->SetSetByCallerMagnitude(DamageSetByCallerTag, DamageAmount);
	}

	EffectCauserASC->ApplyGameplayEffectSpecToTarget(*SpecHandle.Data.Get(), TargetASC);
}
