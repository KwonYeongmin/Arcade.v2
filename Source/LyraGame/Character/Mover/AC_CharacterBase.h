// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AbilitySystemInterface.h"
#include "CoreMinimal.h"
#include "Character/LyraPawn.h"
#include "GameplayCueInterface.h"
#include "GameplayTagAssetInterface.h"
#include "GameplayTagContainer.h"
#include "MoverSimulationTypes.h"

#include "AC_CharacterBase.generated.h"

class AController;
class ALyraPlayerController;
class ALyraPlayerState;
class UAbilitySystemComponent;
class UCapsuleComponent;
class UCharacterMoverComponent;
class ULyraCameraComponent;
class ULyraHeroComponent;
class ULyraHealthComponent;
class ULyraEquipmentManagerComponent;
class ULyraInventoryItemDefinition;
class ULyraPawnExtensionComponent;
class ULyraPawnComponent_CharacterParts;
class ULyraAbilitySystemComponent;
class USkeletalMeshComponent;
class UAnimInstance;
struct FGameplayTag;
struct FGameplayTagContainer;

/**
 * Minimal Mover-driven pawn used as the movement baseline for UAF animation.
 * Reparented onto ALyraPawn so it joins Lyra's InitState chain; ULyraHeroComponent
 * owns input and camera; Jump is routed through the Mover-specific Gameplay Ability.
 */
