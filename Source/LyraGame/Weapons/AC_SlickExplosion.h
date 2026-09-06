// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"

#include "AC_SlickExplosion.generated.h"

#define UE_API LYRAGAME_API

class USphereComponent;
class UGameplayEffect;

/**
 * AAC_SlickExplosion
 *
 * Radial damage burst spawned by ULyraGameplayAbility_FlintWeapon when a Flint hit lands on a
 * target carrying Slick. Detonate() does one authority-side overlap sweep, applies
 * DamageGameplayEffectClass to every damageable actor in range (scaled by the Slick amount that
 * was on the target, with optional linear falloff), fires a cosmetic GameplayCue, then self-destroys.
 *
 * Friendly-fire / self-damage filtering is NOT done here — it is handled the same way
 * AAC_Projectile::ApplyDamage relies on it: DamageGameplayEffectClass (GE_Damage_Basic_SetByCaller)
 * runs ULyraDamageExecution, which resolves this actor's team through its Instigator via
 * ULyraTeamSubsystem::CanCauseDamage. Callers MUST set SpawnParams.Instigator to the attacking pawn.
 */
UCLASS(MinimalAPI, Blueprintable)
class AAC_SlickExplosion : public AActor
{
	GENERATED_BODY()

public:
	UE_API AAC_SlickExplosion(const FObjectInitializer& objectInitializer = FObjectInitializer::Get());

	/** 폭발을 실행한다. SlickAmount 는 소모되기 전 대상에 묻어 있던 총 크기(AAC_ProjectileLiquid::GetTotalSlickAmount). */
	UFUNCTION(BlueprintCallable, Category = "Projectile|Explosion")
	UE_API void Detonate(float SlickAmount, AActor* InstigatorPawn);

	/**
	 * 코스메틱 훅(Niagara/사운드). Detonate 가 데미지 적용 후 모든 뷰어에서 호출한다.
	 * fxScale 은 DT_Combat 의 FXScale — Niagara 버스트 크기에 곱하면 된다.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Projectile|Explosion")
	void OnExploded(float fxScale);

protected:
	/** 서버가 Detonate 에서 호출. 클라에서도 OnExploded 코스메틱이 재생되도록 브로드캐스트한다. */
	UFUNCTION(NetMulticast, Unreliable)
	UE_API void Multicast_OnExploded();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Projectile|Explosion")
	TObjectPtr<USphereComponent> ExplosionSphere;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Explosion", meta = (ClampMin = "0.0"))
	float ExplosionRadius = 300.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Explosion", meta = (ClampMin = "0.0"))
	float ExplosionBaseDamage = 15.0f;

	/** 최종 데미지 = ExplosionBaseDamage + PerSlickDamage * min(SlickAmount, MaxSlickAmountForDamage). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Explosion", meta = (ClampMin = "0.0"))
	float PerSlickDamage = 20.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Explosion", meta = (ClampMin = "0.0"))
	float MaxSlickAmountForDamage = 6.0f;

	/**
	 * Row of DT_Combat (ArcadeData.Table.Combat) to pull the damage numbers from at spawn time.
	 * When the row resolves it overwrites ExplosionBaseDamage / PerSlickDamage /
	 * MaxSlickAmountForDamage / ExplosionRadius / bFalloff / FalloffFloor. Left as-is (the values
	 * above) if UAC_ArcadeDataSubsystem is not ready or the row is missing. NAME_None disables the lookup.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Explosion")
	FName CombatRowName = TEXT("Slick.Explosion");

	/** 폴오프 시 반경 가장자리에서의 데미지 배율(0~1). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Explosion", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float FalloffFloor = 0.4f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Explosion")
	TSubclassOf<UGameplayEffect> DamageGameplayEffectClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Explosion")
	FGameplayTag DamageSetByCallerTag;

	/** true 면 중심에서 멀수록(선형) 데미지를 줄인다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Explosion")
	bool bFalloff = true;

	/** 코스메틱 GameplayCue(예: GCN_Slick_Explosion 이 반응하는 태그). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Explosion")
	FGameplayTag ExplosionGameplayCueTag;

	//~AActor
	virtual void EndPlay(const EEndPlayReason::Type endPlayReason) override;
	//~End

private:
	/** CombatRowName 으로 DT_Combat 을 조회해 데미지/반경 수치를 덮어쓴다. Detonate 시작 시 1회 호출. */
	void ApplyDamageTuningFromData();

	/** 모든 뷰어에서: 카메라 셰이크 + 히트스톱 + OnExploded(fxScale). DT_Combat 의 Feel 필드를 읽는다. */
	void PlayExplosionFeel();

	/** 히트스톱 종료 — 글로벌 타임 딜레이션을 1로 되돌린다. */
	void EndHitStop();

	bool bHitStopActive = false;
	FTimerHandle HitStopTimer;
};

#undef UE_API
