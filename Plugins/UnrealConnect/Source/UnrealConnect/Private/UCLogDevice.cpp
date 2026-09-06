#include "UCLogDevice.h"
#include "Misc/OutputDeviceRedirector.h"

DEFINE_LOG_CATEGORY(LogUnrealConnect);

FUCLogDevice::FUCLogDevice()
{
    GLog->AddOutputDevice(this);
}

FUCLogDevice::~FUCLogDevice()
{
    GLog->RemoveOutputDevice(this);
}

void FUCLogDevice::Serialize(const TCHAR* V, ELogVerbosity::Type Verbosity, const FName& Category)
{
    if (Category != TEXT("LogUnrealConnect")) return;

    FString Prefix;
    switch (Verbosity)
    {
        case ELogVerbosity::Error:   Prefix = TEXT("[ERROR] "); break;
        case ELogVerbosity::Warning: Prefix = TEXT("[WARN]  "); break;
        default:                     Prefix = TEXT("[LOG]   "); break;
    }

    FScopeLock ScopeLock(&Lock);
    Logs.Add(MakeShared<FString>(Prefix + FString(V)));

    if (Logs.Num() > MaxLogEntries)
    {
        Logs.RemoveAt(0, Logs.Num() - MaxLogEntries);
    }
}

TArray<TSharedPtr<FString>> FUCLogDevice::GetLogs() const
{
    FScopeLock ScopeLock(&Lock);
    return Logs;
}

int32 FUCLogDevice::GetLogCount() const
{
    FScopeLock ScopeLock(&Lock);
    return Logs.Num();
}

void FUCLogDevice::Clear()
{
    FScopeLock ScopeLock(&Lock);
    Logs.Empty();
}
