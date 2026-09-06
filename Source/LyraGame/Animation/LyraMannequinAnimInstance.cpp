// Copyright Epic Games, Inc. All Rights Reserved.

#include "LyraMannequinAnimInstance.h"

#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Character/Mover/AC_CharacterBase.h"
#include "Kismet/KismetMathLibrary.h"
#include "KismetAnimationLibrary.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(LyraMannequinAnimInstance)


ULyraMannequinAnimInstance::ULyraMannequinAnimInstance(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

// ─────────────────────────────────────────────────────────────────────────────
// Life cycle
// ─────────────────────────────────────────────────────────────────────────────

void ULyraMannequinAnimInstance::InitializeWithAbilitySystem(UAbilitySystemComponent* ASC)
{
	// The Blueprint CDO stores None property names in GameplayTagPropertyMap because the
	// Blueprint picker only shows BP-local variables, not inherited C++ properties.
	// Rebuild the mappings programmatically so Initialize resolves the correct C++ properties.
	// static const struct { const TCHAR* Tag; const TCHAR* Property; } Bindings[] = {
	// 	{ TEXT("Event.Movement.ADS"),        TEXT("GameplayTag_IsADS")       },
	// 	{ TEXT("Event.Movement.WeaponFire"), TEXT("GameplayTag_IsFiring")    },
	// 	{ TEXT("Event.Movement.Reload"),     TEXT("GameplayTag_IsReloading") },
	// 	{ TEXT("Event.Movement.Dash"),       TEXT("GameplayTag_IsDashing")   },
	// 	{ TEXT("Event.Movement.Melee"),      TEXT("GameplayTag_IsMelee")     },
	// };
	// 
	// GameplayTagPropertyMap.Reset();
	// for (const auto& B : Bindings)
	// {
	// 	FGameplayTagBlueprintPropertyMapping Mapping;
	// 	Mapping.TagToMap    = FGameplayTag::RequestGameplayTag(FName(B.Tag));
	// 	Mapping.PropertyName = FName(B.Property);
	// 	GameplayTagPropertyMap.PropertyMappings.Add(Mapping);
	// }

	Super::InitializeWithAbilitySystem(ASC);
}

#if WITH_EDITOR
EDataValidationResult ULyraMannequinAnimInstance::IsDataValid(FDataValidationContext& Context) const
{
	// Skip ULyraAnimInstance::IsDataValid which validates GameplayTagPropertyMap — our
	// bindings are rebuilt programmatically in InitializeWithAbilitySystem.
	return UAnimInstance::IsDataValid(Context);
}
#endif // WITH_EDITOR

void ULyraMannequinAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();

	IsFirstUpdate = true;
}

void ULyraMannequinAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	// Cache pawn state on the main thread for use in thread-safe update.
	const ACharacter* Character = Cast<ACharacter>(TryGetPawnOwner());
	if (!Character)
	{
		if (const AAC_CharacterBase* MoverCharacter = Cast<AAC_CharacterBase>(TryGetPawnOwner()))
		{
			CachedActorLocation = MoverCharacter->GetActorLocation();
			CachedActorRotation = MoverCharacter->GetActorRotation();
			WorldVelocity = MoverCharacter->GetActorTransform().TransformVectorNoScale(MoverCharacter->LocalVelocity);
			CachedCurrentAcceleration = FVector::ZeroVector;
			bCachedIsMovingOnGround = !MoverCharacter->bIsFalling;
			bCachedIsCrouched = false;
			CachedMovementMode = MoverCharacter->bIsFalling ? MOVE_Falling : MOVE_Walking;
			CachedGravityZ = -980.0f;
			CachedAimPitch = MoverCharacter->GetBaseAimRotation().Pitch;
			IsOnGround = !MoverCharacter->bIsFalling;
			IsJumping = MoverCharacter->bIsJumpStarting;
			IsFalling = MoverCharacter->bIsFalling && !MoverCharacter->bIsJumpStarting;
		}
		return;
	}

	const UCharacterMovementComponent* MovComp = Character->GetCharacterMovement();
	if (!MovComp)
	{
		return;
	}

	CachedActorLocation       = Character->GetActorLocation();
	CachedActorRotation       = Character->GetActorRotation();
	WorldVelocity             = MovComp->Velocity;
	CachedCurrentAcceleration = MovComp->GetCurrentAcceleration();
	bCachedIsMovingOnGround   = MovComp->IsMovingOnGround();
	bCachedIsCrouched         = Character->bIsCrouched;
	CachedMovementMode        = MovComp->MovementMode;
	CachedGravityZ            = MovComp->GetGravityZ();
	CachedAimPitch            = Character->GetBaseAimRotation().Pitch;
}

