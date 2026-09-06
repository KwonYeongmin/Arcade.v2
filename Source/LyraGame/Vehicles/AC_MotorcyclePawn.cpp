// Copyright Epic Games, Inc. All Rights Reserved.

#include "Vehicles/AC_MotorcyclePawn.h"

#include "Character/Mover/AC_CharacterBase.h"
// GetMoverComponent() 는 UCharacterMoverComponent* 를 반환한다. UMoverComponent* 로의
// 파생->기반 변환에 완전한 타입이 필요하므로 이 include 는 실제로 쓰인다.
#include "DefaultMovementSet/CharacterMoverComponent.h"
#include "MoverComponent.h"
#include "MovementMode.h"
#include "MoverSimulationTypes.h"

#include "AbilitySystem/LyraAbilitySystemComponent.h"
#include "Camera/LyraCameraComponent.h"
#include "Character/LyraCharacter.h"
#include "Character/LyraHeroComponent.h"
#include "Character/LyraPawnExtensionComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "Character/LyraPawnData.h"
#include "Components/GameFrameworkComponentManager.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "Input/LyraInputComponent.h"
#include "Input/LyraInputConfig.h"
#include "InputActionValue.h"
#include "InputMappingContext.h"
#include "LyraGameplayTags.h"
#include "Player/LyraPlayerState.h"
#include "TimerManager.h"
#include "Vehicles/AC_MotorcycleMovement.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AC_MotorcyclePawn)

/**
 * Lyra_TraceChannel_Interaction 을 Overlap 하는 기존 프로필.
 * Config/DefaultEngine.ini 에 정의되어 있다 — 새로 만들지 말 것.
 */
static FName NAME_MotorcycleInteractionProfile(TEXT("Interactable_BlockDynamic"));

AAC_MotorcyclePawn::AAC_MotorcyclePawn(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = true;

    CollisionRoot = CreateDefaultSubobject<UCapsuleComponent>(TEXT("CollisionRoot"));
    CollisionRoot->InitCapsuleSize(45.0f, 60.0f);
    CollisionRoot->SetCollisionProfileName(NAME_MotorcycleInteractionProfile);
    // 라이더가 오토바이에 Attach 되면 캐릭터 위치가 이 캡슐과 겹친다.
    // ThirdPerson 카메라의 침투 방지 트레이스(ECC_Camera, LyraCameraMode_ThirdPerson.cpp)가
    // 여길 막힘으로 잡으면 ALyraPlayerController::OnCameraPenetratingTarget() 이 호출되어
    // 뷰 타깃(라이더)의 메시가 그 프레임에 숨겨진다. 상호작용 감지(Interactable_BlockDynamic
    // 프로필)는 유지하되 카메라 채널만 무시하도록 개별 오버라이드한다.
    CollisionRoot->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
    RootComponent = CollisionRoot;

    BikeMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("BikeMesh"));
    BikeMesh->SetupAttachment(CollisionRoot);
    BikeMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    // 오토바이 메시 확보 전까지 위치를 볼 수 있게 하는 임시 큐브.
    ProxyMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ProxyMesh"));
    ProxyMesh->SetupAttachment(CollisionRoot);
    ProxyMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    ProxyMesh->SetRelativeScale3D(FVector(2.0f, 0.8f, 1.0f));   // 오토바이 비율 흉내
    {
        static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
        if (CubeFinder.Succeeded())
        {
            ProxyMesh->SetStaticMesh(CubeFinder.Object);
        }
    }

    PawnExtComponent = CreateDefaultSubobject<ULyraPawnExtensionComponent>(TEXT("PawnExtensionComponent"));
    HeroComponent = CreateDefaultSubobject<ULyraHeroComponent>(TEXT("HeroComponent"));

    CameraComponent = CreateDefaultSubobject<ULyraCameraComponent>(TEXT("CameraComponent"));
    CameraComponent->SetupAttachment(CollisionRoot);
    CameraComponent->SetRelativeLocation(FVector(-400.0f, 0.0f, 120.0f));

    MotorcycleMovement = CreateDefaultSubobject<UAC_MotorcycleMovement>(TEXT("MotorcycleMovement"));
    MotorcycleMovement->UpdatedComponent = CollisionRoot;

    bUseControllerRotationPitch = false;
    bUseControllerRotationYaw = false;
    bUseControllerRotationRoll = false;
}

