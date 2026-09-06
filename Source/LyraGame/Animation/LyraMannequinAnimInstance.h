// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "LyraAnimInstance.h"
#include "Kismet/KismetMathLibrary.h"
#include "LyraMannequinAnimInstance.generated.h"

class UCharacterMovementComponent;

UENUM(BlueprintType)
enum class EAnimEnum_CardinalDirection : uint8
{
	Forward  = 0,
	Backward = 1,
	Left     = 2,
	Right    = 3,
};

UENUM(BlueprintType)
enum class ELyraRootYawOffsetMode : uint8
{
	BlendOut   = 0,  // Default: smoothly spring-interp offset to 0
	Hold       = 1,  // Keep current offset unchanged
	Accumulate = 2,  // Subtract yaw delta each frame to keep feet planted (idle/stop states)
};

/**
 * ULyraMannequinAnimInstance
 *
 * C++ port of ABP_Mannequin_Base locomotion logic.
 * ABP_Mannequin_Base should be reparented to this class.
 */
UCLASS(MinimalAPI, Blueprintable)
class ULyraMannequinAnimInstance : public ULyraAnimInstance
{
	GENERATED_BODY()

public:

	ULyraMannequinAnimInstance(const FObjectInitializer& ObjectInitializer);

	virtual void InitializeWithAbilitySystem(UAbilitySystemComponent* ASC) override;



	// ── Location ─────────────────────────────────────────────────────────────
	UPROPERTY(BlueprintReadWrite, Category = "Locomotion|Location")
	FVector WorldLocation = FVector::ZeroVector;

	UPROPERTY(BlueprintReadWrite, Category = "Locomotion|Location")
	float DisplacementSinceLastUpdate = 0.0f;

	UPROPERTY(BlueprintReadWrite, Category = "Locomotion|Location")
	float DisplacementSpeed = 0.0f;

	// ── Rotation ─────────────────────────────────────────────────────────────
	UPROPERTY(BlueprintReadWrite, Category = "Locomotion|Rotation")
	FRotator WorldRotation = FRotator::ZeroRotator;

	UPROPERTY(BlueprintReadWrite, Category = "Locomotion|Rotation")
	float YawDeltaSinceLastUpdate = 0.0f;

	UPROPERTY(BlueprintReadWrite, Category = "Locomotion|Rotation")
	float YawDeltaSpeed = 0.0f;

	UPROPERTY(BlueprintReadWrite, Category = "Locomotion|Rotation")
	float AdditiveLeanAngle = 0.0f;

	// ── Velocity ─────────────────────────────────────────────────────────────
	UPROPERTY(BlueprintReadWrite, Category = "Locomotion|Velocity")
	FVector WorldVelocity = FVector::ZeroVector;

	UPROPERTY(BlueprintReadWrite, Category = "Locomotion|Velocity")
	FVector LocalVelocity2D = FVector::ZeroVector;

	UPROPERTY(BlueprintReadWrite, Category = "Locomotion|Velocity")
	float LocalVelocityDirectionAngle = 0.0f;

	UPROPERTY(BlueprintReadWrite, Category = "Locomotion|Velocity")
	float LocalVelocityDirectionAngleWithOffset = 0.0f;

	UPROPERTY(BlueprintReadWrite, Category = "Locomotion|Velocity")
	EAnimEnum_CardinalDirection LocalVelocityDirection = EAnimEnum_CardinalDirection::Forward;

	UPROPERTY(BlueprintReadWrite, Category = "Locomotion|Velocity")
	EAnimEnum_CardinalDirection LocalVelocityDirectionNoOffset = EAnimEnum_CardinalDirection::Forward;

	UPROPERTY(BlueprintReadWrite, Category = "Locomotion|Velocity")
	bool HasVelocity = false;

	// ── Acceleration ─────────────────────────────────────────────────────────
	UPROPERTY(BlueprintReadWrite, Category = "Locomotion|Acceleration")
	FVector WorldAcceleration2D = FVector::ZeroVector;

