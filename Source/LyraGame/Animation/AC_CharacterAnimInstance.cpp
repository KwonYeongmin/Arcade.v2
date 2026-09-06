// Copyright Epic Games, Inc. All Rights Reserved.

#include "AC_CharacterAnimInstance.h"

#include "Character/Mover/AC_CharacterBase.h"
// AAC_CharacterBase::GetMoverComponent() 는 UCharacterMoverComponent* 를 반환하며 헤더에는
// 전방 선언만 있다. 그것을 UMoverComponent* 로 대입하는 파생->기반 변환에는 완전한 타입이
// 필요하므로 이 include 는 실제로 쓰인다 — 지우지 말 것.
#include "DefaultMovementSet/CharacterMoverComponent.h"
#include "MoverComponent.h"
#include "MoverPoseSearchTrajectoryPredictor.h"
#include "PoseSearch/MotionMatchingAnimNodeLibrary.h"
#include "PoseSearch/PoseSearchDatabase.h"
#include "PoseSearch/PoseSearchResult.h"
#include "PoseSearch/PoseSearchTrajectoryLibrary.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AC_CharacterAnimInstance)

DEFINE_LOG_CATEGORY_STATIC(LogMoverMotionMatching, Log, All);

UAC_CharacterAnimInstance::UAC_CharacterAnimInstance()
{
	// Trajectory.Samples must never be empty -- see the comment on the Trajectory property.
	// The AnimGraph's Update_Trajectory function reads it every frame via GetTrajectorySampleAtTime
	// regardless of whether a real pawn/predictor exists yet (e.g. during a Blueprint compile's
	// CDO/preview pass), and that function indexes an empty array with -1, crashing the whole
	// editor. One dummy sample at construction keeps it non-empty from the moment this object
	// exists, before NativeInitializeAnimation ever runs.
	Trajectory.Samples.Add(FTransformTrajectorySample());
}

void UAC_CharacterAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();

	APawn* PawnOwner = TryGetPawnOwner();

	if (!PawnOwner)
	{
		// 애님 블루프린트 에디터의 프리뷰 메시는 AAnimationEditorPreviewActor 에 붙어 있어
		// Pawn 이 아니다. 컴파일/저장할 때마다 정상적으로 발생하므로 Error 가 아니라 Verbose.
		UE_LOG(LogMoverMotionMatching, Verbose,
			TEXT("[MotionMatching] No pawn owner yet (likely an anim preview). No trajectory will be generated."));
		return;
	}

	AAC_CharacterBase* OwningMoverPawn = Cast<AAC_CharacterBase>(PawnOwner);
	if (!OwningMoverPawn)
	{
		UE_LOG(LogMoverMotionMatching, Error,
			TEXT("[MotionMatching] Owner '%s' is not an AAC_CharacterBase. No trajectory will be generated."),
			*GetNameSafe(PawnOwner));
		return;
	}

	MoverPawn = OwningMoverPawn;

	UMoverComponent* MoverComponent = OwningMoverPawn->GetMoverComponent();
	if (!MoverComponent)
	{
		UE_LOG(LogMoverMotionMatching, Error,
			TEXT("[MotionMatching] '%s' has no MoverComponent. No trajectory will be generated."),
			*GetNameSafe(OwningMoverPawn));
		return;
	}

	TrajectoryPredictor = NewObject<UMoverTrajectoryPredictor>(this);
	TrajectoryPredictor->Setup(MoverComponent);

	UE_LOG(LogMoverMotionMatching, Log,
		TEXT("[MotionMatching] Trajectory predictor ready for '%s'."), *GetNameSafe(OwningMoverPawn));
}