void AAC_MotorcyclePawn::BeginPlay()
{
    Super::BeginPlay();

    // 실제 오토바이 메시가 지정되면 프록시 큐브는 필요 없다.
    if (ProxyMesh && BikeMesh && BikeMesh->GetSkeletalMeshAsset() != nullptr)
    {
        ProxyMesh->SetVisibility(false);
    }
}

void AAC_MotorcyclePawn::PossessedBy(AController* NewController)
{
    Super::PossessedBy(NewController);

    if (PawnExtComponent)
    {
        PawnExtComponent->HandleControllerChanged();
    }
}

void AAC_MotorcyclePawn::UnPossessed()
{
    // IMC 를 남겨두면 IA_Throttle / IA_Steer 가 W/A/S/D 를 계속 소비해
    // 하차한 캐릭터가 움직이지 못한다.
    if (AppliedIMC)
    {
        if (APlayerController* PC = Cast<APlayerController>(GetController()))
        {
            if (UEnhancedInputLocalPlayerSubsystem* Sub =
                    ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
            {
                Sub->RemoveMappingContext(AppliedIMC);
                // UE_LOG(LogTemp, Warning, TEXT("[Vehicle] IMC_Motorcycle 제거"));
            }
        }
        AppliedIMC = nullptr;
    }

    Super::UnPossessed();

    if (PawnExtComponent)
    {
        PawnExtComponent->HandleControllerChanged();
    }
}

void AAC_MotorcyclePawn::OnRep_Controller()
{
    Super::OnRep_Controller();

    if (PawnExtComponent)
    {
        PawnExtComponent->HandleControllerChanged();
    }
}

void AAC_MotorcyclePawn::OnRep_PlayerState()
{
    Super::OnRep_PlayerState();

    if (PawnExtComponent)
    {
        PawnExtComponent->HandlePlayerStateReplicated();
    }
}

FVector AAC_MotorcyclePawn::GetPawnViewLocation() const
{
    if (const APawn* Rider = CurrentRider)
    {
        return Rider->GetPawnViewLocation();
    }

    return Super::GetPawnViewLocation();
}

void AAC_MotorcyclePawn::GatherInteractionOptions(const FInteractionQuery& InteractQuery, FInteractionOptionBuilder& OptionBuilder)
{
    OptionBuilder.AddInteractionOption(Option);
}

void AAC_MotorcyclePawn::AttachRider(APawn* Rider)
{
    if (!Rider)
    {
        return;
    }

    CurrentRider = Rider;

    Rider->SetActorEnableCollision(false);

    if (ACharacter* RiderChar = Cast<ACharacter>(Rider))
    {
        if (UCharacterMovementComponent* CMC = RiderChar->GetCharacterMovement())
        {
            CMC->StopMovementImmediately();
            CMC->SetMovementMode(MOVE_None);
        }
    }
    else if (AAC_CharacterBase* MoverRider = Cast<AAC_CharacterBase>(Rider))
    {
        // Mover 폰에는 MOVE_None 이 없다. "Null" 은 아무것도 하지 않는 모드로, Mover 가
        // 항상 등록해 둔다(MovementModeStateMachine.cpp:825). 이걸 걸지 않으면 라이더가
        // 오토바이에 붙은 채로도 계속 시뮬레이션되어 부착 위치와 싸운다.
        //
        // 여기서는 SuggestedMovementMode 가 아니라 QueueNextMode 를 쓴다. 탑승하면 라이더는
        // 컨트롤러를 잃어 ProduceInput 이 더 이상 호출되지 않으므로 입력에 실어 보낼 방법이
        // 없다. 탑승 어빌리티가 ServerOnly 라 이 호출은 서버에서 일어나고, 모드는 Mover 의
        // 동기화 상태를 통해 전파된다.
        if (UMoverComponent* Mover = MoverRider->GetMoverComponent())
        {
            Mover->QueueNextMode(UNullMovementMode::NullModeName);
        }

        // 애니메이션 쪽 탑승/주행/하차 상태머신이 이 값으로 진입 조건을 판단한다.
        MoverRider->bIsMounted = true;
    }

    const FAttachmentTransformRules Rules(EAttachmentRule::SnapToTarget, EAttachmentRule::SnapToTarget, EAttachmentRule::KeepWorld, false);

    // 오토바이 스켈레탈 메시가 아직 없으면 소켓 조회가 경고를 뿜는다.
    // 메시가 생기기 전까지는 루트에 붙이고, 소켓은 메시가 있을 때만 쓴다.
    if (BikeMesh && BikeMesh->GetSkeletalMeshAsset() != nullptr)
    {
        Rider->AttachToComponent(BikeMesh, Rules, RiderSocketName);
        UE_LOG(LogTemp, Warning, TEXT("[Vehicle] 라이더 부착 — Socket=%s"), *RiderSocketName.ToString());
    }
    else
    {
        Rider->AttachToComponent(CollisionRoot, Rules);
        UE_LOG(LogTemp, Warning, TEXT("[Vehicle] 라이더 부착 — 프록시(메시 없음, 소켓 미사용)"));
    }

    if (const ACharacter* RiderChar = Cast<ACharacter>(Rider))
    {
        if (USkeletalMeshComponent* MeshComp = RiderChar->GetMesh())
        {
            AController* Ctrl = Rider->GetController();
            APlayerController* PC = Cast<APlayerController>(Ctrl);
            ULocalPlayer* LP = PC ? PC->GetLocalPlayer() : nullptr;

            UE_LOG(LogTemp, Warning, TEXT("[Diag][Mount] Mesh=%s AnimClass=%s AnimationMode=%d AnimInstance=%s | Controller=%s LocalPlayer=%s"),
                *GetNameSafe(MeshComp),
                *GetNameSafe(MeshComp->AnimClass),
                (int32)MeshComp->GetAnimationMode(),
                *GetNameSafe(MeshComp->GetAnimInstance()),
                *GetNameSafe(Ctrl),
                LP ? *LP->GetClass()->GetName() : TEXT("None"));
        }
    }
     else if (AAC_CharacterBase* MoverRider = Cast<AAC_CharacterBase>(Rider))
    {
if (USkeletalMeshComponent* MeshComp = MoverRider->GetMesh())
        {
            AController* Ctrl = MoverRider->GetController();
            APlayerController* PC = Cast<APlayerController>(Ctrl);
            ULocalPlayer* LP = PC ? PC->GetLocalPlayer() : nullptr;

            UE_LOG(LogTemp, Warning, TEXT("[Diag][Mount] Mesh=%s AnimClass=%s AnimationMode=%d AnimInstance=%s | Controller=%s LocalPlayer=%s"),
                *GetNameSafe(MeshComp),
                *GetNameSafe(MeshComp->AnimClass),
                (int32)MeshComp->GetAnimationMode(),
                *GetNameSafe(MeshComp->GetAnimInstance()),
                *GetNameSafe(Ctrl),
                LP ? *LP->GetClass()->GetName() : TEXT("None"));
        }
    }
}

void AAC_MotorcyclePawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);

    // ALyraCharacter 가 하는 것과 같다: PossessedBy 시점엔 Pawn->InputComponent 가 아직
    // null 이라 HeroComponent 의 Spawned -> DataAvailable 전이(입력 컴포넌트 존재 조건)가
    // 실패하는 게 정상이다. 이 호출이 InputComponent 가 실제로 생긴 지금 시점에 초기화
    // 체인의 재시도를 걸어준다 — 이게 없으면 HeroComponent 가 Spawned 에 영원히 갇혀
    // PawnExtComponent 도 DataAvailable 을 못 넘어서고, 그 안에서 하는 카메라 모드
    // 델리게이트 바인딩도 영원히 실행되지 않는다.
    if (PawnExtComponent)
    {
        PawnExtComponent->SetupPlayerInputComponent();
    }

    ULyraInputComponent* LIC = Cast<ULyraInputComponent>(PlayerInputComponent);
    if (!LIC)
    {
        // UE_LOG(LogTemp, Error, TEXT("[Vehicle] LyraInputComponent 캐스팅 실패"));
        return;
    }

    const ULyraPawnExtensionComponent* PawnExt = ULyraPawnExtensionComponent::FindPawnExtensionComponent(this);
    const ULyraPawnData* PawnData = PawnExt ? PawnExt->GetPawnData<ULyraPawnData>() : nullptr;
    const ULyraInputConfig* InputConfig = PawnData ? PawnData->InputConfig : nullptr;

    if (!InputConfig)
    {
        // UE_LOG(LogTemp, Error, TEXT("[Vehicle] InputConfig NULL — BP_Motorcycle 의 PawnExtensionComponent 에 VehicleData_Motorcycle 이 설정됐는지 확인"));
        return;
    }

    // Completed 도 바인딩해야 키를 뗄 때 0 이 들어와 입력이 해제된다.
    LIC->BindNativeAction(InputConfig, LyraGameplayTags::InputTag_Vehicle_Throttle, ETriggerEvent::Triggered, this, &ThisClass::Input_Throttle, /*bLogIfNotFound=*/ true);
    LIC->BindNativeAction(InputConfig, LyraGameplayTags::InputTag_Vehicle_Throttle, ETriggerEvent::Completed, this, &ThisClass::Input_Throttle, /*bLogIfNotFound=*/ true);
    LIC->BindNativeAction(InputConfig, LyraGameplayTags::InputTag_Vehicle_Steer,    ETriggerEvent::Triggered, this, &ThisClass::Input_Steer,    /*bLogIfNotFound=*/ true);
    LIC->BindNativeAction(InputConfig, LyraGameplayTags::InputTag_Vehicle_Steer,    ETriggerEvent::Completed, this, &ThisClass::Input_Steer,    /*bLogIfNotFound=*/ true);
    LIC->BindNativeAction(InputConfig, LyraGameplayTags::InputTag_Vehicle_Dismount, ETriggerEvent::Started,   this, &ThisClass::Input_Dismount, /*bLogIfNotFound=*/ true);

    // UE_LOG(LogTemp, Warning, TEXT("[Vehicle] 입력 바인딩 완료"));

    // ViewTarget은 소유 중인 오토바이로 유지한다. 라이더를 ViewTarget으로 강제하면
    // Lyra의 카메라 침투 방지가 라이더 전체를 숨겨 탑승 캐릭터가 사라질 수 있다.

    // Priority 를 캐릭터 IMC(0) 보다 높게 둬야 W/A/S/D 를 오토바이가 가져간다.
    // 같은 우선순위면 먼저 처리된 컨텍스트가 키를 선점해 낮은 쪽 매핑이 등록되지 않는다.
    if (APlayerController* PC = Cast<APlayerController>(GetController()))
    {
        if (UEnhancedInputLocalPlayerSubsystem* Sub =
                ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
        {
            if (UInputMappingContext* IMC = RidingInputMappingContext.LoadSynchronous())
            {
                Sub->AddMappingContext(IMC, RidingInputMappingPriority);
                AppliedIMC = IMC;   // UnPossessed 에서 제거하기 위해 보관
            }
            else
            {
                UE_LOG(LogTemp, Error,
                    TEXT("[Vehicle] '%s' 의 RidingInputMappingContext 가 비어 있다. ")
                    TEXT("BP_Motorcycle 의 Class Defaults 에서 IMC_Motorcycle 을 지정할 것. ")
                    TEXT("이게 없으면 탑승은 되지만 조작이 전혀 먹지 않는다."),
                    *GetNameSafe(this));
            }
        }
    }
}

