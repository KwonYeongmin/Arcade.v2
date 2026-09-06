// Source/LyraGame/AbilitySystem/Profiler/GASProfilerActor.cpp
#include "AbilitySystem/Profiler/GASProfilerActor.h"
#include "AbilitySystem/Profiler/GASProfilerSubsystem.h"
#include "Engine/World.h"

AGASProfilerActor::AGASProfilerActor()
{
    PrimaryActorTick.bCanEverTick = false;
    bNetLoadOnClient = false;
}

UGASProfilerSubsystem* AGASProfilerActor::GetProfilerSubsystem() const
{
    UWorld* World = GetWorld();
    return World ? World->GetSubsystem<UGASProfilerSubsystem>() : nullptr;
}

FGASProfilerSnapshot AGASProfilerActor::GetSnapshot()
{
    if (UGASProfilerSubsystem* Sub = GetProfilerSubsystem())
    {
        return Sub->GetSnapshot();
    }
    return FGASProfilerSnapshot{};
}

void AGASProfilerActor::ForceActivateAbility(const FString& AbilityTag)
{
    if (UGASProfilerSubsystem* Sub = GetProfilerSubsystem())
    {
        Sub->ForceActivateAbility(AbilityTag);
    }
}

void AGASProfilerActor::SetAttributeValue(const FString& AttributeName, float Value)
{
    if (UGASProfilerSubsystem* Sub = GetProfilerSubsystem())
    {
        Sub->SetAttributeValue(AttributeName, Value);
    }
}

void AGASProfilerActor::ClearEventHistory()
{
    if (UGASProfilerSubsystem* Sub = GetProfilerSubsystem())
    {
        Sub->ClearEventHistory();
    }
}