void ULyraMannequinAnimInstance::NativeThreadSafeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeThreadSafeUpdateAnimation(DeltaSeconds);

	if (!TryGetPawnOwner())
	{
		return;
	}

	// Mirror of ABP_Mannequin_Base BlueprintThreadSafeUpdateAnimation call order.
	UpdateLocationData(DeltaSeconds);
	UpdateRotationData(DeltaSeconds);
	UpdateVelocityData();
	UpdateAccelerationData();
	UpdateWallDetectionHeuristic();
	UpdateCharacterStateData(DeltaSeconds);
	UpdateBlendWeightData(DeltaSeconds);
	UpdateRootYawOffset(DeltaSeconds);
	UpdateAimingData();
	UpdateJumpFallData();

	EnableControlRig = ShouldEnableControlRig();

	IsFirstUpdate = false;
}

// ─────────────────────────────────────────────────────────────────────────────
// Thread-safe Update helpers
// ─────────────────────────────────────────────────────────────────────────────

void ULyraMannequinAnimInstance::UpdateLocationData(float DeltaTime)
{
	// WorldLocation holds the previous frame's location.
	DisplacementSinceLastUpdate = FVector::DistXY(CachedActorLocation, WorldLocation);
	WorldLocation               = CachedActorLocation;

	DisplacementSpeed = FMath::IsNearlyZero(DeltaTime) ? 0.0f
	                                                    : DisplacementSinceLastUpdate / DeltaTime;

	// On first tick there is no meaningful delta yet.
	if (IsFirstUpdate)
	{
		DisplacementSinceLastUpdate = 0.0f;
		DisplacementSpeed           = 0.0f;
	}
}

void ULyraMannequinAnimInstance::UpdateRotationData(float DeltaTime)
{
	// WorldRotation holds the previous frame's rotation.
	YawDeltaSinceLastUpdate = FRotator::NormalizeAxis(CachedActorRotation.Yaw - WorldRotation.Yaw);
	WorldRotation           = CachedActorRotation;

	YawDeltaSpeed = FMath::IsNearlyZero(DeltaTime) ? 0.0f
	                                                : YawDeltaSinceLastUpdate / DeltaTime;

	// Lean multiplier: standing = 0.0375, crouching or ADS = 0.025.
	// const float LeanMultiplier = (IsCrouching || GameplayTag_IsADS) ? 0.025f : 0.0375f;
	// AdditiveLeanAngle = YawDeltaSpeed * LeanMultiplier;

	if (IsFirstUpdate)
	{
		YawDeltaSinceLastUpdate = 0.0f;
		// AdditiveLeanAngle       = 0.0f;
	}
}

