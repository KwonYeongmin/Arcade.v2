// Source/LyraGame/AbilitySystem/Profiler/GASProfilerActor.h
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AbilitySystem/Profiler/GASProfilerTypes.h"
#include "GASProfilerActor.generated.h"

class UGASProfilerSubsystem;

// RC 전용 thin wrapper. 로직 없음 — 모두 UGASProfilerSubsystem에 위임.
UCLASS(NotBlueprintable, NotPlaceable)
class LYRAGAME_API AGASProfilerActor : public AActor
{
    GENERATED_BODY()

public:
    AGASProfilerActor();

    UFUNCTION(BlueprintCallable, Category="GASProfiler")
    FGASProfilerSnapshot GetSnapshot();

    UFUNCTION(BlueprintCallable, Category="GASProfiler")
    void ForceActivateAbility(const FString& AbilityTag);

    UFUNCTION(BlueprintCallable, Category="GASProfiler")
    void SetAttributeValue(const FString& AttributeName, float Value);

    UFUNCTION(BlueprintCallable, Category="GASProfiler")
    void ClearEventHistory();

private:
    UGASProfilerSubsystem* GetProfilerSubsystem() const;
};
