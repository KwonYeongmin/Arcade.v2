// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "LyraCameraMode.h"

#include "AC_CameraMode_Vehicle.generated.h"

#define UE_API LYRAGAME_API

/**
 * UAC_CameraMode_Vehicle
 *
 * 탈 것용 3인칭 체이스 카메라. ULyraCameraMode_ThirdPerson 은 피벗 회전에
 * TargetPawn->GetViewRotation() — 즉 컨트롤러의 마우스 시야 방향 — 을 쓴다. 캐릭터는
 * bUseControllerRotationYaw 로 몸이 그 방향을 따라가므로 문제가 없지만, 탈 것은
 * bUseControllerRotationYaw = false 이고 실제 진행 방향은 조향 입력이 만든다
 * (UAC_MotorcycleMovement::TickComponent). 그래서 마우스를 조금만 움직여도 카메라가
 * 보는 방향과 탈 것이 실제로 도는 방향이 어긋나 옆면이나 앞면이 보이게 된다.
 *
 * 이 모드는 피벗 회전을 액터의 실제 회전(대상의 진행 방향)으로 고정한다. 마우스는
 * 더 이상 카메라 방향에 관여하지 않는다 — 항상 탈 것 뒤에서 따라간다.
 */
UCLASS(MinimalAPI)
class UAC_CameraMode_Vehicle : public ULyraCameraMode
{
	GENERATED_BODY()

public:
	UE_API UAC_CameraMode_Vehicle();

protected:
	UE_API virtual FVector GetPivotLocation() const override;
	UE_API virtual FRotator GetPivotRotation() const override;
	UE_API virtual void UpdateView(float DeltaTime) override;

	/** 대상 뒤로 이 거리만큼 물러난다. */
	UPROPERTY(EditDefaultsOnly, Category = "Vehicle Camera", Meta = (ClampMin = "0.0", UIMin = "0.0"))
	float Distance = 500.0f;

	/** 피벗 위치보다 이만큼 위에 카메라를 둔다. */
	UPROPERTY(EditDefaultsOnly, Category = "Vehicle Camera", Meta = (UIMin = "0.0"))
	float HeightOffset = 180.0f;

	/**
	 * 내려다보는 각도(도). 음수가 아래를 본다.
	 * 값이 -90 에 가까울수록 탑뷰, 0 이면 지평선과 수평.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Vehicle Camera", Meta = (ClampMin = "-89.0", ClampMax = "89.0", UIMin = "-89.0", UIMax = "0.0"))
	float PitchAngle = -15.0f;
};

#undef UE_API
