// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Character/Mover/AC_HeroBase.h"

#include "AC_ShooterPawn.generated.h"

#define UE_API LYRAGAME_API

struct FInputActionValue;

/**
 * AAC_ShooterPawn
 *
 * Mover 계열에서 AAC_ShooterCharacter 에 대응하는 층이다. 이름이 다른 이유는
 * AAC_ShooterCharacter 가 CMC 기반으로 계속 존재하기 때문이다 — 두 구현을 나란히 두고
 * 네트워크 부하를 비교하는 것이 이 프로젝트의 목적 중 하나다.
 *
 * 현재 담고 있는 것: 비행.
 * 아직 이식하지 않은 것: 퀵슬롯 · 아웃라인 · 이모트 오디오 · 스폰 시 숨김 · 체력 연동.
 *
 * 이식 전에 반드시 AAC_CharacterBase 에 이미 있는지 확인할 것. 인벤토리를 옮기려다
 * 이미 있는 것을 중복 구현해 되돌린 전례가 있다(커밋 5569cb08 → 70a1bc69).
 */
UCLASS(MinimalAPI, Blueprintable)
class AAC_ShooterPawn : public AAC_HeroBase
{
	GENERATED_BODY()

public:
	UE_API AAC_ShooterPawn(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/**
	 * 비행에서 하강할 때 적용할 중력 배율. 낮을수록 천천히 내려온다.
	 *
	 * CMC 판의 FlightDescendGravityScale 을 그대로 옮긴 것이다. CMC 는 GravityScale 프로퍼티가
	 * 있지만 Mover 에는 없어서, 월드 중력에 이 값을 곱한 가속도를 SetGravityOverride 로 건다.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Shooter|Flight", meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "1.0"))
	float FlightDescendGravityScale = 0.2f;

	/**
	 * 비행 중 상승 키를 누르고 있을 때 이동 의도에 더할 위쪽 성분.
	 *
	 * 이동 의도는 크기 1 로 정규화되므로 이 값은 "수평 대비 얼마나 위로 가고 싶은가" 다.
	 * 1.0 이면 45도로 상승한다. 실제 속도는 Mover 의 비행 최대 속도가 정한다.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Shooter|Flight", meta = (ClampMin = "0.0"))
	float FlightAscendInputScale = 1.0f;


protected:
	UE_API virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	UE_API virtual void Tick(float DeltaSeconds) override;

	/**
	 * 비행 모드 요청을 입력 커맨드에 실어 보낸다.
	 *
	 * UMoverComponent::QueueNextMode 로 모드를 직접 밀면 그 호출을 한 쪽에서만 모드가 바뀐다.
	 * SuggestedMovementMode 는 FCharacterDefaultInputs 의 일부라 서버로 복제되고 예측·롤백
	 * 경로를 함께 탄다. 멀티플레이에서는 이쪽만 옳다.
	 */
	UE_API virtual void ProduceInput_Implementation(int32 SimTimeMs, FMoverInputCmdContext& InputCmd) override;

private:
	UE_API void Input_AscendTriggered(const FInputActionValue& Value);
	UE_API void Input_AscendReleased(const FInputActionValue& Value);
	UE_API void Input_DescendActive(const FInputActionValue& Value);

	/** 다음 ProduceInput 이 요청할 이동 모드. NAME_None 이면 요청하지 않는다. */
	FName PendingMovementMode = NAME_None;

	/** 하강 중 낮춘 중력이 걸려 있는지. 착지하면 되돌린다. */
	bool bDescendGravityApplied = false;

	/** 상승 키를 누르고 있는 동안 참. ProduceInput 이 이동 의도에 위쪽 성분을 더한다. */
	bool bAscendHeld = false;

	/** 비행 중 상승 입력을 이동 의도에 더한다. ProduceInput 에서만 부른다. */
	UE_API void AddAscendToMoveInput(FMoverInputCmdContext& InputCmd) const;

	UE_API void ApplyDescendGravity();
	UE_API void ClearDescendGravity();
};

#undef UE_API
