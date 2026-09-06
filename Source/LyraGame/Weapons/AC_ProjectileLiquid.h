// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AC_Projectile.h"
#include "GameplayEffectTypes.h"

#include "AC_ProjectileLiquid.generated.h"

#define UE_API LYRAGAME_API

class UAbilitySystemComponent;

/**
 * Object channel for liquid blobs — registered in Config/DefaultEngine.ini as
 * "Arcade_ObjectChannel_LiquidBlob". GameTraceChannel1-5 are Lyra's; 6 is the first free slot.
 */
inline constexpr ECollisionChannel LiquidBlobObjectChannel = ECollisionChannel::ECC_GameTraceChannel6;

UENUM(BlueprintType)
enum class EACLiquidState : uint8
{
	// In flight.
	Flying,
	// Landed on a surface or an actor and stopped. Still absorbs incoming liquid blobs.
	Stuck
};

/**
 * AAC_ProjectileLiquid
 *
 * A liquid blob projectile. On top of AAC_Projectile it adds:
 *  - Merging: when two liquid blobs overlap, the deterministically-chosen larger one absorbs
 *    the other — its size grows by AbsorbRatio of the absorbed size (capped at MaxSizeMultiplier),
 *    velocity becomes the size-weighted average (momentum conservation), the smaller is destroyed.
 *  - Slicked debuff: on hitting a pawn with an ASC, applies SlickedGameplayEffectClass (a Duration
 *    GE granting Status.Slicked) instead of dealing damage. At most one GE_Slicked instance is ever
 *    active per target — later blobs sticking to the same target reuse/refresh that one GE. The
 *    "amount" of Slick on a target is read from GetTotalSlickAmount(), the sum of SizeMultiplier
 *    across every blob attached to it, not from the GE.
 *  - Sticking: on hitting the world or a pawn it can stay put for StuckLifeSeconds instead of
 *    destroying immediately, so later blobs can still merge into it.
 *
 * Collision: the CollisionComponent uses a dedicated object channel (see
 * Config/DefaultEngine.ini "Arcade_ObjectChannel_LiquidBlob"). Blobs BLOCK world geometry
 * (WorldStatic + WorldDynamic) so they stick to any wall/floor/prop, OVERLAP pawns so
 * HandleOverlap can apply the Slicked debuff, and OVERLAP each other so they merge. Weapon
 * traces ignore blobs, so Flint hitscan passes through a blob and hits the coated pawn behind it.
 *
 * Visuals (mesh / Material / Niagara / stuck-state material swap) live in a B_Projectile_* child;
 * use OnEnterStuckState / OnSizeChanged / OnProjectileImpact as hooks.
 */
UCLASS(MinimalAPI, Blueprintable)
class AAC_ProjectileLiquid : public AAC_Projectile
{
	GENERATED_BODY()

public:
	UE_API AAC_ProjectileLiquid(const FObjectInitializer& objectInitializer = FObjectInitializer::Get());

	UFUNCTION(BlueprintPure, Category = "Projectile|Liquid")
	EACLiquidState GetLiquidState() const { return LiquidState; }

	UFUNCTION(BlueprintPure, Category = "Projectile|Liquid")
	float GetSizeMultiplier() const { return SizeMultiplier; }

	/** Target 에 attach 된 모든 AAC_ProjectileLiquid 의 SizeMultiplier 합. 클라에서도 대략 정확(SizeMultiplier replicate). */
	UFUNCTION(BlueprintPure, Category = "Projectile|Liquid")
	static UE_API float GetTotalSlickAmount(const AActor* Target);

	/** Target 에 attach 된 모든 AAC_ProjectileLiquid 를 모은다. */
	static UE_API void GetSlickBlobsOn(const AActor* target, TArray<AAC_ProjectileLiquid*>& outBlobs);

	/** Target 에 붙은 블롭들을 전부 소모한다: 각자 GE_Slicked 제거(중복 없이 한 번만) + Destroy. 서버에서만 유효. */
	UFUNCTION(BlueprintCallable, Category = "Projectile|Liquid")
	static UE_API void ConsumeSlickOn(AActor* Target);

	/** Target 에 붙은 Slick 총량이 바뀌었음을 Target 의 UAC_SlickReactionComponent 에 알린다(있다면). */
	static UE_API void NotifySlickAmountChanged(AActor* target);

