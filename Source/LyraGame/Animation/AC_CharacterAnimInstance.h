// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Animation/AnimInstance.h"
#include "Animation/AnimNodeReference.h"
#include "Animation/TrajectoryTypes.h"

#include "AC_CharacterAnimInstance.generated.h"

#define UE_API LYRAGAME_API

class AAC_CharacterBase;
class UMoverTrajectoryPredictor;

  UENUM(BlueprintType)
  enum class ECharacterAnimContext : uint8
  {
      Default,
      Vehicle,
      Hovering
  };

  UENUM(BlueprintType)
  enum class EUpperBodyPose : uint8
  {
      Unarmed,
      Pistol
  };

/**
 * UAC_CharacterAnimInstance
 *
 * Mover 폰용 모션 매칭 AnimInstance.
 *
 * Mover 컴포넌트를 아는 유일한 애니메이션 코드다. 매 프레임 Mover 상태에서 궤적을 만들어
 * Trajectory 로 노출하고, AnimGraph 의 Pose History 노드가 그것을 읽어 Motion Matching 에
 * 넘긴다. 궤적 생성 자체는 엔진의 UMoverTrajectoryPredictor 가 담당한다.
 *
 * ULyraAnimInstance 를 상속하지 않는다. 그쪽은 GameplayTagPropertyMap 과 ALyraCharacter
 * 캐스팅을 들고 오는데, Mover 폰은 ALyraCharacter 가 아니며 1단계에 태그 바인딩이 필요없다.
 *
 * 스레딩 제약: NativeThreadSafeUpdateAnimation 은 애니메이션 워커 스레드에서
 * UMoverComponent::GetPredictedTrajectory 를 호출한다. 이 함수는 내부적으로
 * RollbackBlackboard->BeginPredictionFrame() 에서
 * check(!bIsPredictionInProgress || InPredictionThreadId == FPlatformTLS::GetCurrentThreadId())
 * 를 검사하므로, 같은 Mover 컴포넌트에 대해 게임 스레드에서 GetPredictedTrajectory 를
 * 또 호출하면(예: Mover 궤적 디버그 드로잉이 켜졌을 때 UMoverDebugComponent::DrawTrajectory 가
 * 바로 이 짓을 한다) 어서션이 터지고 Development 빌드에서 크래시한다. 이 폰의 Mover 컴포넌트에
 * 대해서는 이 AnimInstance 외에 어떤 코드도 GetPredictedTrajectory 를 호출해서는 안 된다.
 * 이는 엔진 쪽의 날카로운 제약이며 우회 코드로 해결할 대상이 아니다.
 */
/**
 * 모션 매칭이 이번 프레임에 무엇을 골랐는지. 디버그 표시 전용이다.
 *
 * 애니메이션 워커 스레드가 쓰고 게임 스레드가 읽으므로 반드시 잠금 아래에서만 접근한다.
 * FString 은 참조 카운트를 쓰기 때문에 잠금 없이 스레드를 넘나들면 실제로 깨진다.
 */
struct FAC_MotionMatchingDebugState
{
	FString AnimationName;
	FString DatabaseName;
	float Time = 0.0f;
	float SearchCost = 0.0f;
	bool bIsContinuingPose = false;
	bool bIsValid = false;
};