void AAC_MotorcyclePawn::Input_Throttle(const FInputActionValue& Value)
{
    if (MotorcycleMovement)
    {
        MotorcycleMovement->SetThrottleInput(Value.Get<float>());
    }
}

void AAC_MotorcyclePawn::Input_Steer(const FInputActionValue& Value)
{
    if (MotorcycleMovement)
    {
        MotorcycleMovement->SetSteerInput(Value.Get<float>());
    }
}

bool AAC_MotorcyclePawn::FindDismountLocation(FVector& OutLocation) const
{
    // 라이더의 실제 캡슐 크기를 쓴다. 고정값을 쓰면 캐릭터가 바뀔 때 어긋난다.
    float Radius = 40.0f;
    float HalfHeight = 90.0f;
    if (const ACharacter* RiderChar = Cast<ACharacter>(CurrentRider))
    {
        if (const UCapsuleComponent* Cap = RiderChar->GetCapsuleComponent())
        {
            Radius = Cap->GetScaledCapsuleRadius();
            HalfHeight = Cap->GetScaledCapsuleHalfHeight();
        }
    }

    const FVector Base = GetActorLocation();

    // 좌 -> 우 순으로 시도한다.
    const FVector Candidates[] = {
        Base - GetActorRightVector() * DismountExitOffset,
        Base + GetActorRightVector() * DismountExitOffset,
    };

    FCollisionQueryParams Params(SCENE_QUERY_STAT(MotorcycleDismount), false, this);
    if (CurrentRider)
    {
        Params.AddIgnoredActor(CurrentRider);
    }

    for (const FVector& Candidate : Candidates)
    {
        // 후보 지점의 지면을 찾는다. 오토바이 원점 높이에 캡슐을 그대로 두면
        // 캡슐 아랫부분이 땅에 박혀 항상 "막힘"으로 판정된다.
        const FVector TraceStart = Candidate + FVector(0.0f, 0.0f, HalfHeight);
        const FVector TraceEnd   = Candidate - FVector(0.0f, 0.0f, GroundSearchDistance);

        FHitResult GroundHit;
        if (!GetWorld()->LineTraceSingleByChannel(GroundHit, TraceStart, TraceEnd, ECC_Visibility, Params))
        {
            continue;   // 지면이 없다 (허공/낭떠러지)
        }

        // 캡슐이 지면 위에 서도록 올린다. 2cm 는 지면과의 여유.
        const FVector Standing = GroundHit.Location + FVector(0.0f, 0.0f, HalfHeight + 2.0f);

        if (!GetWorld()->OverlapAnyTestByChannel(Standing, FQuat::Identity, ECC_Pawn,
                FCollisionShape::MakeCapsule(Radius, HalfHeight), Params))
        {
            OutLocation = Standing;
            return true;
        }
    }

    return false;
}