void UAC_CharacterAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	// MoverPawn 은 NativeInitializeAnimation 에서 한 번만 캐시한다. 그런데 소유 폰의 블루프린트를
	// (예: PIE 도중 클래스 디폴트 수정 후) 리컴파일하면 엔진이 그 인스턴스를 통째로 재생성하고,
	// 이 AnimInstance 가 들고 있던 약한 포인터는 조용히 무효화된다 -- 재초기화 이벤트 없이. 그
	// 상태로는 아래 모든 필드가 "폰 없음" 기본값에 영원히 멈춘다. 무효화됐으면 여기서 다시
	// 잡아본다 -- TryGetPawnOwner() 는 매 프레임 다시 조회하므로 자체적으로 복구된다.
	if (!MoverPawn.IsValid())
	{
		MoverPawn = Cast<AAC_CharacterBase>(TryGetPawnOwner());
	}

	// 폰의 애니메이션 상태는 게임 스레드의 Tick 에서 갱신된다. 여기서 복사해두면
	// NativeThreadSafeUpdateAnimation 과 Chooser 가 워커 스레드에서 안전하게 읽을 수 있다.
	const AAC_CharacterBase* Pawn = MoverPawn.Get();
	bIsFalling = Pawn && Pawn->bIsFalling;
	bIsLanding = Pawn && Pawn->bIsLanding;
	bIsMounted = Pawn && Pawn->bIsMounted;
	bIsMoving = Pawn && Pawn->bIsMoving;
	TimeToJumpApex = Pawn ? Pawn->TimeToJumpApex : 0.0f;
	MovementModeName = Pawn ? Pawn->MovementMode : NAME_None;
	bPistolEquipped = Pawn && Pawn->IsPistolEquipped();
	AimYaw = Pawn ? Pawn->AimYaw : 0.0f;
	AimPitch = Pawn ? Pawn->AimPitch : 0.0f;

	// 이동 환경 결정
	if (bIsMounted)
	{
	    CurrentCharacterAnimContext = ECharacterAnimContext::Vehicle;
	}
	else if (MovementModeName == DefaultModeNames::Flying)
	{
	    CurrentCharacterAnimContext = ECharacterAnimContext::Hovering;
	}
	else
	{
	    CurrentCharacterAnimContext = ECharacterAnimContext::Default;
	}

	// 상체 무기 상태 결정
	CurrentUpperBodyPose = bPistolEquipped ? EUpperBodyPose::Pistol:EUpperBodyPose::Unarmed ;

	LocalVelocity = Pawn ? Pawn->LocalVelocity : FVector::ZeroVector;

	// 현재 이동 모드가 없으면 예측이 불가능하다. UMoverComponent::GetPredictedTrajectory 가
	// 바로 이 조건으로 경고하고 빈 샘플을 돌려준다(MoverComponent.cpp:2389).
	const UMoverComponent* Mover = Pawn ? Pawn->GetMoverComponent() : nullptr;
	bCanPredictTrajectory = Mover && Mover->GetMovementMode() != nullptr;
}

void UAC_CharacterAnimInstance::NativeThreadSafeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeThreadSafeUpdateAnimation(DeltaSeconds);

	if (!bCanPredictTrajectory)
	{
		// 탑승 등으로 Mover 가 세워져 있다. 마지막 궤적을 그대로 두면 모션 매칭이
		// 정지 상태를 계속 매칭하므로 포즈가 튀지 않는다.
		return;
	}

	if (!TrajectoryPredictor)
	{
		if (!bHasWarnedMissingPredictor)
		{
			bHasWarnedMissingPredictor = true;
			UE_LOG(LogMoverMotionMatching, Warning,
				TEXT("[MotionMatching] TrajectoryPredictor is null; update is a no-op. Check earlier NativeInitializeAnimation logs."));
		}
		return;
	}

	UPoseSearchTrajectoryLibrary::PoseSearchGenerateTransformTrajectoryWithPredictor(
		TrajectoryPredictor,
		DeltaSeconds,
		TrajectoryState,
		DesiredControllerYawLastUpdate,
		Trajectory,
		HistorySamplingInterval,
		TrajectoryHistoryCount,
		PredictionSamplingInterval,
		TrajectoryPredictionCount);
}

void UAC_CharacterAnimInstance::ReportMotionMatchingState(const FAnimNodeReference& Node)
{
	EAnimNodeReferenceConversionResult ConversionResult = EAnimNodeReferenceConversionResult::Failed;
	const FMotionMatchingAnimNodeReference MotionMatchingNode =
		UMotionMatchingAnimNodeLibrary::ConvertToMotionMatchingNode(Node, ConversionResult);

	FAC_MotionMatchingDebugState NewState;

	if (ConversionResult == EAnimNodeReferenceConversionResult::Succeeded)
	{
		FPoseSearchBlueprintResult Result;
		bool bIsResultValid = false;
		UMotionMatchingAnimNodeLibrary::GetMotionMatchingSearchResult(MotionMatchingNode, Result, bIsResultValid);

		if (bIsResultValid)
		{
			NewState.bIsValid = true;
			NewState.AnimationName = GetNameSafe(Result.SelectedAnim);
			NewState.DatabaseName = GetNameSafe(Result.SelectedDatabase);
			NewState.Time = Result.SelectedTime;
			NewState.SearchCost = Result.SearchCost;
			NewState.bIsContinuingPose = Result.bIsContinuingPoseSearch;
		}
	}

	FScopeLock Lock(&MotionMatchingDebugStateLock);
	MotionMatchingDebugState = MoveTemp(NewState);
}

FAC_MotionMatchingDebugState UAC_CharacterAnimInstance::GetMotionMatchingDebugState() const
{
	FScopeLock Lock(&MotionMatchingDebugStateLock);
	return MotionMatchingDebugState;
}
