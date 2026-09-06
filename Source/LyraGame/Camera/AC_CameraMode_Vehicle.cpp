// Copyright Epic Games, Inc. All Rights Reserved.

#include "Camera/AC_CameraMode_Vehicle.h"

#include "GameFramework/Actor.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AC_CameraMode_Vehicle)

UAC_CameraMode_Vehicle::UAC_CameraMode_Vehicle()
{
	FieldOfView = 90.0f;
	ViewPitchMin = -89.0f;
	ViewPitchMax = 89.0f;
}

FVector UAC_CameraMode_Vehicle::GetPivotLocation() const
{
	// 기반 클래스는 논-캐릭터 폰에 GetPawnViewLocation() 을 쓰는데, 이는
	// BaseEyeHeight 를 더한 값이라 탈 것에는 설정된 적 없는 값에 기댄다.
	// 액터 위치를 그대로 쓰고, 높이는 이 클래스의 HeightOffset 이 전담한다.
	return GetTargetActor()->GetActorLocation();
}

FRotator UAC_CameraMode_Vehicle::GetPivotRotation() const
{
	// 컨트롤러의 시야 방향(마우스) 대신 액터의 실제 회전을 쓴다. 탈 것의 진행 방향은
	// 조향이 결정하며 컨트롤러 회전과 무관하다 — 클래스 코멘트 참고.
	return GetTargetActor()->GetActorRotation();
}

void UAC_CameraMode_Vehicle::UpdateView(float DeltaTime)
{
	const FVector PivotLocation = GetPivotLocation();
	const FRotator PivotRotation = GetPivotRotation();

	// Yaw 만 카메라가 따라간다. 탈 것의 Pitch/Roll(비탈길, 코너링 린)까지 그대로
	// 물려받으면 카메라가 같이 기울어 어지럽다.
	const FRotator YawOnlyRotation(0.0f, PivotRotation.Yaw, 0.0f);

	FRotator DesiredRotation = YawOnlyRotation;
	DesiredRotation.Pitch = PitchAngle;

	View.Rotation = DesiredRotation;
	View.ControlRotation = View.Rotation;
	View.FieldOfView = FieldOfView;

	View.Location = PivotLocation
		- (YawOnlyRotation.Vector() * Distance)
		+ (FVector::UpVector * HeightOffset);
}