void AAC_MotorcyclePawn::Input_Dismount(const FInputActionValue& /*Value*/)
{
    if (!MotorcycleMovement)
    {
        return;
    }

    const float Speed = MotorcycleMovement->GetSpeed();
    if (Speed > DismountMaxSpeed)
    {
        UE_LOG(LogTemp, Warning, TEXT("[Vehicle] 하차 거부 — 속도 %.0f > %.0f"), Speed, DismountMaxSpeed);
        return;
    }

    FVector ExitLocation;
    if (!FindDismountLocation(ExitLocation))
    {
        UE_LOG(LogTemp, Warning, TEXT("[Vehicle] 하차 거부 — 하차 지점이 막힘"));
        return;
    }

    AController* PC = GetController();
    APawn* Rider = DetachRider();
    if (!PC || !Rider)
    {
        UE_LOG(LogTemp, Error, TEXT("[Vehicle] 하차 실패 — Controller=%s Rider=%s"), *GetNameSafe(PC), *GetNameSafe(Rider));
        return;
    }

    Rider->SetActorLocation(ExitLocation, false, nullptr, ETeleportType::TeleportPhysics);
    PC->Possess(Rider);

    // 재빙의된 캐릭터의 입력 바인딩을 재구성한다.
    // (Lyra 는 재빙의를 상정하지 않아 그냥 두면 바인딩이 빈 채로 남는다)
    // 내부에서 ClearAllMappings 도 하므로 오토바이 IMC 가 함께 정리된다.
    if (ULyraHeroComponent* HeroComp = ULyraHeroComponent::FindHeroComponent(Rider))
    {
        HeroComp->ReinitializePlayerInput(Rider->InputComponent);
    }

    // 오토바이가 ASC를 가져갈 때 기존 캐릭터의 PawnExtensionComponent는 ASC 포인터를
    // 비운다. 직접 InitAbilityActorInfo()만 호출하면 그 포인터와 초기화 델리게이트가
    // 복구되지 않으므로 Lyra의 공식 초기화 경로를 사용한다.
    if (ALyraPlayerState* LyraPS = Rider->GetPlayerState<ALyraPlayerState>())
    {
        if (ULyraAbilitySystemComponent* ASC = LyraPS->GetLyraAbilitySystemComponent())
        {
            if (ULyraPawnExtensionComponent* RiderPawnExt =
                    ULyraPawnExtensionComponent::FindPawnExtensionComponent(Rider))
            {
                RiderPawnExt->InitializeAbilitySystem(ASC, LyraPS);
            }
        }
    }

    if (const ACharacter* RiderChar = Cast<ACharacter>(Rider))
    {
        if (USkeletalMeshComponent* MeshComp = RiderChar->GetMesh())
        {
            AController* DismountCtrl = Rider->GetController();
            APlayerController* DismountPC = Cast<APlayerController>(DismountCtrl);
            ULocalPlayer* DismountLP = DismountPC ? DismountPC->GetLocalPlayer() : nullptr;

            UE_LOG(LogTemp, Warning, TEXT("[Diag][Dismount] Mesh=%s AnimClass=%s AnimationMode=%d AnimInstance=%s | Controller=%s LocalPlayer=%s"),
                *GetNameSafe(MeshComp),
                *GetNameSafe(MeshComp->AnimClass),
                (int32)MeshComp->GetAnimationMode(),
                *GetNameSafe(MeshComp->GetAnimInstance()),
                *GetNameSafe(DismountCtrl),
                DismountLP ? *DismountLP->GetClass()->GetName() : TEXT("None"));
        }
    }

    UE_LOG(LogTemp, Warning, TEXT("[Vehicle] 하차 — Rider=%s"), *GetNameSafe(Rider));
}