void ULyraMannequinAnimInstance::UpdateVelocityData()
{
	const bool bWasMovingLastUpdate = !LocalVelocity2D.IsNearlyZero();

	const FVector WorldVelocity2D(WorldVelocity.X, WorldVelocity.Y, 0.0f);
	LocalVelocity2D = WorldRotation.UnrotateVector(WorldVelocity2D);

	LocalVelocityDirectionAngle = UKismetAnimationLibrary::CalculateDirection(WorldVelocity2D, WorldRotation);
	LocalVelocityDirectionAngleWithOffset = LocalVelocityDirectionAngle - RootYawOffset;

	// BP 실행 순서 및 인자와 일치: WithOffset 먼저, CurrentDirection = Forward, UseCurrentDirection = WasMoving
	LocalVelocityDirection = SelectCardinalDirectionFromAngle(
		LocalVelocityDirectionAngleWithOffset, CardinalDirectionDeadZone,
		EAnimEnum_CardinalDirection::Forward, bWasMovingLastUpdate);

	LocalVelocityDirectionNoOffset = SelectCardinalDirectionFromAngle(
		LocalVelocityDirectionAngle, CardinalDirectionDeadZone,
		EAnimEnum_CardinalDirection::Forward, bWasMovingLastUpdate);

	HasVelocity = !FMath::IsNearlyZero(LocalVelocity2D.SizeSquared2D(), 1e-6);
}

void ULyraMannequinAnimInstance::UpdateAccelerationData()
{
	WorldAcceleration2D = FVector(CachedCurrentAcceleration.X, CachedCurrentAcceleration.Y, 0.0f);
	LocalAcceleration2D = WorldRotation.UnrotateVector(WorldAcceleration2D);
	HasAcceleration     = !FMath::IsNearlyZero(LocalAcceleration2D.SizeSquared2D(), SMALL_NUMBER);

	// Blend pivot direction toward the current acceleration direction.
	const FVector NormalizedAccel = WorldAcceleration2D.GetSafeNormal();
	PivotDirection2D = FMath::Lerp(PivotDirection2D, NormalizedAccel, 0.5f);
	PivotDirection2D = PivotDirection2D.GetSafeNormal();

	// Cardinal direction from acceleration is used for pivot intent detection.
	const float AccelAngle = UKismetAnimationLibrary::CalculateDirection(PivotDirection2D, WorldRotation);
	CardinalDirectionFromAcceleration = SelectCardinalDirectionFromAngle(
		AccelAngle, CardinalDirectionDeadZone, EAnimEnum_CardinalDirection::Forward, false);
}

void ULyraMannequinAnimInstance::UpdateWallDetectionHeuristic()
{
	// Heuristic: large angle between accel and velocity + high accel + low speed = wall.
	const float AccelLen = LocalAcceleration2D.Size2D();
	const float VelLen   = LocalVelocity2D.Size2D();
	const float DotProduct = FVector::DotProduct(
		LocalAcceleration2D.GetSafeNormal2D(), LocalVelocity2D.GetSafeNormal2D());

	IsRunningIntoWall =
		(AccelLen > 0.1f) &&
		(VelLen   < 200.0f) &&
		FMath::IsWithinInclusive(DotProduct, -0.6f, 0.6f);
}

void ULyraMannequinAnimInstance::UpdateCharacterStateData(float DeltaTime)
{
	// Ground
	// IsOnGround = bCachedIsMovingOnGround;
	// 
	// // Crouch state change detection
	// const bool bWasCrouchingLastUpdate = IsCrouching;
	// IsCrouching      = bCachedIsCrouched;
	// CrouchStateChange = (IsCrouching != bWasCrouchingLastUpdate);
	// 
	// // ADS state change detection (WasADSLastUpdate persists, GameplayTag_IsADS updated by tag binding)
	// ADSStateChanged   = (GameplayTag_IsADS != WasADSLastUpdate);
	// WasADSLastUpdate  = GameplayTag_IsADS;
	// 
	// // Fired weapon timer
	// if (GameplayTag_IsFiring)
	// 	TimeSinceFiredWeapon = 0.0f;
	// else
	// 	TimeSinceFiredWeapon += DeltaTime;
	// 
	// // Jump / Fall from movement mode
	// if (CachedMovementMode == MOVE_Falling)
	// {
	// 	if (WorldVelocity.Z > 0.0f)
	// 	{
	// 		IsJumping = true;
	// 		IsFalling = false;
	// 	}
	// 	else
	// 	{
	// 		IsJumping = false;
	// 		IsFalling = true;
	// 	}
	// }
	// else
	// {
	// 	IsJumping = false;
	// 	IsFalling = false;
	// }
}

