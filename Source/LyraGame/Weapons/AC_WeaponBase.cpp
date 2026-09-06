// Copyright Epic Games, Inc. All Rights Reserved.

#include "Weapons/AC_WeaponBase.h"
#include "Weapons/AC_WeaponEffectBase.h"

#include "Components/AudioComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraSystem.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "Sound/SoundBase.h"
#include "Teams/LyraTeamDisplayAsset.h"


#include UE_INLINE_GENERATED_CPP_BY_NAME(AC_WeaponBase)

AAC_WeaponBase::AAC_WeaponBase(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
}

// ─── BeginPlay ───────────────────────────────────────────────────────────────

void AAC_WeaponBase::BeginPlay()
{
    Super::BeginPlay();

    // ObserveTeamColors async 체인에서 TeamAgent로 사용하기 위해 미리 초기화.
    // BP EventGraph의 ObserveTeamColors 노드는 이 값을 읽으므로 C++ BeginPlay가 먼저 실행되어야 한다.
    OwnerAsPawn = Cast<APawn>(GetOwner());
}

// ─── Fire ────────────────────────────────────────────────────────────────────

PRAGMA_DISABLE_SHADOW_VARIABLE_WARNINGS
void AAC_WeaponBase::Fire(const TArray<FVector>& ImpactPositions,
                          const TArray<FVector>& ImpactNormals,
                          const TArray<TEnumAsByte<EPhysicalSurface>>& ImpactSurfaceTypes)
{
    // 1. 임팩트 데이터 멤버에 캐시
    this->ImpactPositions    = ImpactPositions;
    this->ImpactNormals      = ImpactNormals;
    this->ImpactSurfaceTypes = ImpactSurfaceTypes;

    // GCN이 ImpactNormals를 전달하지 않으면 포지션 수만큼 UpVector로 채움
    if (this->ImpactNormals.IsEmpty() && !this->ImpactPositions.IsEmpty())
    {
        this->ImpactNormals.Init(FVector::UpVector, this->ImpactPositions.Num());
    }

    // 2. Muzzle 소켓 위치 저장
    if (USkeletalMeshComponent* WeaponMesh = GetWeaponMesh())
    {
        MuzzlePosition = WeaponMesh->GetSocketLocation(FName("Muzzle"));
    }

    // 3. 다탄 무기(샷건) — 가상 탄환 데이터 추가
    if (bNeedsFakeProjectileData)
    {
        AddFakeProjectileData(NumerOfFakeProjectiles, 5.0f);
    }

    // 4. 이팩트 서브액터 3개에 데이터 전달 + Fire()
    USkeletalMeshComponent* WeaponMesh = GetWeaponMesh();

    EnsureSubActor(WeaponFire, WeaponFireClass, WeaponMesh);
    if (IsValid(WeaponFire))
    {
        TransferImpactDataAndFire(WeaponFire);
    }

    EnsureSubActor(WeaponImpacts, WeaponImpactsClass, WeaponMesh);
    if (IsValid(WeaponImpacts))
    {
        TransferImpactDataAndFire(WeaponImpacts);
    }

    EnsureSubActor(WeaponDecals, WeaponDecalsClass, WeaponMesh);
    if (IsValid(WeaponDecals))
    {
        TransferImpactDataAndFire(WeaponDecals);
    }
}
PRAGMA_ENABLE_SHADOW_VARIABLE_WARNINGS

// ─── TriggerFireAudio ────────────────────────────────────────────────────────

void AAC_WeaponBase::TriggerFireAudio(USoundBase* Sound, AActor* Actor)
{
    // AudioComponent가 없으면 새로 스폰 후 저장 (lazy init)
    if (!IsValid(AudioComponent))
    {
        USceneComponent* AttachTarget = nullptr;
        if (IsValid(Actor))
        {
            AttachTarget = Actor->FindComponentByClass<USceneComponent>();
        }

        UAudioComponent* NewAC = UGameplayStatics::SpawnSoundAttached(
            Sound, AttachTarget, FName("hand_r"),
            FVector::ZeroVector, FRotator::ZeroRotator,
            EAttachLocation::KeepRelativeOffset,
            /*bStopWhenAttachedToDestroyed=*/false);

        AudioComponent = NewAC;
    }

    // AudioParameterController 인터페이스로 "Fire" 트리거 파라미터 전송.
    // UAudioComponent가 IAudioParameterControllerInterface를 구현하므로 Cast 없이 직접 호출.
    // Cast<IAudioParameterControllerInterface>는 AudioExtensions 모듈 의존성을 유발하므로 사용하지 않음.
    if (IsValid(AudioComponent))
    {
        AudioComponent->SetTriggerParameter(FName("Fire"));
    }
}