UCLASS(MinimalAPI, Blueprintable)
class UAC_CharacterAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:

	UE_API UAC_CharacterAnimInstance();

	/**
	 * AnimGraph 의 Pose History 노드가 읽는 궤적. 스켈레탈 메시 컴포넌트의 월드 스페이스다.
	 *
	 * Samples 가 절대 비어있으면 안 된다 -- AnimGraph 의 Update_Trajectory 함수가 매 프레임
	 * (폰이 없어 실제 궤적 생성이 스킵되는 경우도 포함해) GetTrajectorySampleAtTime 으로 이걸
	 * 읽는데, 그 함수는 빈 배열에서 인덱스 -1 로 접근해 "Array index out of bounds" 어서션으로
	 * 에디터를 통째로 크래시시킨다 (Blueprint 컴파일 시의 프리뷰/CDO 컨텍스트에서 실제로 발생
	 * 확인됨 - Update_Trajectory -> Update_Logic -> BlueprintThreadSafeUpdateAnimation). 기본
	 * 생성자에서 더미 샘플 하나를 채워 이 배열이 만들어지는 시점부터 항상 최소 1개를 유지한다.
	 */
	UPROPERTY(BlueprintReadWrite, Transient, Category = "Motion Matching")
	FTransformTrajectory Trajectory;

	/**
	 * 낙하 중인지. Chooser 가 공중 데이터베이스를 고르는 조건이다.
	 *
	 * 폰의 상태는 게임 스레드에서 갱신되므로 NativeUpdateAnimation 에서 복사한다. 워커 스레드가
	 * 폰 프로퍼티를 직접 읽어서는 안 된다.
	 */
	UPROPERTY(BlueprintReadOnly, Transient, Category = "Motion Matching")
	bool bIsFalling = false;

	UPROPERTY(BlueprintReadOnly, Transient, Category = "Animation Context")
	ECharacterAnimContext CurrentCharacterAnimContext = ECharacterAnimContext::Default;
	    

	UPROPERTY(BlueprintReadOnly, Transient, Category = "Animation Context")
	EUpperBodyPose CurrentUpperBodyPose =  EUpperBodyPose::Unarmed;
     


	/**
	 * 착지 회복 창이 열려 있는지. Chooser 가 착지 전용 데이터베이스를 고르는 조건이다.
	 *
	 * 착지를 공중과 같은 데이터베이스에 두면 재생 도중 낙하 클립으로 되돌아간다. 이 클립들의
	 * 인덱싱된 궤적이 실제와 뒤집혀 있기 때문이다 — Fall_Loop 은 제자리로 인덱싱되어 있어
	 * "땅에 서 있는" 쿼리에 가장 잘 맞고, Fall_Land 는 크게 하강하는 것으로 인덱싱되어 있어
	 * 정작 착지 순간에 가장 안 맞는다. 클립 하나짜리 데이터베이스로 분리하면 되돌아갈 곳이
	 * 없어져 이 문제를 우회한다.
	 *
	 * 창의 길이는 AAC_CharacterBase::LandingRecoveryTime 이 정하며, 착지 클립 길이와
	 * 맞춰야 클립이 잘리지 않는다.
	 */
	UPROPERTY(BlueprintReadOnly, Transient, Category = "Motion Matching")
	bool bIsLanding = false;

	/** 탈것 탑승 중인지. 애니메이션 상태머신의 Mounting/Riding/Dismounting 진입 조건이다. */
	UPROPERTY(BlueprintReadOnly, Transient, Category = "Motion Matching")
	bool bIsMounted = false;

	/**
	 * AAC_CharacterBase::AimYaw/AimPitch 를 그대로 미러링한다 (컨트롤러 조준 각도, 몸 정면
	 * 기준, ±90도로 클램프됨). AimOffset 레이어의 BS_Neutral_AO_Stand_NoSmoothing
	 * 블렌드스페이스가 이 값을 X/Y 로 받아야 정지 중에 상체가 조준 방향을 따라간다 —
	 * 이 값들이 없으면 블렌드스페이스가 계속 (0,0) 중립 포즈에 머문다.
	 */
	UPROPERTY(BlueprintReadOnly, Transient, Category = "Motion Matching")
	float AimYaw = 0.0f;

	UPROPERTY(BlueprintReadOnly, Transient, Category = "Motion Matching")
	float AimPitch = 0.0f;

	/**
	 * 지상에서 실제로 이동 중인지 (AAC_CharacterBase::MovingSpeedThreshold 기준).
	 * 정지 시 권총 아이들 전신 포즈로 교체하는 조건으로 쓴다 — 이동 중 상체 레이어링과
	 * 달리 아이들은 로코모션 클립 위에 얹는 방식이 아니라 전신을 통째로 바꾼다.
	 */
	UPROPERTY(BlueprintReadOnly, Transient, Category = "Motion Matching")
	bool bIsMoving = false;

	/** 점프 상태머신의 Apex 진입 조건. AAC_CharacterBase::TimeToJumpApex 를 그대로 미러링한다. */
	UPROPERTY(BlueprintReadOnly, Transient, Category = "Motion Matching")
	float TimeToJumpApex = 0.0f;

	/**
	 * 현재 이동 모드 이름. AAC_CharacterBase::MovementMode 를 그대로 미러링한다.
	 * AnimGraph 의 호버링 레이어가 MovementModeName == "Flying" 으로 진입을 판정한다.
	 * 폰 상태는 게임 스레드에서 갱신되므로 NativeUpdateAnimation 에서 복사한다.
	 */
	UPROPERTY(BlueprintReadOnly, Transient, Category = "Motion Matching")
	FName MovementModeName = NAME_None;

	/**
	 * 권총(사거리 무기)을 장착 중인지. AAC_CharacterBase::IsPistolEquipped() 를 미러링한다.
	 * 상체 권총 레이어의 블렌드 웨이트를 이 값으로 보간한다. 탑승 중이면 폰이 false 를 돌려준다.
	 */
	UPROPERTY(BlueprintReadOnly, Transient, Category = "Motion Matching")
	bool bPistolEquipped = false;

	/**
	 * Mover 속도를 폰 로컬 스페이스로 변환한 값 (X=전후, Y=좌우, Z=상하).
	 * AAC_CharacterBase::LocalVelocity 를 미러링한다. 호버링 BlendSpace 의 축 입력으로 쓴다 —
	 * 워커 스레드가 폰 프로퍼티를 직접 읽지 않도록 게임 스레드에서 복사한다.
	 */
	UPROPERTY(BlueprintReadOnly, Transient, Category = "Motion Matching")
	FVector LocalVelocity = FVector::ZeroVector;

	/**
	 * 과거 샘플 간격(초). PSS_MoverLocomotion 스키마가 과거 -0.3s 까지 샘플링하므로,
	 * HistorySamplingInterval * (TrajectoryHistoryCount - 1) 는 최소 0.3s 를 유지해야 한다.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Motion Matching", meta = (ClampMin = "0.001"))
	float HistorySamplingInterval = 0.04f;

	/**
	 * 과거 샘플 개수. PSS_MoverLocomotion 스키마가 과거 -0.3s 까지 샘플링하므로,
	 * HistorySamplingInterval * (TrajectoryHistoryCount - 1) 는 최소 0.3s 를 유지해야 한다.
	 * 클램프 범위 안에서라도 이 값을 줄이면 스키마의 가장 오래된 샘플이 굶주려 매칭 품질이
	 * 조용히 저하된다.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Motion Matching", meta = (ClampMin = "2"))
	int32 TrajectoryHistoryCount = 10;

	/**
	 * 미래 예측 샘플 간격(초). PSS_MoverLocomotion 스키마가 미래 +0.6s 까지만 샘플링하므로,
	 * PredictionSamplingInterval * TrajectoryPredictionCount 는 최소 0.6s 를 유지해야 한다.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Motion Matching", meta = (ClampMin = "0.001"))
	float PredictionSamplingInterval = 0.2f;

	/**
	 * 미래 예측 샘플 개수. PSS_MoverLocomotion 스키마가 미래 +0.6s 까지만 샘플링하므로,
	 * PredictionSamplingInterval * TrajectoryPredictionCount 는 최소 0.6s 를 유지해야 한다.
	 * 기본값 4 는 0.2s 간격으로 0.8s 지평선을 만든다 — 스키마가 읽는 +0.6s 보다 넉넉하게
	 * 유지하되, 예전 기본값 8(1.6s)처럼 Mover 시뮬레이션을 불필요하게 넓게 돌리지 않는다.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Motion Matching", meta = (ClampMin = "1"))
	int32 TrajectoryPredictionCount = 4;

	/**
	 * 모션 매칭 노드의 이번 프레임 검색 결과를 디버그용으로 기록한다.
	 *
	 * AnimGraph 의 Motion Matching 노드에 붙인 On Update 함수에서 Node 핀을 그대로 넘겨
	 * 호출한다. 데이터베이스 선택에는 관여하지 않으므로 Set Database to Search 앞뒤 어디에
	 * 놓아도 되지만, 뒤에 놓아야 이번 프레임에 적용된 데이터베이스가 찍힌다.
	 */
	UFUNCTION(BlueprintCallable, Category = "Motion Matching|Debug", meta = (BlueprintThreadSafe))
	UE_API void ReportMotionMatchingState(const FAnimNodeReference& Node);

	/** 게임 스레드에서 디버그 상태를 복사해 간다. GAS 프로파일러가 쓴다. */
	UE_API FAC_MotionMatchingDebugState GetMotionMatchingDebugState() const;

