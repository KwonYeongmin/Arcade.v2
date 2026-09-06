// Source/LyraGame/AbilitySystem/Profiler/GASProfilerSubsystem.h
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "AbilitySystem/Profiler/GASProfilerTypes.h"
#include "GASProfilerSubsystem.generated.h"

class ULyraAbilitySystemComponent;
class UGameplayAbility;
class AGASProfilerActor;
struct FAbilityEndedData;
struct FGameplayTagContainer;
struct FOnAttributeChangeData;

UCLASS()
class LYRAGAME_API UGASProfilerSubsystem : public UTickableWorldSubsystem
{
    GENERATED_BODY()

public:
    // UTickableWorldSubsystem interface
    void Initialize(FSubsystemCollectionBase& Collection) override;
    void Deinitialize() override;
    void Tick(float DeltaTime) override;
    TStatId GetStatId() const override;

    // RC endpoint — called by AGASProfilerActor
    UFUNCTION(BlueprintCallable, Category="GASProfiler")
    FGASProfilerSnapshot GetSnapshot();

    UFUNCTION(BlueprintCallable, Category="GASProfiler")
    void ForceActivateAbility(const FString& AbilityTag);

    UFUNCTION(BlueprintCallable, Category="GASProfiler")
    void SetAttributeValue(const FString& AttributeName, float Value);

    UFUNCTION(BlueprintCallable, Category="GASProfiler")
    void ClearEventHistory();

    AGASProfilerActor* GetProfilerActor() const { return ProfilerActor; }

private:
    ULyraAbilitySystemComponent* GetLocalASC() const;

    void BindASCDelegates(ULyraAbilitySystemComponent* ASC);
    void UnbindASCDelegates(ULyraAbilitySystemComponent* ASC);
    void RefreshSnapshot();
    // Sampled every frame rather than on TickInterval: a landing window is shorter than the
    // 0.5s snapshot cadence, so a transition would be missed entirely at that rate.
    void RefreshAnimationSnapshot();
    void AddEventEntry(const FString& EventType, const FString& AbilityOrTag, const FString& Detail = TEXT(""));

    // Delegate handlers
    void OnAbilityActivated(UGameplayAbility* Ability);
    void OnAbilityEnded(UGameplayAbility* Ability);
    void OnAbilityFailed(const UGameplayAbility* Ability, const FGameplayTagContainer& FailureTags);
    void OnHealthAttributeChanged(const FOnAttributeChangeData& Data);

    UPROPERTY() TObjectPtr<ULyraAbilitySystemComponent> BoundASC;
    UPROPERTY() TObjectPtr<AGASProfilerActor> ProfilerActor;

    FGASProfilerSnapshot CachedSnapshot;
    TArray<FGASEventEntry> EventHistory;

    // Last animation reported, used to emit an ANIM_CHANGED event only on the frame it changes.
    FString LastAnimationName;

    float TickAccumulator = 0.f;
    static constexpr float TickInterval = 0.5f;
    static constexpr int32 MaxHistory = 100;
};
