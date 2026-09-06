// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Character/LyraPawn.h"
#include "Interaction/IInteractableTarget.h"
#include "Interaction/InteractionOption.h"

#include "AC_MotorcyclePawn.generated.h"

class UCapsuleComponent;
class USkeletalMeshComponent;
class UStaticMeshComponent;
class UAC_MotorcycleMovement;
class UInputMappingContext;
class ULyraCameraComponent;
class ULyraHeroComponent;
class ULyraPawnExtensionComponent;
struct FInputActionValue;
struct FInteractionQuery;

/**
 * AAC_MotorcyclePawn
 *
 * 오토바이 탈 것. 캐릭터와 별개 Pawn이며 Controller가 Possess해 제어한다.
 * ASC는 ALyraPlayerState에 있으므로 Possess해도 어빌리티가 유지된다.
 *
 * 기획서: docs/plan/vehicle_motorcycle_기획서.md
 */
UCLASS(Blueprintable)
class AAC_MotorcyclePawn : public ALyraPawn, public IInteractableTarget
{
    GENERATED_BODY()

public:
    AAC_MotorcyclePawn(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

    //~IInteractableTarget interface
    virtual void GatherInteractionOptions(const FInteractionQuery& InteractQuery, FInteractionOptionBuilder& OptionBuilder) override;
    //~End of IInteractableTarget interface

    USkeletalMeshComponent* GetBikeMesh() const { return BikeMesh; }

    /** 라이더를 RiderSocket 에 부착하고 이동/콜리전을 끈다. */
    void AttachRider(APawn* Rider);

    /** 라이더를 떼어내고 이동/콜리전을 복구한다. 직전 라이더를 반환한다 (없으면 nullptr). */
    APawn* DetachRider();

    APawn* GetRider() const { return CurrentRider; }

    UAC_MotorcycleMovement* GetMotorcycleMovement() const { return MotorcycleMovement; }

    virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

    /**
     * ALyraPawn::PossessedBy 는 팀 델리게이트만 처리하고 PawnExtensionComponent 에는
     * 알리지 않는다. AAC_CharacterBase 는 이 셋을 직접 오버라이드해서 알림을 추가하는데,
     * 오토바이는 ALyraPawn 을 바로 상속해서 그게 없었다 — 탑승해도 InitState 체인이
     * DataAvailable 을 못 넘어서고, 그 안에서 하는 ASC 아바타 전환과 카메라 모드 델리게이트
     * 바인딩이 전부 실행되지 않았다. 그 결과 탑승해도 라이더의 상호작용 어빌리티가 계속
     * 돌고, 카메라는 어떤 모드도 못 받아 원점을 보여줬다.
     */
    virtual void PossessedBy(AController* NewController) override;
    virtual void UnPossessed() override;
    virtual void OnRep_Controller() override;
    virtual void OnRep_PlayerState() override;

    /** Keep the bike as ViewTarget, but frame the third-person camera around its rider. */
    virtual FVector GetPawnViewLocation() const override;

protected:
    virtual void BeginPlay() override;

    /**
     * 탑승 중 적용할 입력 매핑 컨텍스트. BP_Motorcycle 에서 지정한다.
     *
     * 예전에는 경로 문자열로 LoadObject 했는데, 에셋을 플러그인으로 옮기자 조용히 실패했다.
     * 더 나쁜 것은 옛 경로에 같은 이름의 사본이 남아 있던 경우다 — 로드는 성공하지만 그
     * IMC 가 가리키는 InputAction 이 InputConfig 가 듣는 것과 다른 객체라, 키를 눌러도
     * 아무 일도 일어나지 않고 에러도 나지 않았다. 참조로 두면 에셋을 옮겨도 따라온다.
     */
    UPROPERTY(EditDefaultsOnly, Category = "Motorcycle|Input")
    TSoftObjectPtr<UInputMappingContext> RidingInputMappingContext;

    /** 캐릭터 IMC(0) 보다 높아야 W/A/S/D 를 오토바이가 가져간다. */
    UPROPERTY(EditDefaultsOnly, Category = "Motorcycle|Input")
    int32 RidingInputMappingPriority = 10;

    /**
     * 탑승 중 적용한 IMC. 하차 시 반드시 제거해야 한다.
     * 남겨두면 IA_Throttle / IA_Steer 가 W/A/S/D 를 계속 소비해
     * 하차 후 캐릭터가 움직이지 못한다.
     */
    UPROPERTY(Transient)
    TObjectPtr<UInputMappingContext> AppliedIMC;

    /** BP에서 Text="오토바이 탑승", InteractionAbilityToGrant=GA_Mount 를 설정한다. */
    UPROPERTY(EditAnywhere, Category = "Motorcycle|Interaction")
    FInteractionOption Option;

    /** 기획서 §6: rider.socketId */
    UPROPERTY(EditDefaultsOnly, Category = "Motorcycle|Rider")
    FName RiderSocketName = FName(TEXT("RiderSocket"));

    UPROPERTY(Transient)
    TObjectPtr<APawn> CurrentRider;

    UPROPERTY(VisibleAnywhere, Category = "Motorcycle")
    TObjectPtr<UAC_MotorcycleMovement> MotorcycleMovement;

    /** 기획서 §3-2: dismount.maxSpeed — 이 속도 이하에서만 하차 허용 (cm/s) */
    UPROPERTY(EditDefaultsOnly, Category = "Motorcycle|Dismount", meta = (ClampMin = "0.0"))
    float DismountMaxSpeed = 200.0f;

    /** 기획서 §3-2: dismount.exitOffset — 하차 시 측면 이동 거리 (cm) */
    UPROPERTY(EditDefaultsOnly, Category = "Motorcycle|Dismount", meta = (ClampMin = "1.0"))
    float DismountExitOffset = 100.0f;

    /** 하차 지점의 지면을 아래로 얼마나 찾을지 (cm) */
    UPROPERTY(EditDefaultsOnly, Category = "Motorcycle|Dismount", meta = (ClampMin = "1.0"))
    float GroundSearchDistance = 300.0f;

private:
    void Input_Throttle(const FInputActionValue& Value);
    void Input_Steer(const FInputActionValue& Value);
    void Input_Dismount(const FInputActionValue& Value);

    /** 하차 지점을 찾는다. 양쪽 다 막혀 있으면 false. */
    bool FindDismountLocation(FVector& OutLocation) const;

    UPROPERTY(VisibleAnywhere, Category = "Motorcycle")
    TObjectPtr<UCapsuleComponent> CollisionRoot;

    UPROPERTY(VisibleAnywhere, Category = "Motorcycle")
    TObjectPtr<USkeletalMeshComponent> BikeMesh;

    /**
     * 오토바이 스켈레탈 메시가 아직 없어서 위치를 눈으로 볼 수 없다.
     * 임시 큐브로 대체한다 — BikeMesh 에 실제 메시가 지정되면 자동으로 숨는다.
     * 기획서 §7 의 "선행 에셋 의존성" 항목.
     */
    UPROPERTY(VisibleAnywhere, Category = "Motorcycle|Proxy")
    TObjectPtr<UStaticMeshComponent> ProxyMesh;

    UPROPERTY(VisibleAnywhere, Category = "Motorcycle")
    TObjectPtr<ULyraPawnExtensionComponent> PawnExtComponent;

    UPROPERTY(VisibleAnywhere, Category = "Motorcycle")
    TObjectPtr<ULyraHeroComponent> HeroComponent;

    UPROPERTY(VisibleAnywhere, Category = "Motorcycle")
    TObjectPtr<ULyraCameraComponent> CameraComponent;
};
