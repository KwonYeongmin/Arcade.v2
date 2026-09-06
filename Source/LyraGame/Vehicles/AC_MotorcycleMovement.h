// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameFramework/PawnMovementComponent.h"

#include "AC_MotorcycleMovement.generated.h"

/**
 * UAC_MotorcycleMovement
 *
 * 오토바이 아케이드 주행. ChaosVehicles 를 쓰지 않는다.
 * 린(기울기)은 비주얼 전용이라 2륜 전복이 물리적으로 불가능하다.
 *
 * 파라미터 출처: docs/Rules/plan/vehicle_motorcycle.output.json (driving 블록)
 */
UCLASS()
class UAC_MotorcycleMovement : public UPawnMovementComponent
{
    GENERATED_BODY()

public:
    UAC_MotorcycleMovement();

    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    /** -1(제동/후진) ~ +1(전진) */
    void SetThrottleInput(float Value) { ThrottleInput = FMath::Clamp(Value, -1.0f, 1.0f); }

    /** -1(좌) ~ +1(우) */
    void SetSteerInput(float Value) { SteerInput = FMath::Clamp(Value, -1.0f, 1.0f); }

    /** 현재 속력 (cm/s, 항상 >= 0) */
    float GetSpeed() const { return FMath::Abs(ForwardSpeed); }

    /** 현재 린 각도 (deg). AnimInstance 가 읽는다. */
    float GetLeanAngle() const { return CurrentLean; }

protected:
    // --- 기획서 §4 파라미터 ---

    UPROPERTY(EditDefaultsOnly, Category = "Motorcycle|Driving", meta = (ClampMin = "1.0"))
    float MaxSpeed = 2000.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Motorcycle|Driving", meta = (ClampMin = "1.0"))
    float Acceleration = 1500.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Motorcycle|Driving", meta = (ClampMin = "0.0"))
    float Braking = 2500.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Motorcycle|Driving", meta = (ClampMin = "0.0"))
    float TurnRate = 180.0f;

    /** 이 속도 미만이면 조향 불가. 제자리 회전을 막아 탈 것다운 회전 반경을 만든다. */
    UPROPERTY(EditDefaultsOnly, Category = "Motorcycle|Driving", meta = (ClampMin = "0.0"))
    float MinSpeedToSteer = 100.0f;

    /** 코너링 기울기 최대치. 비주얼 전용이며 물리에 관여하지 않는다. */
    UPROPERTY(EditDefaultsOnly, Category = "Motorcycle|Driving", meta = (ClampMin = "0.0", ClampMax = "89.0"))
    float MaxLeanAngle = 35.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Motorcycle|Driving")
    bool bGroundAlign = true;

    UPROPERTY(EditDefaultsOnly, Category = "Motorcycle|Driving", meta = (ClampMin = "1.0"))
    float GroundTraceDistance = 200.0f;

    /** 후진 최대 속도는 전진의 이 비율까지만 */
    UPROPERTY(EditDefaultsOnly, Category = "Motorcycle|Driving", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float ReverseSpeedRatio = 0.3f;

private:
    float ThrottleInput = 0.0f;
    float SteerInput = 0.0f;
    float ForwardSpeed = 0.0f;
    float CurrentLean = 0.0f;
};