protected:

	UE_API virtual void NativeInitializeAnimation() override;
	UE_API virtual void NativeUpdateAnimation(float DeltaSeconds) override;
	UE_API virtual void NativeThreadSafeUpdateAnimation(float DeltaSeconds) override;

private:

	/** NativeUpdateAnimation 이 매 프레임 상태를 읽어오는 소유 폰. 게임 스레드에서만 접근한다. */
	TWeakObjectPtr<AAC_CharacterBase> MoverPawn;

	/** 게임 스레드에서만 생성한다. NewObject 는 워커 스레드에서 호출할 수 없다. */
	UPROPERTY(Transient)
	TObjectPtr<UMoverTrajectoryPredictor> TrajectoryPredictor;

	/**
	 * 라이브러리의 InOutTrajectory. 프레임 간 히스토리가 여기에 누적되므로 반드시 멤버로 유지한다.
	 * 지역 변수로 두면 과거 구간이 매 프레임 비워져 모션 매칭이 정지 상태로 오인한다.
	 */
	FTransformTrajectory TrajectoryState;

	/** 라이브러리의 InOutDesiredControllerYawLastUpdate. 시그니처상 필수 인자다. */
	float DesiredControllerYawLastUpdate = 0.0f;

	/** TrajectoryPredictor 가 없어 업데이트가 no-op 될 때 한 번만 경고하기 위한 가드. */
	bool bHasWarnedMissingPredictor = false;

	/**
	 * Mover 가 궤적을 예측할 수 있는 상태인지. 게임 스레드에서 갱신한다.
	 *
	 * 탈 것에 탑승하면 Mover 를 "Null" 모드로 세우는데, 그 상태에서 GetPredictedTrajectory 를
	 * 부르면 엔진이 매 프레임 경고를 뿜고 기본값 샘플을 돌려준다. 그 궤적으로 모션 매칭을
	 * 돌리면 매 프레임 다른 포즈가 뽑혀 캐릭터가 떨린다.
	 */
	bool bCanPredictTrajectory = false;

	FAC_MotionMatchingDebugState MotionMatchingDebugState;

	mutable FCriticalSection MotionMatchingDebugStateLock;
};

#undef UE_API
