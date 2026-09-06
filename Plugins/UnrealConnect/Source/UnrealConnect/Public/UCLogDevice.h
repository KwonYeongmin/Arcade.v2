#pragma once

#include "CoreMinimal.h"
#include "Misc/OutputDevice.h"

DECLARE_LOG_CATEGORY_EXTERN(LogUnrealConnect, Log, All);

/**
 * Custom output device that captures all LogUnrealConnect log messages.
 * Registered with GLog on construction, unregistered on destruction.
 */
class UNREALCONNECT_API FUCLogDevice : public FOutputDevice
{
public:
    FUCLogDevice();
    virtual ~FUCLogDevice() override;

    virtual void Serialize(const TCHAR* V, ELogVerbosity::Type Verbosity, const FName& Category) override;
    virtual bool CanBeUsedOnAnyThread() const override { return true; }

    TArray<TSharedPtr<FString>> GetLogs() const;
    int32 GetLogCount() const;
    void Clear();

private:
    mutable FCriticalSection Lock;
    TArray<TSharedPtr<FString>> Logs;

    static constexpr int32 MaxLogEntries = 500;
};
