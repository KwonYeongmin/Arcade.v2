// Copyright Epic Games, Inc. All Rights Reserved.

#include "Character/Mover/AC_ShooterPawn.h"

#include "Character/LyraPawnData.h"
#include "Character/LyraPawnExtensionComponent.h"
#include "DefaultMovementSet/CharacterMoverComponent.h"
#include "Input/LyraInputComponent.h"
#include "LyraGameplayTags.h"
#include "MoverComponent.h"
#include "MoverDataModelTypes.h"
#include "MoverSimulationTypes.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AC_ShooterPawn)

DEFINE_LOG_CATEGORY_STATIC(LogArcadeFlight, Log, All);

AAC_ShooterPawn::AAC_ShooterPawn(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = true;
}

void AAC_ShooterPawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	ULyraInputComponent* LIC = Cast<ULyraInputComponent>(PlayerInputComponent);
	if (!LIC)
	{
		return;
	}

	const ULyraPawnExtensionComponent* PawnExtComp = ULyraPawnExtensionComponent::FindPawnExtensionComponent(this);
	const ULyraPawnData* PawnData = PawnExtComp ? PawnExtComp->GetPawnData<ULyraPawnData>() : nullptr;
	const ULyraInputConfig* InputConfig = PawnData ? PawnData->InputConfig : nullptr;

	if (!InputConfig)
	{
		UE_LOG(LogArcadeFlight, Warning,
			TEXT("[Flight] '%s' has no InputConfig; flight input will not be bound."), *GetNameSafe(this));
		return;
	}

	LIC->BindNativeAction(InputConfig, LyraGameplayTags::InputTag_Flight_Ascend,
		ETriggerEvent::Triggered, this, &ThisClass::Input_AscendTriggered, /*bLogIfNotFound=*/ true);
	// 눌린 상태를 직접 추적한다. Triggered 만으로는 놓는 순간을 알 수 없어 상승이 멈추지 않는다.
	LIC->BindNativeAction(InputConfig, LyraGameplayTags::InputTag_Flight_Ascend,
		ETriggerEvent::Completed, this, &ThisClass::Input_AscendReleased, /*bLogIfNotFound=*/ false);
	LIC->BindNativeAction(InputConfig, LyraGameplayTags::InputTag_Flight_Ascend,
		ETriggerEvent::Canceled, this, &ThisClass::Input_AscendReleased, /*bLogIfNotFound=*/ false);
	LIC->BindNativeAction(InputConfig, LyraGameplayTags::InputTag_Flight_Descend,
		ETriggerEvent::Started, this, &ThisClass::Input_DescendActive, /*bLogIfNotFound=*/ true);
}

void AAC_ShooterPawn::Input_AscendTriggered(const FInputActionValue& /*Value*/)
{
	const UCharacterMoverComponent* Mover = GetMoverComponent();
	if (!Mover)
	{
		return;
	}

	// 누르고 있는 동안 AddAscendToMoveInput 이 이동 의도에 위쪽 성분을 더한다. 여기서
	// 직접 힘이나 트랜스폼을 건드리면 다음 Mover 틱이 덮어쓰고, 멀티에서는 서버가 되돌린다.
	bAscendHeld = true;

	if (Mover->IsFlying())
	{
		return;
	}

	// 상승은 중력을 건드리지 않는다. 하강에서 낮춰둔 중력이 남아 있으면 여기서 정리한다.
	ClearDescendGravity();
	PendingMovementMode = DefaultModeNames::Flying;
	UE_LOG(LogArcadeFlight, Log, TEXT("[Flight] 비행 시작"));
}


void AAC_ShooterPawn::Input_AscendReleased(const FInputActionValue& /*Value*/)
{
	bAscendHeld = false;
}

void AAC_ShooterPawn::Input_DescendActive(const FInputActionValue& /*Value*/)
{
	const UCharacterMoverComponent* Mover = GetMoverComponent();
	if (!Mover || !Mover->IsFlying())
	{
		// 지상에서의 같은 키는 Lyra 의 crouch 가 가져간다.
		return;
	}

	// 비행 모드는 지면에 닿아도 착지로 넘어가지 않고 미끄러지기만 한다. Falling 으로 넘겨야
	// Mover 가 접지를 감지해 Walking 으로 되돌린다. 중력을 낮춰 천천히 내려오게 한다.
	ApplyDescendGravity();
	PendingMovementMode = DefaultModeNames::Falling;
}

void AAC_ShooterPawn::ProduceInput_Implementation(int32 SimTimeMs, FMoverInputCmdContext& InputCmd)
{
	Super::ProduceInput_Implementation(SimTimeMs, InputCmd);

	AddAscendToMoveInput(InputCmd);

	if (PendingMovementMode.IsNone())
	{
		return;
	}

	FCharacterDefaultInputs& Inputs =
		InputCmd.InputCollection.FindOrAddMutableDataByType<FCharacterDefaultInputs>();
	Inputs.SuggestedMovementMode = PendingMovementMode;

	PendingMovementMode = NAME_None;
}

void AAC_ShooterPawn::AddAscendToMoveInput(FMoverInputCmdContext& InputCmd) const
{
	const UCharacterMoverComponent* Mover = GetMoverComponent();
	if (!bAscendHeld || !Mover || !Mover->IsFlying())
	{
		return;
	}

	FCharacterDefaultInputs& Inputs =
		InputCmd.InputCollection.FindOrAddMutableDataByType<FCharacterDefaultInputs>();

	// UFlyingMode 는 이동 의도를 월드 공간 그대로 받아 평면 제약만 적용한다
	// (FlyingMode.cpp:37). 즉 의도의 위쪽 성분이 그대로 수직 비행이 된다.
	const FVector Ascend = Mover->GetUpDirection() * FlightAscendInputScale;
	const FVector Combined = (Inputs.GetMoveInput() + Ascend).GetClampedToMaxSize(1.0f);

	Inputs.SetMoveInput(EMoveInputType::DirectionalIntent, Combined);

	// OrientationIntent 는 건드리지 않는다. 기반 클래스가 수평 성분만으로 설정해 두었고,
	// 여기에 위쪽 성분을 섞으면 캐릭터가 하늘을 보고 기운다.
}

void AAC_ShooterPawn::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// CMC 판은 ACharacter::Landed() 훅으로 중력을 되돌렸다. Mover 폰에는 그 훅이 없으므로
	// 접지 여부로 판정한다. bIsFalling 은 AAC_CharacterBase 가 매 틱 갱신한다.
	if (bDescendGravityApplied && !bIsFalling)
	{
		ClearDescendGravity();
	}
}

void AAC_ShooterPawn::ApplyDescendGravity()
{
	UMoverComponent* Mover = GetMoverComponent();
	if (!Mover || bDescendGravityApplied)
	{
		return;
	}

	// 현재 실효 중력에 배율을 곱한다. 월드 중력이나 액터별 오버라이드를 그대로 존중한다.
	const FVector ScaledGravity = Mover->GetGravityAcceleration() * FlightDescendGravityScale;
	Mover->SetGravityOverride(true, ScaledGravity);
	bDescendGravityApplied = true;
}

void AAC_ShooterPawn::ClearDescendGravity()
{
	if (!bDescendGravityApplied)
	{
		return;
	}

	if (UMoverComponent* Mover = GetMoverComponent())
	{
		Mover->SetGravityOverride(false);
	}
	bDescendGravityApplied = false;
}