void ULyraMannequinAnimInstance::UpdateBlendWeightData(float DeltaTime)
{
	const float TargetWeight = (bCachedIsMovingOnGround && IsOnGround) ? 1.0f : 0.0f;
	UpperbodyDynamicAdditiveWeight = FMath::FInterpTo(
		UpperbodyDynamicAdditiveWeight, TargetWeight, DeltaTime, 6.0f);
}

void ULyraMannequinAnimInstance::UpdateRootYawOffset(float DeltaTime)
{
	// then_0: If Accumulate mode, subtract yaw delta to keep root fixed relative to world.
	if (RootYawOffsetMode == ELyraRootYawOffsetMode::Accumulate)
	{
		SetRootYawOffset(RootYawOffset - YawDeltaSinceLastUpdate);
	}

	// then_1: If BlendOut or Dashing, spring-interpolate offset toward 0.
	// if (RootYawOffsetMode == ELyraRootYawOffsetMode::BlendOut || GameplayTag_IsDashing)
	// {
	// 	const float NewOffset = UKismetMathLibrary::FloatSpringInterp(
	// 		RootYawOffset, 0.0f, RootYawOffsetSpringState,
	// 		/*Stiffness=*/80.0f, /*CriticalDamping=*/1.0f, DeltaTime,
	// 		/*Mass=*/1.0f, /*TargetVelocityAmount=*/0.5f,
	// 		/*bClamp=*/false, /*Min=*/-180.0f, /*Max=*/180.0f, /*bInitFromTarget=*/false);
	// 	SetRootYawOffset(NewOffset);
	// }

	// then_2: Reset mode to BlendOut — state machine must re-request each frame.
	RootYawOffsetMode = ELyraRootYawOffsetMode::BlendOut;
}

void ULyraMannequinAnimInstance::UpdateAimingData()
{
	AimPitch = FRotator::NormalizeAxis(CachedAimPitch);
}

