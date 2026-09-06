// Source/LyraGame/AbilitySystem/Profiler/GASProfilerTypes.h
#pragma once

#include "CoreMinimal.h"
#include "GASProfilerTypes.generated.h"

USTRUCT(BlueprintType)
struct LYRAGAME_API FGASAbilityInfo
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) FString ClassName;
    UPROPERTY(BlueprintReadOnly) FString ActivationGroup;
    UPROPERTY(BlueprintReadOnly) FString OwnedTags;
    UPROPERTY(BlueprintReadOnly) bool bIsActive = false;
};

USTRUCT(BlueprintType)
struct LYRAGAME_API FGASEffectInfo
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) FString ClassName;
    UPROPERTY(BlueprintReadOnly) float Duration = 0.f;
    UPROPERTY(BlueprintReadOnly) float TimeRemaining = 0.f;
    UPROPERTY(BlueprintReadOnly) int32 StackCount = 1;
};

USTRUCT(BlueprintType)
struct LYRAGAME_API FGASAttributeInfo
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) FString Name;
    UPROPERTY(BlueprintReadOnly) float CurrentValue = 0.f;
    UPROPERTY(BlueprintReadOnly) float BaseValue = 0.f;
};

USTRUCT(BlueprintType)
struct LYRAGAME_API FGASEventEntry
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) float Timestamp = 0.f;
    // EventType values: "ACTIVATED" | "ENDED" | "FAILED" | "HEALTH_CHANGED" | "ANIM_CHANGED"
    UPROPERTY(BlueprintReadOnly) FString EventType;
    UPROPERTY(BlueprintReadOnly) FString AbilityOrTag;
    // Detail example: "60.0 -> 45.0 (-15)"
    UPROPERTY(BlueprintReadOnly) FString Detail;
};

/**
 * What the character is animating right now.
 *
 * Motion matching picks a new clip every frame, so "which animation is playing" is the first
 * question when a transition looks wrong. Montages are listed alongside it because an ability
 * that plays a montage will mask whatever the anim graph selected.
 */
USTRUCT(BlueprintType)
struct LYRAGAME_API FGASAnimationInfo
{
    GENERATED_BODY()

    // Source values: "MotionMatching" | "Montage"
    UPROPERTY(BlueprintReadOnly) FString Source;
    UPROPERTY(BlueprintReadOnly) FString AssetName;
    // Motion matching only: the database the pose was selected from.
    UPROPERTY(BlueprintReadOnly) FString DatabaseName;
    UPROPERTY(BlueprintReadOnly) float Time = 0.f;
    UPROPERTY(BlueprintReadOnly) float Weight = 1.f;
    // Motion matching only: true when the search kept the pose it was already playing rather
    // than picking a new one. A transition that will not happen usually shows up here first.
    UPROPERTY(BlueprintReadOnly) bool bIsContinuingPose = false;
    UPROPERTY(BlueprintReadOnly) float SearchCost = 0.f;
};

USTRUCT(BlueprintType)
struct LYRAGAME_API FGASProfilerSnapshot
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) TArray<FGASAbilityInfo>   ActiveAbilities;
    UPROPERTY(BlueprintReadOnly) TArray<FGASEffectInfo>    ActiveEffects;
    UPROPERTY(BlueprintReadOnly) TArray<FGASAttributeInfo> Attributes;
    UPROPERTY(BlueprintReadOnly) TArray<FGASAnimationInfo>  Animation;
    // Most recent MaxHistory entries, oldest first
    UPROPERTY(BlueprintReadOnly) TArray<FGASEventEntry>    EventHistory;
};