UCLASS(Blueprintable)
class LYRAGAME_API AAC_CharacterBase : public ALyraPawn, public IMoverInputProducerInterface, public IAbilitySystemInterface, public IGameplayCueInterface, public IGameplayTagAssetInterface
{
	GENERATED_BODY()

public:
	AAC_CharacterBase(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	UFUNCTION(BlueprintCallable, Category = "Lyra|Character")
	ALyraPlayerController* GetLyraPlayerController() const;

	UFUNCTION(BlueprintCallable, Category = "Lyra|Character")
	ALyraPlayerState* GetLyraPlayerState() const;

	UFUNCTION(BlueprintCallable, Category = "Lyra|Character")
	ULyraAbilitySystemComponent* GetLyraAbilitySystemComponent() const;

	//~ Begin IAbilitySystemInterface
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	//~ End IAbilitySystemInterface

	//~ Begin IGameplayTagAssetInterface
	virtual void GetOwnedGameplayTags(FGameplayTagContainer& TagContainer) const override;
	virtual bool HasMatchingGameplayTag(FGameplayTag TagToCheck) const override;
	virtual bool HasAllMatchingGameplayTags(const FGameplayTagContainer& TagContainer) const override;
	virtual bool HasAnyMatchingGameplayTags(const FGameplayTagContainer& TagContainer) const override;
	//~ End IGameplayTagAssetInterface

	//~ Begin APawn interface
	virtual FVector GetVelocity() const override;
	//~ End APawn interface

	virtual void Tick(float DeltaSeconds) override;

private:
	/** 탈것에 부착된 동안 Mover 의 매 프레임 되쓰기를 되돌린다. AAC_MotorcyclePawn::AttachRider 참고. */
	void HoldAttachmentAgainstMover();

public:
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void PostInitializeComponents() override;
	virtual void BeginPlay() override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void UnPossessed() override;
	virtual void OnRep_Controller() override;
	virtual void OnRep_PlayerState() override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void Reset() override;

	UFUNCTION(BlueprintPure, Category = "UAF Mover|Components")
	UCapsuleComponent* GetCapsuleComponent() const { return CapsuleComponent; }

	UFUNCTION(BlueprintPure, Category = "UAF Mover|Components")
	USkeletalMeshComponent* GetMesh() const { return MeshComponent; }

	UFUNCTION(BlueprintPure, Category = "UAF Mover|Components")
	UCharacterMoverComponent* GetMoverComponent() const { return MoverComponent; }

	UFUNCTION(BlueprintPure, Category = "UAF Mover|Components")
	ULyraCameraComponent* GetCameraComponent() const { return CameraComponent; }

	/** Gameplay Ability boundary for Mover jump; does not expose Character/CMC APIs. */
	UFUNCTION(BlueprintPure, Category = "UAF Mover|Movement")
	bool CanStartMoverJump() const;

	UFUNCTION(BlueprintCallable, Category = "UAF Mover|Movement")
	void StartMoverJump();

	UFUNCTION(BlueprintCallable, Category = "UAF Mover|Movement")
	void StopMoverJump();

	/** Speed along the plane perpendicular to Mover's current up direction. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category = "UAF Mover|Animation")
	float GroundSpeed = 0.0f;

	/** Mover velocity transformed into this Pawn's local space. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category = "UAF Mover|Animation")
	FVector LocalVelocity = FVector::ZeroVector;

	/** Normalized local-space direction of the planar movement velocity. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category = "UAF Mover|Animation")
	FVector MovementDirection = FVector::ZeroVector;

	/** Controller aim yaw relative to this Pawn, clamped to the Aim Offset's authored range. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category = "UAF Mover|Animation")
	float AimYaw = 0.0f;

	/** Controller aim pitch relative to this Pawn, clamped to the Aim Offset's authored range. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category = "UAF Mover|Animation")
	float AimPitch = 0.0f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category = "UAF Mover|Animation")
	bool bIsMoving = false;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category = "UAF Mover|Animation")
	bool bIsFalling = false;

	/** True while airborne and still rising, which is the jump start phase. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category = "UAF Mover|Animation")
	bool bIsJumpStarting = false;

	/**
	 * 정점까지 남은 예상 시간(초). Lyra 원본 ABP_Mannequin_Base::UpdateJumpFallData 와 같은
	 * 공식이다(수직 속도 / 중력 가속도). 낙하 중 상승 국면이 아니면 0.
	 *
	 * 점프 애니메이션 상태머신에서 Apex 상태 진입 조건(TimeToJumpApex < 0.4)으로 쓴다.
	 * 모션 매칭은 이 클립들에 맞지 않는다는 게 실측으로 확인됐다(§11 참고) — 상태머신으로
	 * 되돌아가되, 전이 조건은 Lyra 자신이 이미 검증한 것을 그대로 가져온다.
	 */
	UPROPERTY(BlueprintReadOnly, Transient, Category = "UAF Mover|Animation")
	float TimeToJumpApex = 0.0f;

	/** True for a short recovery window after touching down. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category = "UAF Mover|Animation")
	bool bIsLanding = false;

	/**
	 * 탈것에 탑승 중인지. AAC_MotorcyclePawn::AttachRider / DetachRider 가 설정한다.
	 * 애니메이션 쪽에서 탑승/주행/하차 상태머신 진입 조건으로 쓴다.
	 */
	UPROPERTY(BlueprintReadOnly, Transient, Category = "UAF Mover|Animation")
	bool bIsMounted = false;

	/** Current movement mode copied from the latest Mover Sync State. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category = "UAF Mover|Animation")
	FName MovementMode = NAME_None;

	//~ Helpers that feed B_CharacterBase's BPI_SandboxCharacter_Pawn implementation.
	//~ Judgement/maths live here in C++; the Blueprint only assembles the GASP struct
	//~ and maps the returned index onto the GASP UserDefinedEnums.

	/** Planar speed at or below which the gait is Walk. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UAF Mover|Animation", meta = (ClampMin = "0.0"))
	float WalkGaitMaxSpeed = 200.0f;

	/** Planar speed at or below which the gait is Run; above it the gait is Sprint. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UAF Mover|Animation", meta = (ClampMin = "0.0"))
	float RunGaitMaxSpeed = 550.0f;

	/** Fallback acceleration reported to the anim graph when Mover settings are unavailable. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UAF Mover|Animation", meta = (ClampMin = "0.0"))
	float AnimMaxAcceleration = 800.0f;

	/** Fallback deceleration reported to the anim graph when Mover settings are unavailable. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UAF Mover|Animation", meta = (ClampMin = "0.0"))
	float AnimMaxDeceleration = 2000.0f;

	/** 0 = Walk, 1 = Run, 2 = Sprint, from GroundSpeed against the two thresholds above. */
	UFUNCTION(BlueprintPure, Category = "UAF Mover|Animation")
	int32 GetAnimGaitIndex() const;

	/** World-space aiming rotation for the anim graph (controller aim, not the clamped local AimYaw/Pitch). */
	UFUNCTION(BlueprintPure, Category = "UAF Mover|Animation")
	FRotator GetAnimAimRotation() const;

	/** Approximate ground contact point: actor location dropped by the capsule half height along -Up. */
	UFUNCTION(BlueprintPure, Category = "UAF Mover|Animation")
	FVector GetAnimGroundLocation() const;

	/** Ground normal from the Mover floor query when grounded, otherwise the Mover up direction. */
	UFUNCTION(BlueprintPure, Category = "UAF Mover|Animation")
	FVector GetAnimGroundNormal() const;

	/** World-space acceleration implied by the latest consumed movement input. */
	UFUNCTION(BlueprintPure, Category = "UAF Mover|Animation")
	FVector GetAnimInputAcceleration() const;

	/**
	 * Drives the pistol upper-body layer weight. True only when a ranged weapon is equipped
	 * and the pawn is not riding a vehicle (mounting swaps to the riding pose, which owns the
	 * whole body). First pass treats any ranged weapon as "pistol"; split by weapon type when
	 * rifle / shotgun layers arrive.
	 */
	UFUNCTION(BlueprintPure, Category = "UAF Mover|Animation")
	bool IsPistolEquipped() const;

protected:
	//~ Begin IMoverInputProducerInterface
	virtual void ProduceInput_Implementation(int32 SimTimeMs, FMoverInputCmdContext& InputCmd) override;
	//~ End IMoverInputProducerInterface

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "UAF Mover|Components")
	TObjectPtr<UCapsuleComponent> CapsuleComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "UAF Mover|Components")
	TObjectPtr<USkeletalMeshComponent> MeshComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "UAF Mover|Components")
	TObjectPtr<UCharacterMoverComponent> MoverComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "UAF Mover|Components")
	TObjectPtr<ULyraCameraComponent> CameraComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "UAF Mover|Components")
	TObjectPtr<ULyraPawnExtensionComponent> PawnExtComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "UAF Mover|Components")
	TObjectPtr<ULyraHeroComponent> HeroComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "UAF Mover|Components")
	TObjectPtr<ULyraPawnComponent_CharacterParts> CharacterPartsComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "UAF Mover|Components")
	TObjectPtr<ULyraEquipmentManagerComponent> EquipmentManagerComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "UAF Mover|Components")
	TObjectPtr<ULyraHealthComponent> HealthComponent;

	/** Test-loadout items granted through the standard Lyra controller inventory/quickbar path. */
	UPROPERTY(EditDefaultsOnly, Category = "UAF Mover|Equipment")
	TArray<TSubclassOf<ULyraInventoryItemDefinition>> InitialInventoryItems;

	/**
	 * Optional runtime-only lookup paths for GameFeature-owned test items. Keeping these as
	 * strings prevents a ProjectContent pawn Blueprint from serializing an illegal hard/soft
	 * reference to content owned by a GameFeature plugin.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "UAF Mover|Equipment")
	TArray<FString> InitialInventoryItemClassPaths;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UAF Mover|Animation", meta = (ClampMin = "0.0"))
	float MovingSpeedThreshold = 3.0f;

	/** Upward speed above which an airborne Pawn counts as still starting its jump. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UAF Mover|Animation", meta = (ClampMin = "0.0"))
	float JumpRisingSpeedThreshold = 10.0f;

	/**
	 * How long the landing pose stays active after touching down.
	 *
	 * Match this to the landing clip's length. Motion matching gets a landing-only database
	 * for the duration of this window, so a window shorter than the clip cuts the clip off,
	 * and a longer one holds the character in a finished pose. MM_Unarmed_Jump_Fall_Land is
	 * 0.4s.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UAF Mover|Animation", meta = (ClampMin = "0.0"))
	float LandingRecoveryTime = 0.4f;

	virtual void OnAbilitySystemInitialized();

	UFUNCTION()
	virtual void OnDeathStarted(AActor* OwningActor);

	UFUNCTION()
	virtual void OnDeathFinished(AActor* OwningActor);

private:
	void UpdateAnimationStateFromMover(float DeltaSeconds);
	void OnAbilitySystemUninitialized();
	void RefreshMovementModeGameplayTag();
	void ClearMovementModeGameplayTags(ULyraAbilitySystemComponent& AbilitySystem);
	void AddInitialInventory();
	void ClearInitialInventory();
	virtual void FellOutOfWorld(const UDamageType& DamageType) override;

	void DisableMovementAndCollision();
	void StartRagdoll();
	void HideEquippedWeapons();
	void DestroyDueToDeath();
	void UninitAndDestroy();

	bool bJumpPressed = false;
	bool bJumpJustPressed = false;
	bool bHasLoggedMovementState = false;
	bool bLastLoggedMovingState = false;
	bool bWasFallingLastUpdate = false;
	float LandingTimeRemaining = 0.0f;
	FGameplayTag ActiveMovementModeTag;
	bool bInitialInventoryGranted = false;

	// NetworkPrediction ticks the sim at a fixed rate and can call ProduceInput several
	// times within a single engine frame to catch up. ConsumeMovementInputVector() is
	// destructive, so a bare call here would hand the accumulated input to only the first
	// sim frame and zero to the rest, making effective move speed framerate-dependent.
	// Controller->GetControlRotation() showed the same multi-call-per-frame symptom
	// (reading back as a transient zero on catch-up calls), which fed a flickering
	// OrientationIntent into Mover and broke aim-facing/animation. Latch both per engine
	// frame and reuse them for any repeat calls.
	bool bHasConsumedInputThisFrame = false;
	uint64 LastInputConsumedFrame = 0;
	FVector LastConsumedMovementInput = FVector::ZeroVector;
	FRotator LastConsumedControlRotation = FRotator::ZeroRotator;
};