void ULyraMannequinAnimInstance::UpdateJumpFallData()
{
	if (IsJumping && CachedGravityZ < 0.0f)
	{
		// Time to reach apex = vertical speed / |gravity|
		TimeToJumpApex = WorldVelocity.Z / -CachedGravityZ;
	}
	else
	{
		TimeToJumpApex = 0.0f;
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// TurnInPlace helpers
// ─────────────────────────────────────────────────────────────────────────────

void ULyraMannequinAnimInstance::ProcessTurnYawCurve()
{
	// Save the previous frame's TurnYawCurveValue before any update.
	const float PreviousValue = TurnYawCurveValue;

	const float TurnYawWeight = GetCurveValue(FName("TurnYawWeight"));

	if (FMath::IsNearlyZero(TurnYawWeight, 0.0001f))
	{
		TurnYawCurveValue = 0.0f;
		return;
	}

	// "Unweight" RemainingTurnYaw to get the full rotation angle.
	TurnYawCurveValue = GetCurveValue(FName("RemainingTurnYaw")) / TurnYawWeight;

	// Avoid applying on the first frame the curve becomes relevant (PreviousValue would be 0
	// while TurnYawCurveValue jumps to some large angle — no actual rotation has occurred yet).
	if (!FMath::IsNearlyZero(PreviousValue, 0.0001f) &&
		!FMath::IsNearlyEqual(TurnYawCurveValue, PreviousValue, 0.0001f))
	{
		SetRootYawOffset(RootYawOffset - (TurnYawCurveValue - PreviousValue));
	}
}

void ULyraMannequinAnimInstance::SetRootYawOffset(float InRootYawOffset)
{
	if (!bEnableRootYawOffset)
	{
		RootYawOffset = 0.0f;
		AimYaw        = 0.0f;
		return;
	}

	const FVector2D& ClampRange = IsCrouching ? RootYawOffsetAngleClampCrouched
	                                           : RootYawOffsetAngleClamp;

	const float Normalized = FRotator::NormalizeAxis(InRootYawOffset);

	// If both clamp bounds are zero the feature is unconfigured — skip clamping.
	RootYawOffset = (FMath::IsNearlyZero(ClampRange.X) && FMath::IsNearlyZero(ClampRange.Y))
	                    ? Normalized
	                    : FMath::ClampAngle(Normalized, ClampRange.X, ClampRange.Y);

	// Counter the offset in aiming so the weapon stays aligned with the camera.
	AimYaw = -RootYawOffset;
}

// ─────────────────────────────────────────────────────────────────────────────
// Pure helpers
// ─────────────────────────────────────────────────────────────────────────────

EAnimEnum_CardinalDirection ULyraMannequinAnimInstance::SelectCardinalDirectionFromAngle(
	double Angle, double DeadZone, EAnimEnum_CardinalDirection Direction, bool UseCurrentDirection) const
{
	double FwdDeadZone = DeadZone;
	double BwdDeadZone = DeadZone;

	// When currently moving in Fwd/Bwd, double that direction's dead zone to prevent rapid toggling.
	if (UseCurrentDirection)
	{
		switch (Direction)
		{
		case EAnimEnum_CardinalDirection::Forward:
			FwdDeadZone *= 2.0;
			break;
		case EAnimEnum_CardinalDirection::Backward:
			BwdDeadZone *= 2.0;
			break;
		default:
			break;
		}
	}

	const double AbsAngle = FMath::Abs(Angle);

	if (AbsAngle <= 45.0 + FwdDeadZone)
		return EAnimEnum_CardinalDirection::Forward;

	if (AbsAngle >= 135.0 - BwdDeadZone)
		return EAnimEnum_CardinalDirection::Backward;

	return (Angle > 0.0) ? EAnimEnum_CardinalDirection::Right : EAnimEnum_CardinalDirection::Left;
}

EAnimEnum_CardinalDirection ULyraMannequinAnimInstance::GetOppositeCardinalDirection(EAnimEnum_CardinalDirection Direction) const
{
	switch (Direction)
	{
	case EAnimEnum_CardinalDirection::Forward:  return EAnimEnum_CardinalDirection::Backward;
	case EAnimEnum_CardinalDirection::Backward: return EAnimEnum_CardinalDirection::Forward;
	case EAnimEnum_CardinalDirection::Left:     return EAnimEnum_CardinalDirection::Right;
	case EAnimEnum_CardinalDirection::Right:    return EAnimEnum_CardinalDirection::Left;
	default:                                    return EAnimEnum_CardinalDirection::Forward;
	}
}

bool ULyraMannequinAnimInstance::IsMovingPerpendicularToInitialPivot() const
{
	const bool bInitFwdOrBack =
		(PivotInitialDirection == EAnimEnum_CardinalDirection::Forward ||
		 PivotInitialDirection == EAnimEnum_CardinalDirection::Backward);

	const bool bCurrentFwdOrBack =
		(LocalVelocityDirection == EAnimEnum_CardinalDirection::Forward ||
		 LocalVelocityDirection == EAnimEnum_CardinalDirection::Backward);

	// Perpendicular means: initial was Fwd/Back and current is Left/Right, or vice versa.
	return bInitFwdOrBack != bCurrentFwdOrBack;
}

bool ULyraMannequinAnimInstance::ShouldEnableControlRig() const
{
	return UseFootPlacement && (GetCurveValue(FName("DisableLegIK")) <= 0.0f);
}

UCharacterMovementComponent* ULyraMannequinAnimInstance::GetMovementComponent() const
{
	if (const ACharacter* Character = Cast<ACharacter>(TryGetPawnOwner()))
	{
		return Character->GetCharacterMovement();
	}
	return nullptr;
}
