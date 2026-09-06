// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameFramework/Actor.h"
#include "PhysicalMaterials/PhysicalMaterial.h"

#include "AC_WeaponEffectBase.generated.h"

#define UE_API LYRAGAME_API

/**
 * AAC_WeaponEffectBase
 *
 * B_WeaponFire / B_WeaponImpacts / B_WeaponDecals 공통 베이스 클래스.
 * AAC_WeaponBase::TransferImpactDataAndFire에서 Cast 후 임팩트 데이터를 직접 대입한다.
 *
 * Fire 실행 로직은 BP 커스텀 이벤트("Fire"/"fire")에 그대로 두어
 * C++ UFUNCTION 충돌을 방지한다.
 */
UCLASS(MinimalAPI, Blueprintable)
class AAC_WeaponEffectBase : public AActor
{
    GENERATED_BODY()

public:
    UE_API AAC_WeaponEffectBase(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

    // ─── 임팩트 데이터 (AAC_WeaponBase에서 매 Fire마다 주입) ─────────────────

    UPROPERTY(BlueprintReadWrite, Transient, Category="WeaponEffect|ImpactData")
    TArray<FVector> ImpactPositions;

    UPROPERTY(BlueprintReadWrite, Transient, Category="WeaponEffect|ImpactData")
    TArray<FVector> ImpactNormals;

    UPROPERTY(BlueprintReadWrite, Transient, Category="WeaponEffect|ImpactData")
    TArray<TEnumAsByte<EPhysicalSurface>> ImpactSurfaceTypes;

    UPROPERTY(BlueprintReadWrite, Transient, Category="WeaponEffect|ImpactData")
    FVector MuzzlePosition = FVector::ZeroVector;
};

#undef UE_API
