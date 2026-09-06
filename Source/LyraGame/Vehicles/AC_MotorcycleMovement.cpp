// Copyright Epic Games, Inc. All Rights Reserved.

#include "Vehicles/AC_MotorcycleMovement.h"

#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Vehicles/AC_MotorcyclePawn.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AC_MotorcycleMovement)

UAC_MotorcycleMovement::UAC_MotorcycleMovement()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = true;
}

void UAC_MotorcycleMovement::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (!UpdatedComponent || ShouldSkipUpdate(DeltaTime))
    {
        return;
    }

    // --- 1. 종방향 속도 ---
    if (ThrottleInput > 0.0f)
    {
        ForwardSpeed += Acceleration * ThrottleInput * DeltaTime;
    }
    else if (ThrottleInput < 0.0f)
    {
        ForwardSpeed += Braking * ThrottleInput * DeltaTime;
    }
    else
    {
        // 입력이 없으면 제동력의 절반으로 자연 감속한다.
        const float Decel = Braking * 0.5f * DeltaTime;
        ForwardSpeed = (ForwardSpeed > 0.0f)
            ? FMath::Max(0.0f, ForwardSpeed - Decel)
            : FMath::Min(0.0f, ForwardSpeed + Decel);
    }
    ForwardSpeed = FMath::Clamp(ForwardSpeed, -MaxSpeed * ReverseSpeedRatio, MaxSpeed);

    // --- 2. 조향 (최소 속도 이상일 때만) ---
    FRotator NewRotation = UpdatedComponent->GetComponentRotation();
    const bool bCanSteer = FMath::Abs(ForwardSpeed) >= MinSpeedToSteer;
    if (bCanSteer && !FMath::IsNearlyZero(SteerInput))
    {
        // 후진 중에는 조향 방향이 반대가 되어야 자연스럽다.
        const float Direction = (ForwardSpeed >= 0.0f) ? 1.0f : -1.0f;
        NewRotation.Yaw += SteerInput * TurnRate * Direction * DeltaTime;
    }

    // --- 3. 린 (비주얼 전용) ---
    const float SpeedRatio = FMath::Clamp(FMath::Abs(ForwardSpeed) / MaxSpeed, 0.0f, 1.0f);
    const float TargetLean = bCanSteer ? (-SteerInput * MaxLeanAngle * SpeedRatio) : 0.0f;
    CurrentLean = FMath::FInterpTo(CurrentLean, TargetLean, DeltaTime, 5.0f);
    NewRotation.Roll = CurrentLean;

    // --- 4. 지면 정렬 ---
    FVector Delta = UpdatedComponent->GetForwardVector() * ForwardSpeed * DeltaTime;
    if (bGroundAlign)
    {
        const FVector Start = UpdatedComponent->GetComponentLocation();
        const FVector End = Start - FVector(0.0f, 0.0f, GroundTraceDistance);

        FCollisionQueryParams Params(SCENE_QUERY_STAT(MotorcycleGroundAlign), false, GetOwner());

        // 탑승 직후 몇 프레임은 라이더의 콜리전이 아직 꺼지기 전일 수 있다
        // (AttachRider 의 SetActorEnableCollision(false) 와 이 트레이스가 같은 프레임
        // 안에서 순서를 보장받지 못한다). 그 틈에 라이더 캡슐이 트레이스를 막으면
        // 지면을 못 찾고 낙하 분기를 타 오토바이가 통째로 떨어진다. 라이더 자체를
        // 무시 목록에 넣어 그 경합을 원천적으로 없앤다.
        if (const AAC_MotorcyclePawn* BikeOwner = Cast<AAC_MotorcyclePawn>(GetOwner()))
        {
            if (APawn* Rider = BikeOwner->GetRider())
            {
                Params.AddIgnoredActor(Rider);
            }
        }

        FHitResult Hit;
        if (GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params))
        {
            const FRotator Aligned = FRotationMatrix::MakeFromZX(Hit.ImpactNormal, UpdatedComponent->GetForwardVector()).Rotator();
            NewRotation.Pitch = Aligned.Pitch;

            // 캡슐 중심을 지면 위 HoverHeight 에 맞춘다.
            // 지면 높이로 그냥 맞추면 캡슐 아래 절반이 땅에 박혀 매 프레임 충돌하고,
            // 그 결과 속도가 계속 깎여 조향 최소 속도에 영원히 도달하지 못한다.
            float HoverHeight = 60.0f;
            if (const UCapsuleComponent* Cap = Cast<UCapsuleComponent>(UpdatedComponent.Get()))
            {
                HoverHeight = Cap->GetScaledCapsuleHalfHeight();
            }
            const float DesiredZ = Hit.Location.Z + HoverHeight;
            Delta.Z += (DesiredZ - Start.Z);
        }
        else
        {
            // 공중이면 낙하시킨다.
            Delta.Z -= 980.0f * DeltaTime;
        }
    }

    // --- 5. 적용 ---
    FHitResult MoveHit;
    SafeMoveUpdatedComponent(Delta, NewRotation.Quaternion(), true, MoveHit);
    if (MoveHit.IsValidBlockingHit())
    {
        SlideAlongSurface(Delta, 1.0f - MoveHit.Time, MoveHit.Normal, MoveHit);
        // 기획서 §7: 충돌 데미지는 없다. 부딪히면 감속만 한다.
        ForwardSpeed *= 0.5f;
    }

    UpdateComponentVelocity();
}