	/** 상태가 Stuck 으로 바뀔 때(로컬/OnRep 모두). 머티리얼 교체 등 코스메틱. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Projectile|Liquid")
	void OnEnterStuckState();

	/** SizeMultiplier 가 바뀔 때(병합/OnRep). 스케일은 C++ 가 이미 적용했다. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Projectile|Liquid")
	void OnSizeChanged(float NewSizeMultiplier);

protected:
	/** 흡수 시 더할 상대 크기 비율. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Liquid", meta = (ClampMin = "0.0"))
	float AbsorbRatio = 0.85f;

	/** SizeMultiplier 상한. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Liquid", meta = (ClampMin = "1.0"))
	float MaxSizeMultiplier = 4.0f;

	/** true 면 월드/폰에 맞아도 즉시 소멸하지 않고 StuckLifeSeconds 동안 붙어 있는다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Liquid")
	bool bStickOnHit = true;

	/** Stuck 상태 유지 시간(초). 0 이면 무한(부모 MaxLifeSeconds 만 적용). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Liquid", meta = (ClampMin = "0.0"))
	float StuckLifeSeconds = 2.0f;

	/** 폰에 붙을 때 대상 ASC 에 적용할 디버프 GE(Duration, Status.Slicked 태그 부여, No Stack). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Liquid")
	TSubclassOf<class UGameplayEffect> SlickedGameplayEffectClass;

	/** 스폰 시 SizeMultiplier 를 이 범위에서 랜덤 선택한다. X==Y 면(기본값) 항상 1.0 고정(기존과 동일). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Liquid")
	FVector2D InitialSizeMultiplierRange = FVector2D(1.0f, 1.0f);

	UPROPERTY(ReplicatedUsing = OnRep_LiquidState, BlueprintReadOnly, Category = "Projectile|Liquid")
	EACLiquidState LiquidState = EACLiquidState::Flying;

	UPROPERTY(ReplicatedUsing = OnRep_SizeMultiplier, BlueprintReadOnly, Category = "Projectile|Liquid")
	float SizeMultiplier = 1.0f;

	UE_API virtual void BeginPlay() override;
	UE_API virtual void EndPlay(const EEndPlayReason::Type endPlayReason) override;
	UE_API virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	//~AAC_Projectile
	UE_API virtual void HandleBlockingHit(AActor* otherActor, const FHitResult& hit) override;
	//~End

private:
	/** CollisionComponent 을 LiquidBlob 채널 규칙으로 설정한다. ctor 와 BeginPlay 양쪽에서 호출 —
	 *  BeginPlay 에서 다시 부르는 이유는 B_Projectile_* BP 의 SCS 컴포넌트가 갖고 있을 수 있는
	 *  콜리전 오버라이드를 런타임에 무력화하기 위해서다. */
	void ConfigureCollision() const;

	UFUNCTION()
	void HandleOverlap(UPrimitiveComponent* overlappedComp, AActor* otherActor, UPrimitiveComponent* otherComp,
		int32 otherBodyIndex, bool bFromSweep, const FHitResult& sweepResult);

	UFUNCTION()
	void OnRep_LiquidState();

	UFUNCTION()
	void OnRep_SizeMultiplier();

	/** 이 블롭이 Other 를 흡수한다(서버). 크기 증가 + 운동량 평균 + GE_Slicked 소유권 이전 + Other 파괴. */
	void AbsorbAndGrow(AAC_ProjectileLiquid* other);

	/** SizeMultiplier 로 액터 스케일 갱신 + OnSizeChanged 호출. */
	void ApplyScaleFromSize();

	/** 이동을 멈추고 Stuck 상태로 전환한다. */
	void EnterStuckState(USceneComponent* attachTo, const FVector& worldLocation);

	/** 두 블롭 중 이 블롭이 흡수 주체인지 결정적으로 판정한다. */
	bool ShouldAbsorb(const AAC_ProjectileLiquid* other) const;

	/** Target 의 ASC 에 SlickedGameplayEffectClass 를 적용한다. 이미 Target 에 붙은 다른 블롭이 활성
	 *  핸들을 갖고 있으면 그것을 제거하고 이 블롭이 새로 적용해 소유권 + duration 을 갱신한다. */
	bool ApplySlickedTo(AActor* target);

	/** 이 블롭을 소모한다. GE_Slicked 해제는 EndPlay 가 처리한다. */
	void ConsumeForExplosion();

	bool bHasAppliedToPawn = false;

	/** 이 블롭이 현재 소유한 GE_Slicked 핸들(있다면). ApplySlickedTo / AbsorbAndGrow 가 소유권을 옮긴다. */
	FActiveGameplayEffectHandle ActiveSlickedGEHandle;

	/** ActiveSlickedGEHandle 이 적용된 대상. 핸들 제거 시 어느 ASC 에서 지울지 알아야 하므로 캐시. */
	TWeakObjectPtr<AActor> SlickedTargetActor;
};

#undef UE_API
