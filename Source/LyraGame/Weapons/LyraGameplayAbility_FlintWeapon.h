// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameplayTagContainer.h"
#include "LyraGameplayAbility_RangedWeapon.h"

#include "LyraGameplayAbility_FlintWeapon.generated.h"

class AAC_SlickExplosion;
class APawn;
class UGameplayEffect;

/**
 * ULyraGameplayAbility_FlintWeapon
 *
 * Hitscan ranged weapon ability. Every resolved bullet applies a small direct-damage GE. If the
 * hit actor is carrying Slick (AAC_ProjectileLiquid::GetTotalSlickAmount > 0), it also spawns an
 * AAC_SlickExplosion at the impact point and consumes all of that target's Slick.
 */
UCLASS()
class ULyraGameplayAbility_FlintWeapon : public ULyraGameplayAbility_RangedWeapon
{
	GENERATED_BODY()

public:
	/** SourcePawn 이 지금 조준 중인 대상이 Slick 묻은 대상인지(UI 리티클 훅). 서버/클라 모두에서 호출 가능한
	 *  순수 라인트레이스 판정. 위젯은 어빌리티 인스턴스가 없으므로 static 으로 둔다. */
	UFUNCTION(BlueprintPure, Category = "Flint Weapon")
	static bool IsAimingAtSlickedTarget(APawn* SourcePawn, float Range);

	/** 어빌리티 컨텍스트용 래퍼. 아바타 폰과 DetectionRange 로 위의 static 을 호출한다. */
	UFUNCTION(BlueprintPure, Category = "Flint Weapon")
	bool IsAimingAtSlickedTargetNow() const;

protected:
	/** 매 명중 시 적용할 소량의 직접 데미지 GE. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Flint Weapon")
	TSubclassOf<UGameplayEffect> DirectDamageGameplayEffectClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Flint Weapon")
	FGameplayTag DirectDamageSetByCallerTag;

	/** DirectDamage 의 폴백 값. 아래 CombatRowName 행이 있으면 그 BaseDamage 로 대체된다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Flint Weapon", meta = (ClampMin = "0.0"))
	float DirectDamage = 5.0f;

	/**
	 * DT_Combat (ArcadeData.Table.Combat) 에서 직격 데미지를 읽어올 행 이름. 행의 BaseDamage 를 사용한다.
	 * 서브시스템이 없거나 행이 없으면 위 DirectDamage 를 그대로 쓴다. NAME_None 이면 조회하지 않는다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Flint Weapon")
	FName CombatRowName = TEXT("Flint.Direct");

	/** Slick 묻은 대상에 명중했을 때 스폰할 폭발. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Flint Weapon")
	TSubclassOf<AAC_SlickExplosion> ExplosionClass;

	/** IsAimingAtSlickedTarget 판정용 라인트레이스 최대 거리. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Flint Weapon", meta = (ClampMin = "0.0"))
	float DetectionRange = 5000.0f;

	//~ULyraGameplayAbility_RangedWeapon
	virtual void OnRangedWeaponTargetDataReady_Implementation(const FGameplayAbilityTargetDataHandle& targetData) override;
	//~End

private:
	/** CombatRowName 으로 DT_Combat 을 조회한 직격 데미지. 조회 실패 시 DirectDamage 반환. */
	float ResolveDirectDamage() const;

	void ApplyDirectDamage(AActor* hitActor, const FHitResult& hit);
	void TrySpawnExplosion(AActor* hitActor, const FHitResult& hit);
};