// ─── UpdateCustomStencil ─────────────────────────────────────────────────────

void AAC_WeaponBase::UpdateCustomStencil(int32 TeamId)
{
    // 로컬 플레이어 소유 무기이면 TeamId, 아니면 0
    const bool bLocallyControlled = IsValid(OwnerAsPawn) && OwnerAsPawn->IsLocallyControlled();
    const int32 StencilValue = bLocallyControlled ? TeamId : 0;

    TArray<UPrimitiveComponent*> PrimComps;
    GetComponents<UPrimitiveComponent>(PrimComps, /*bIncludeChildActors=*/true);
    for (UPrimitiveComponent* Comp : PrimComps)
    {
        Comp->SetCustomDepthStencilValue(StencilValue);
    }
}

// ─── UpdateTeamColors ────────────────────────────────────────────────────────

void AAC_WeaponBase::UpdateTeamColors(ULyraTeamDisplayAsset* TeamDisplayAsset)
{
    if (!IsValid(TeamDisplayAsset) || !bApplyTeamColorsToWeapon) return;
    TeamDisplayAsset->ApplyToActor(this, /*bIncludeChildActors=*/true);
}

// ─── AddFakeProjectileData ───────────────────────────────────────────────────

void AAC_WeaponBase::AddFakeProjectileData(int32 NumProjectiles, float ConeHalfAngleDeg)
{
    if (NumProjectiles <= 0 || ImpactPositions.Num() == 0) return;

    USkeletalMeshComponent* WeaponMesh = GetWeaponMesh();
    if (!WeaponMesh) return;

    UWorld* World = GetWorld();
    if (!World) return;

    FVector MuzzleLocation = WeaponMesh->GetSocketLocation(FName("Muzzle"));
    FVector Direction = (ImpactPositions[0] - MuzzleLocation).GetSafeNormal();

    FCollisionQueryParams Params;
    Params.AddIgnoredActor(this);

    const float TraceLength   = 1000000.0f;
    const float HalfAngleRad  = FMath::DegreesToRadians(ConeHalfAngleDeg);

    for (int32 i = 0; i < NumProjectiles; ++i)
    {
        FVector RandDir  = FMath::VRandCone(Direction, HalfAngleRad);
        FVector TraceEnd = MuzzleLocation + RandDir * TraceLength;

        FHitResult OutHit;
        if (World->LineTraceSingleByChannel(OutHit, MuzzleLocation, TraceEnd, ECC_Visibility, Params))
        {
            TEnumAsByte<EPhysicalSurface> SurfaceType = SurfaceType_Default;
            if (OutHit.PhysMaterial.IsValid())
            {
                SurfaceType = OutHit.PhysMaterial->SurfaceType;
            }

            ImpactPositions.Add(OutHit.Location);
            ImpactNormals.Add(OutHit.Normal);
            ImpactSurfaceTypes.Add(SurfaceType);
        }
    }
}

// ─── Private helpers ─────────────────────────────────────────────────────────

USkeletalMeshComponent* AAC_WeaponBase::GetWeaponMesh() const
{
    return FindComponentByClass<USkeletalMeshComponent>();
}