APawn* AAC_MotorcyclePawn::DetachRider()
{
    APawn* Rider = CurrentRider;
    if (!Rider)
    {
        return nullptr;
    }

    Rider->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
    Rider->SetActorEnableCollision(true);

    if (ACharacter* RiderChar = Cast<ACharacter>(Rider))
    {
        if (UCharacterMovementComponent* CMC = RiderChar->GetCharacterMovement())
        {
            CMC->SetMovementMode(MOVE_Walking);
        }
    }
    else if (AAC_CharacterBase* MoverRider = Cast<AAC_CharacterBase>(Rider))
    {
        // Walking 이 아니라 Falling 으로 되돌린다. 하차 지점이 공중이면 Walking 은 틀린
        // 상태로 시작하는 셈이지만, Falling 은 접지하는 순간 Mover 가 알아서 Walking 으로
        // 넘겨준다. CMC 판이 MOVE_Walking 을 쓰는 것과 다른 이유가 이것이다.
        if (UMoverComponent* Mover = MoverRider->GetMoverComponent())
        {
            Mover->QueueNextMode(DefaultModeNames::Falling);
        }

        MoverRider->bIsMounted = false;
    }

    CurrentRider = nullptr;
    UE_LOG(LogTemp, Warning, TEXT("[Vehicle] 라이더 분리"));
    return Rider;
}
