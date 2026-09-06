// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"

#include "AC_SlickReactionComponent.generated.h"

#define UE_API LYRAGAME_API

class UAbilitySystemComponent;
class UCharacterMovementComponent;

/**
 * UAC_SlickReactionComponent
 *
 * Attach to an enemy pawn (Character with a UCharacterMovementComponent and an ASC). Watches
 * SlickedStatusTag on the owner's ASC and scales MaxWalkSpeed / GravityScale down/up based on
 * AAC_ProjectileLiquid::GetTotalSlickAmount(GetOwner()) — more Slick means slower and heavier.
 * Restores the cached base values when the tag is removed.
 */
UCLASS(MinimalAPI, ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class UAC_SlickReactionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UE_API UAC_SlickReactionComponent(const FObjectInitializer& objectInitializer = FObjectInitializer::Get());

	/** 현재 Slick 총량을 다시 읽어 이동 디버프를 갱신한다. 태그 변화 없이 총량만 바뀌었을 때
	 *  (블롭 병합/소멸) AAC_ProjectileLiquid::NotifySlickAmountChanged 가 호출한다. 멱등. */
	UE_API void RefreshMovementScale();

protected:
	/** Status.Slicked 에 해당하는 태그. 에디터에서 지정(네이티브 선언 없음, Config/DefaultGameplayTags.ini 로만 등록). */
	UPROPERTY(EditDefaultsOnly, Category = "Slick Reaction")
	FGameplayTag SlickedStatusTag;

	/** Slick 양이 AmountForFullSlow 이상이면 MaxWalkSpeed 가 BaseMaxWalkSpeed * MinSpeedMultiplier 까지 줄어든다. */
	UPROPERTY(EditDefaultsOnly, Category = "Slick Reaction", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MinSpeedMultiplier = 0.4f;

	/** Slick 양이 AmountForFullSlow 이상이면 GravityScale 이 BaseGravityScale * MaxGravityMultiplier 까지 늘어난다. */
	UPROPERTY(EditDefaultsOnly, Category = "Slick Reaction", meta = (ClampMin = "1.0"))
	float MaxGravityMultiplier = 2.5f;

	/** 이 Slick 양에서 배율이 최댓값에 도달한다(선형 보간). */
	UPROPERTY(EditDefaultsOnly, Category = "Slick Reaction", meta = (ClampMin = "0.01"))
	float AmountForFullSlow = 4.0f;

	UE_API virtual void BeginPlay() override;
	UE_API virtual void EndPlay(const EEndPlayReason::Type endPlayReason) override;

private:
	/** PawnExtensionComponent 가 ASC 초기화를 알려올 때(또는 이미 초기화된 상태면 즉시) 호출된다. */
	void OnPawnAbilitySystemInitialized();

	/** 주어진 ASC 에 Status.Slicked 태그 이벤트를 등록하고, 이미 붙어 있는 태그를 한 번 반영한다. */
	void RegisterSlickedTagEvent(UAbilitySystemComponent* asc);

	void OnSlickedTagChanged(const FGameplayTag tag, int32 newCount);
	void RestoreBaseMovement();

	TWeakObjectPtr<UCharacterMovementComponent> CachedMovement;
	float BaseMaxWalkSpeed = 0.0f;
	float BaseGravityScale = 1.0f;
	bool bBaseValuesCached = false;

	/** 태그 이벤트를 등록한 ASC. EndPlay 에서 같은 ASC 에서 해제해야 하므로 캐시. */
	TWeakObjectPtr<UAbilitySystemComponent> RegisteredASC;

	FDelegateHandle TagChangedDelegateHandle;
};

#undef UE_API