	UPROPERTY(BlueprintReadWrite, Category = "Locomotion|Acceleration")
	FVector LocalAcceleration2D = FVector::ZeroVector;

	UPROPERTY(BlueprintReadWrite, Category = "Locomotion|Acceleration")
	bool HasAcceleration = false;

	UPROPERTY(BlueprintReadWrite, Category = "Locomotion|Acceleration")
	FVector PivotDirection2D = FVector::ZeroVector;

	UPROPERTY(BlueprintReadWrite, Category = "Locomotion|Acceleration")
	EAnimEnum_CardinalDirection CardinalDirectionFromAcceleration = EAnimEnum_CardinalDirection::Forward;

	// ── Character State ───────────────────────────────────────────────────────
	UPROPERTY(BlueprintReadWrite, Category = "Locomotion|CharacterState")
	bool IsOnGround = false;

	UPROPERTY(BlueprintReadWrite, Category = "Locomotion|CharacterState")
	bool IsCrouching = false;

	UPROPERTY(BlueprintReadWrite, Category = "Locomotion|CharacterState")
	bool CrouchStateChange = false;

	UPROPERTY(BlueprintReadWrite, Category = "Locomotion|CharacterState")
	bool WasADSLastUpdate = false;

	UPROPERTY(BlueprintReadWrite, Category = "Locomotion|CharacterState")
	bool ADSStateChanged = false;

	UPROPERTY(BlueprintReadWrite, Category = "Locomotion|CharacterState")
	float TimeSinceFiredWeapon = 0.0f;

	UPROPERTY(BlueprintReadWrite, Category = "Locomotion|CharacterState")
	bool IsJumping = false;

	UPROPERTY(BlueprintReadWrite, Category = "Locomotion|CharacterState")
	bool IsFalling = false;

	UPROPERTY(BlueprintReadWrite, Category = "Locomotion|CharacterState")
	bool IsRunningIntoWall = false;

	// ── Jump / Fall ───────────────────────────────────────────────────────────
	UPROPERTY(BlueprintReadWrite, Category = "Locomotion|JumpFall")
	float TimeToJumpApex = 0.0f;

	// ── Aiming ───────────────────────────────────────────────────────────────
	UPROPERTY(BlueprintReadWrite, Category = "Locomotion|Aiming")
	float AimPitch = 0.0f;

	UPROPERTY(BlueprintReadWrite, Category = "Locomotion|Aiming")
	float AimYaw = 0.0f;

	// ── Blend Weights ─────────────────────────────────────────────────────────
	UPROPERTY(BlueprintReadWrite, Category = "Locomotion|BlendWeight")
	float UpperbodyDynamicAdditiveWeight = 0.0f;

	UPROPERTY(BlueprintReadWrite, Category = "Locomotion|BlendWeight")
	bool EnableControlRig = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Locomotion|BlendWeight")
	bool UseFootPlacement = false;

	// ── Root Yaw Offset ───────────────────────────────────────────────────────
	UPROPERTY(BlueprintReadWrite, Category = "Locomotion|RootYawOffset")
	float RootYawOffset = 0.0f;

	UPROPERTY(BlueprintReadWrite, Category = "Locomotion|RootYawOffset")
	ELyraRootYawOffsetMode RootYawOffsetMode = ELyraRootYawOffsetMode::BlendOut;

