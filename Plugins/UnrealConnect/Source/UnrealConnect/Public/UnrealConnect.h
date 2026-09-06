#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class FUnrealConnectServer;
class FUCLogDevice;

class UNREALCONNECT_API FUnrealConnectModule : public IModuleInterface
{
public:
    static FUnrealConnectModule& Get()
    {
        return FModuleManager::LoadModuleChecked<FUnrealConnectModule>("UnrealConnect");
    }

    static bool IsAvailable()
    {
        return FModuleManager::Get().IsModuleLoaded("UnrealConnect");
    }

    virtual void StartupModule() override;
    virtual void ShutdownModule() override;

    // Server control (called from UI panel)
    void StartServer();
    void StopServer();

    FUnrealConnectServer* GetServer() const { return Server.Get(); }
    FUCLogDevice*         GetLogDevice() const { return LogDevice.Get(); }

private:
    void RegisterTabSpawner();
    void UnregisterTabSpawner();
    TSharedRef<class SDockTab> OnSpawnTab(const class FSpawnTabArgs& Args);

    TUniquePtr<FUnrealConnectServer> Server;
    TUniquePtr<FUCLogDevice>         LogDevice;

    static const FName ServerTabName;
};
