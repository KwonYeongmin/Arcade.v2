// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"

#include "AC_Projectile.generated.h"

#define UE_API LYRAGAME_API

class USphereComponent;
class UProjectileMovementComponent;
class UGameplayEffect;

/**
 * AAC_Projectile
 *
 * 눈에 보이게 날아가는 발사체. 게임플레이 부분만 담는다:
 *  - USphereComponent 충돌 + UProjectileMovementComponent 비행 (직선 또는 포물선)
 *  - 첫 충돌에서 대상 ASC 에 데미지 GameplayEffect 적용 (서버 권한에서만)
 *  - OnProjectileImpact BP 훅으로 임팩트 이펙트/데칼/GameplayCue 를 붙일 자리 제공
 *
 * 비주얼(Material / Niagara / 메시)은 이 클래스가 만들지 않는다. B_Projectile_* BP 자식에서
 * CollisionComponent 아래에 원하는 컴포넌트를 붙인다.
 *
 * 발사: 발사 코드가 무기 머즐 트랜스폼으로 SpawnActor 한 뒤 LaunchInDirection() 을 호출한다.
 * Instigator 를 쏜 폰으로, Owner 를 무기/폰으로 세팅해야 데미지가 팀에 올바르게 귀속된다.
 */
UCLASS(MinimalAPI, Blueprintable)
class AAC_Projectile : public AActor
{
	GENERATED_BODY()

public:
	UE_API AAC_Projectile(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/**
	 * 발사 방향으로 초기 속도를 건다. 스폰 직후 발사 코드가 호출한다.
	 * Direction 은 정규화되지 않아도 된다(내부에서 정규화). InitialSpeed / GravityScale 은 디폴트값을 쓴다.
	 */
	UFUNCTION(BlueprintCallable, Category = "Projectile")
	UE_API void LaunchInDirection(const FVector& Direction);

	/**
	 * 충돌 순간 호출되는 BP 훅. 데미지는 C++ 가 이미(서버에서) 적용했으므로, 여기서는 임팩트
	 * 파티클 / 데칼 / GameplayCue / 사운드 등 코스메틱만 처리한다. 모든 인스턴스에서 호출된다.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Projectile")
	void OnProjectileImpact(const FHitResult& Hit);

protected:
	/** 루트이자 스윕 충돌. B_Projectile_* BP 에서 이 아래에 비주얼을 붙인다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Projectile")
	TObjectPtr<USphereComponent> CollisionComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Projectile")
	TObjectPtr<UProjectileMovementComponent> ProjectileMovement;

	/**
	 * 명중 시 대상 ASC 에 적용할 데미지 GameplayEffect. 비어 있으면 데미지 없이 OnProjectileImpact
	 * 만 호출한다. Lyra 데미지 GE 처럼 SetByCaller 를 쓰는 GE 면 DamageSetByCallerTag 를 지정한다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile")
	TSubclassOf<UGameplayEffect> DamageGameplayEffectClass;

	/**
	 * DamageGameplayEffectClass 에 SetByCaller 로 넘길 데미지 태그. 유효하면 그 태그에 Damage 값을
	 * 실어 보낸다. 비워두면 GE 레벨(= Damage)만 전달한다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile")
	FGameplayTag DamageSetByCallerTag;

	/** 데미지 크기. SetByCaller 값 및 GE 적용 레벨로 쓴다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile", meta = (ClampMin = "0.0"))
	float Damage = 20.0f;

	/** 초기 속도(cm/s). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile", meta = (ClampMin = "0.0"))
	float InitialSpeed = 6000.0f;

	/** 0 = 직선, > 0 = 포물선. ProjectileMovement 의 ProjectileGravityScale. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile", meta = (ClampMin = "0.0"))
	float GravityScale = 0.0f;

	/** 이 시간(초) 뒤 자동 소멸. 0 이면 소멸 안 함. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile", meta = (ClampMin = "0.0"))
	float MaxLifeSeconds = 5.0f;

	/** true 면 첫 충돌에서 소멸한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile")
	bool bDestroyOnHit = true;

	/** 충돌 반경(cm). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile", meta = (ClampMin = "0.0"))
	float CollisionRadius = 8.0f;

	UE_API virtual void BeginPlay() override;

	/**
	 * 블로킹 충돌 처리 진입점. 서브클래스가 재정의한다.
	 * 기본 구현: OtherActor 에 데미지 적용 → OnProjectileImpact → bDestroyOnHit 이면 소멸.
	 * OtherActor 는 null 일 수 있다(월드 지오메트리).
	 */
	UE_API virtual void HandleBlockingHit(AActor* OtherActor, const FHitResult& Hit);

	/** 대상 ASC 에 DamageGameplayEffectClass 를 적용한다. 크기·상태에 따라 데미지를 바꾸려면 GetDamageAmount 를 재정의한다. */
	UE_API void ApplyDamage(AActor* Target, const FHitResult& Hit);

	/** 실제 적용할 데미지 크기. 기본은 Damage 프로퍼티. 유체 병합처럼 크기에 비례시키려면 재정의한다. */
	UE_API virtual float GetDamageAmount() const;

	/** 재진입/다중 충돌 방지. */
	bool bHasHit = false;

private:
	/** ProjectileMovement 스윕이 부르는 델리게이트. bHasHit 가드 후 HandleBlockingHit 로 넘긴다. */
	UFUNCTION()
	void HandleHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
		FVector NormalImpulse, const FHitResult& Hit);
};

#undef UE_API
