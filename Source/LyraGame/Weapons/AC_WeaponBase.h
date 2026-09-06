// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameFramework/Actor.h"
#include "PhysicalMaterials/PhysicalMaterial.h"

#include "AC_WeaponBase.generated.h"

#define UE_API LYRAGAME_API

class UAudioComponent;
class ULyraTeamDisplayAsset;
class UNiagaraSystem;
class USoundBase;
class USkeletalMeshComponent;
class UStaticMesh;

/**
 * AAC_WeaponBase
 *
 * B_Weapon BP 로직을 C++로 포팅한 무기 베이스 클래스.
 * B_Weapon BP의 Parent Class로 사용된다. (Arcade 프로젝트 이식 대상)
 *
 * 역할:
 *  - Fire 이벤트 처리 (임팩트 데이터 캐시 → 이팩트 서브액터 lazy spawn → Fire 전달)
 *  - 팀 색상 / CustomStencil 업데이트
 *  - 발사 오디오 트리거 (AudioParameterController 인터페이스)
 *
 * BP에 유지되는 것:
 *  - ObserveTeamColors AsyncAction 체인 (BP AsyncAction 노드 전용)
 *  - 무기별 컴포넌트/에셋 설정 (메시, 파티클 등)
 */
UCLASS(MinimalAPI, Blueprintable)
class AAC_WeaponBase : public AActor
{
    GENERATED_BODY()

public:
    UE_API AAC_WeaponBase(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

    // ─── 퍼블릭 API ──────────────────────────────────────────────────────────

    /** Gameplay Ability에서 무기 발사 시 호출. 이팩트 서브액터들에 데이터를 전달하고 Fire()를 구동한다. */
    UFUNCTION(BlueprintCallable, Category="Weapon", meta=(AutoCreateRefTerm="ImpactPositions,ImpactNormals,ImpactSurfaceTypes"))
    UE_API void Fire(const TArray<FVector>& ImpactPositions,
                     const TArray<FVector>& ImpactNormals,
                     const TArray<TEnumAsByte<EPhysicalSurface>>& ImpactSurfaceTypes);

    /** 발사 사운드를 AudioParameterController 인터페이스로 트리거한다. */
    UFUNCTION(BlueprintCallable, Category="Weapon")
    UE_API void TriggerFireAudio(USoundBase* Sound, AActor* Actor);

    /** 팀 ID 기반으로 모든 PrimitiveComponent의 CustomDepthStencil 값을 설정한다. */
    UFUNCTION(BlueprintCallable, Category="Weapon")
    UE_API void UpdateCustomStencil(int32 TeamId);

    /** 팀 색상 에셋을 이 액터에 적용한다. ApplyTeamColorsToWeapon이 true일 때만 실행. */
    UFUNCTION(BlueprintCallable, Category="Weapon")
    UE_API void UpdateTeamColors(ULyraTeamDisplayAsset* TeamDisplayAsset);

public:
    // ─── 서브액터 클래스 (BP에서 설정) ───────────────────────────────────────

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|SubActors")
    TSubclassOf<AActor> WeaponFireClass;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|SubActors")
    TSubclassOf<AActor> WeaponImpactsClass;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|SubActors")
    TSubclassOf<AActor> WeaponDecalsClass;

    // ─── 이팩트 에셋 (BP에서 설정) ───────────────────────────────────────────

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Weapon|Effects")
    TObjectPtr<UNiagaraSystem> ShellEjectSystem;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Weapon|Effects")
    TObjectPtr<UNiagaraSystem> MuzzleFlashSystem;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Weapon|Effects")
    TObjectPtr<UNiagaraSystem> TracerSystem;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Weapon|Effects")
    TObjectPtr<UStaticMesh> ShellEjectMesh;

    // ─── 동작 제어 (BP에서 설정) ─────────────────────────────────────────────

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Weapon|Config")
    bool bApplyTeamColorsToWeapon = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Weapon|Config")
    bool bNeedsFakeProjectileData = false;

    /** BP 원본 변수명 오타(Numer) 유지 — 이름 변경 시 BP 변수와 불일치 발생 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Weapon|Config")
    int32 NumerOfFakeProjectiles = 0;

    // ─── 런타임 레퍼런스 ─────────────────────────────────────────────────────

    UPROPERTY(BlueprintReadWrite, Transient, Category="Weapon|Runtime")
    TObjectPtr<APawn> OwnerAsPawn;

    UPROPERTY(BlueprintReadWrite, Transient, Category="Weapon|Runtime")
    TObjectPtr<AActor> WeaponFire;

    UPROPERTY(BlueprintReadWrite, Transient, Category="Weapon|Runtime")
    TObjectPtr<AActor> WeaponImpacts;

    UPROPERTY(BlueprintReadWrite, Transient, Category="Weapon|Runtime")
    TObjectPtr<AActor> WeaponDecals;

    UPROPERTY(BlueprintReadWrite, Transient, Category="Weapon|Runtime")
    TObjectPtr<UAudioComponent> AudioComponent;

    // ─── 임팩트 캐시 (Fire 시 수신, 서브액터로 전달) ────────────────────────

    UPROPERTY(BlueprintReadWrite, Transient, Category="Weapon|ImpactData")
    TArray<FVector> ImpactPositions;

    UPROPERTY(BlueprintReadWrite, Transient, Category="Weapon|ImpactData")
    TArray<FVector> ImpactNormals;

    UPROPERTY(BlueprintReadWrite, Transient, Category="Weapon|ImpactData")
    TArray<TEnumAsByte<EPhysicalSurface>> ImpactSurfaceTypes;

    UPROPERTY(BlueprintReadWrite, Transient, Category="Weapon|ImpactData")
    FVector MuzzlePosition = FVector::ZeroVector;

protected:
    virtual void BeginPlay() override;

    /**
     * 다탄 무기(샷건 등)에서 가상 탄환 임팩트 데이터를 추가 생성한다.
     * Add Fake Projectile Data BP 함수 포팅 — 그래프 미분석, 추후 구현 필요.
     */
    UFUNCTION(BlueprintCallable, Category="Weapon")
    UE_API void AddFakeProjectileData(int32 NumProjectiles, float ConeHalfAngleDeg);

private:
    USkeletalMeshComponent* GetWeaponMesh() const;

    /** SubActorRef가 유효하지 않으면 SubActorClass로 스폰하고 AttachParent에 부착한다. */
    void EnsureSubActor(TObjectPtr<AActor>& SubActorRef, TSubclassOf<AActor> SubActorClass,
                        USkeletalMeshComponent* AttachParent);

    /** WeaponFire 스폰 직후 이팩트 에셋 프로퍼티(ShellEjectSystem 등)를 전달한다. */
    void InitWeaponFireProperties(AActor* InWeaponFire, USkeletalMeshComponent* WeaponMesh) const;

    /** SubActor의 ImpactPositions/Normals/SurfaceTypes/MuzzlePosition을 설정하고 Fire()를 호출한다. */
    void TransferImpactDataAndFire(AActor* SubActor);

    /** 리플렉션으로 프로퍼티 값을 복사한다. 프로퍼티가 없으면 무시. */
    static void SetPropertyByName(UObject* Target, FName PropName, const void* SrcValuePtr);
};

#undef UE_API