	UPROPERTY(BlueprintReadWrite, Category = "Locomotion|RootYawOffset")
	FFloatSpringState RootYawOffsetSpringState;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Locomotion|RootYawOffset")
	FVector2D RootYawOffsetAngleClamp = FVector2D(-120.0f, 120.0f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Locomotion|RootYawOffset")
	FVector2D RootYawOffsetAngleClampCrouched = FVector2D(-90.0f, 90.0f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Locomotion|RootYawOffset")
	bool bEnableRootYawOffset = true;

	// ── Turn Yaw Curve ────────────────────────────────────────────────────────
	UPROPERTY(BlueprintReadWrite, Category = "Locomotion|TurnYaw")
	float TurnYawCurveValue = 0.0f;

	// ── Locomotion States ─────────────────────────────────────────────────────
	UPROPERTY(BlueprintReadWrite, Category = "Locomotion|States")
	EAnimEnum_CardinalDirection StartDirection = EAnimEnum_CardinalDirection::Forward;

	UPROPERTY(BlueprintReadWrite, Category = "Locomotion|States")
	EAnimEnum_CardinalDirection PivotInitialDirection = EAnimEnum_CardinalDirection::Forward;

	UPROPERTY(BlueprintReadWrite, Category = "Locomotion|States")
	float LastPivotTime = 0.0f;

	UPROPERTY(BlueprintReadWrite, Category = "Locomotion|States")
	bool LinkedLayerChanged = false;

	UPROPERTY(BlueprintReadWrite, Category = "Locomotion|States")
	TObjectPtr<UAnimInstance> LastLinkedLayer;

	UPROPERTY(BlueprintReadWrite, Category = "Locomotion|States")
	bool IsFirstUpdate = true;

	// ── Config ────────────────────────────────────────────────────────────────
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Locomotion|Config")
	float CardinalDirectionDeadZone = 10.0f;

	// ── Pure Helper Functions (AnimGraph 상태머신 함수에서 호출 — BlueprintThreadSafe 필수) ──
	UFUNCTION(BlueprintPure, meta=(BlueprintThreadSafe))
	EAnimEnum_CardinalDirection SelectCardinalDirectionFromAngle(
		double Angle, double DeadZone,
		EAnimEnum_CardinalDirection Direction, bool UseCurrentDirection) const;

	UFUNCTION(BlueprintPure, meta=(BlueprintThreadSafe))
	EAnimEnum_CardinalDirection GetOppositeCardinalDirection(EAnimEnum_CardinalDirection Direction) const;

	UFUNCTION(BlueprintPure, meta=(BlueprintThreadSafe))
	bool IsMovingPerpendicularToInitialPivot() const;

	UFUNCTION(BlueprintPure, meta=(BlueprintThreadSafe))
	bool ShouldEnableControlRig() const;

	UFUNCTION(BlueprintPure, meta=(BlueprintThreadSafe))
	UCharacterMovementComponent* GetMovementComponent() const;

protected:

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif // WITH_EDITOR

	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;
	virtual void NativeThreadSafeUpdateAnimation(float DeltaSeconds) override;

	UFUNCTION(BlueprintCallable, meta=(BlueprintThreadSafe))
	void ProcessTurnYawCurve();

private:

	// ── Thread-safe Update Helpers ────────────────────────────────────────────
	void UpdateLocationData(float DeltaTime);
	void UpdateRotationData(float DeltaTime);
	void UpdateVelocityData();
	void UpdateAccelerationData();
	void UpdateWallDetectionHeuristic();
	void UpdateCharacterStateData(float DeltaTime);
	void UpdateBlendWeightData(float DeltaTime);
	void UpdateRootYawOffset(float DeltaTime);
	void UpdateAimingData();
	void UpdateJumpFallData();

	// TurnInPlace helpers
	void SetRootYawOffset(float InRootYawOffset);

	// ── Cached Pawn Data (populated in NativeUpdateAnimation, main thread) ────
	FVector  CachedActorLocation     = FVector::ZeroVector;
	FRotator CachedActorRotation     = FRotator::ZeroRotator;
	FVector  CachedCurrentAcceleration = FVector::ZeroVector;
	bool     bCachedIsMovingOnGround = false;
	bool     bCachedIsCrouched       = false;
	uint8    CachedMovementMode      = 0;
	float    CachedGravityZ          = -980.0f;
	float    CachedAimPitch          = 0.0f;
};
