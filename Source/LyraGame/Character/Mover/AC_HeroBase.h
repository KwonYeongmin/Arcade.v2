// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Character/Mover/AC_CharacterBase.h"

#include "AC_HeroBase.generated.h"

#define UE_API LYRAGAME_API

/**
 * AAC_HeroBase
 *
 * Mover 계열에서 ALyraHeroCharacter 에 대응하는 층이다.
 *
 * CMC 계열이 ALyraCharacter → ALyraHeroCharacter → AAC_ShooterCharacter 로 책임을 나눈 것과
 * 같은 층위를 Mover 쪽에도 둔다. 이 프로젝트의 목적이 Lyra 캐릭터 시스템을 그대로 따르는
 * 것이므로, 기능만 옮기고 구조를 뭉개면 목적에 어긋난다.
 *
 * 아직 비어 있다. 계약 이식 계획(docs/superpowers/specs/2026-08-22-lyra-character-contract-inventory.md §7)
 * 이 이 층에 배정한 것은 다음이며, 사망 처리 덩어리로 묶여 별도 작업으로 남아 있다:
 *
 *   ILyraContextEffectsInterface · Ragdoll() · DeathMontages · RagdollImpulse* ·
 *   HideEquippedWeapons() · FootStep*
 *
 * 비어 있다고 지우지 말 것. 이 층이 없으면 AAC_ShooterPawn 이 위 기능을 받을 자리가 없어져
 * 다시 한 클래스에 전부 쌓이게 된다.
 */
UCLASS(MinimalAPI, Blueprintable)
class AAC_HeroBase : public AAC_CharacterBase
{
	GENERATED_BODY()

public:
	UE_API AAC_HeroBase(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
};

#undef UE_API