void AAC_WeaponBase::EnsureSubActor(TObjectPtr<AActor>& SubActorRef,
                                     TSubclassOf<AActor> SubActorClass,
                                     USkeletalMeshComponent* AttachParent)
{
    if (IsValid(SubActorRef)) return;
    if (!IsValid(SubActorClass)) return;

    UWorld* World = GetWorld();
    if (!World) return;

    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    AActor* Spawned = World->SpawnActor<AActor>(SubActorClass, FTransform::Identity, Params);
    if (!IsValid(Spawned)) return;

    SubActorRef = Spawned;

    if (IsValid(AttachParent))
    {
        Spawned->AttachToComponent(AttachParent, FAttachmentTransformRules::KeepRelativeTransform);
    }

    // WeaponFire 전용 초기 프로퍼티 설정 (Shell Eject / Muzzle Flash / Tracer 에셋)
    if (SubActorRef == WeaponFire)
    {
        InitWeaponFireProperties(Spawned, AttachParent);
    }
}

void AAC_WeaponBase::InitWeaponFireProperties(AActor* InWeaponFire,
                                               USkeletalMeshComponent* WeaponMesh) const
{
    if (!IsValid(InWeaponFire)) return;

    // null이 아닐 때만 덮어씀 — B_WeaponFire 자체 기본값(Niagara 에셋)을 null로 오염시키지 않는다.
    if (IsValid(ShellEjectSystem))
        SetPropertyByName(InWeaponFire, FName("Shell Eject System"),  &ShellEjectSystem);
    if (IsValid(MuzzleFlashSystem))
        SetPropertyByName(InWeaponFire, FName("Muzzle Flash System"), &MuzzleFlashSystem);
    if (IsValid(TracerSystem))
        SetPropertyByName(InWeaponFire, FName("Tracer System"),       &TracerSystem);
    if (IsValid(ShellEjectMesh))
        SetPropertyByName(InWeaponFire, FName("Shell Eject Mesh"),    &ShellEjectMesh);

    if (IsValid(WeaponMesh))
        SetPropertyByName(InWeaponFire, FName("Skeletal Mesh Component"), &WeaponMesh);
}

void AAC_WeaponBase::TransferImpactDataAndFire(AActor* SubActor)
{
    if (!IsValid(SubActor)) return;

    // AAC_WeaponEffectBase 자식이면 직접 대입 (리플렉션 오버헤드 없음)
    if (AAC_WeaponEffectBase* EffectActor = Cast<AAC_WeaponEffectBase>(SubActor))
    {
        EffectActor->ImpactPositions    = ImpactPositions;
        EffectActor->ImpactNormals      = ImpactNormals;
        EffectActor->ImpactSurfaceTypes = ImpactSurfaceTypes;
        EffectActor->MuzzlePosition     = MuzzlePosition;
        // B_WeaponDecals는 "Impact Surface Types"(공백) BP 변수를 별도로 사용
        SetPropertyByName(SubActor, FName("Impact Surface Types"), &ImpactSurfaceTypes);
    }
    else
    {
        // 비-베이스 서브액터 폴백: 리플렉션으로 전달
        SetPropertyByName(SubActor, FName("ImpactPositions"),      &ImpactPositions);
        SetPropertyByName(SubActor, FName("ImpactNormals"),        &ImpactNormals);
        SetPropertyByName(SubActor, FName("ImpactSurfaceTypes"),   &ImpactSurfaceTypes);
        SetPropertyByName(SubActor, FName("MuzzlePosition"),       &MuzzlePosition);
        SetPropertyByName(SubActor, FName("Impact Surface Types"), &ImpactSurfaceTypes);
    }

    // "Fire" 이벤트 먼저, 없으면 "fire"(B_WeaponDecals 소문자) 시도
    UFunction* FireFunc = SubActor->FindFunction(FName("Fire"));
    if (!FireFunc)
    {
        FireFunc = SubActor->FindFunction(FName("fire"));
    }
    if (FireFunc)
    {
        SubActor->ProcessEvent(FireFunc, nullptr);
    }
}

void AAC_WeaponBase::SetPropertyByName(UObject* Target, FName PropName, const void* SrcValuePtr)
{
    if (!IsValid(Target) || !SrcValuePtr) return;

    FProperty* Prop = Target->GetClass()->FindPropertyByName(PropName);
    if (!Prop) return;

    void* DestPtr = Prop->ContainerPtrToValuePtr<void>(Target);
    Prop->CopyCompleteValue(DestPtr, SrcValuePtr);
}
