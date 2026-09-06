#include "UnrealConnect.h"
#include "UnrealConnectServer.h"
#include "UCLogDevice.h"
#include "UCServerPanel.h"
#include "Modules/ModuleManager.h"
#include "Framework/Docking/TabManager.h"
#include "Widgets/Docking/SDockTab.h"
#include "Styling/AppStyle.h"

#define LOCTEXT_NAMESPACE "FUnrealConnectModule"

const FName FUnrealConnectModule::ServerTabName("UnrealConnectServer");

void FUnrealConnectModule::StartupModule()
{
    // Start log capture immediately so we don't miss early messages
    LogDevice = MakeUnique<FUCLogDevice>();

    // Register the editor tab - server is NOT auto-started; user controls it via the panel
    RegisterTabSpawner();

    UE_LOG(LogUnrealConnect, Log, TEXT("UnrealConnect module loaded. Open 'Window > UnrealConnect Server' to start."));
}

void FUnrealConnectModule::ShutdownModule()
{
    UnregisterTabSpawner();

    if (Server.IsValid() && Server->IsRunning())
    {
        Server->Stop();
        Server.Reset();
    }

    // LogDevice destructor unregisters itself from GLog
    LogDevice.Reset();
}

void FUnrealConnectModule::StartServer()
{
    if (Server.IsValid() && Server->IsRunning())
    {
        UE_LOG(LogUnrealConnect, Warning, TEXT("Server is already running on port %d"), Server->GetPort());
        return;
    }

    Server = MakeUnique<FUnrealConnectServer>();
    if (Server->Start(8765))
    {
        UE_LOG(LogUnrealConnect, Log, TEXT("HTTP server started on port 8765"));
        UE_LOG(LogUnrealConnect, Log, TEXT("MCP endpoint: http://localhost:8765"));
    }
    else
    {
        UE_LOG(LogUnrealConnect, Error, TEXT("Failed to start HTTP server on port 8765"));
        Server.Reset();
    }
}

void FUnrealConnectModule::StopServer()
{
    if (!Server.IsValid() || !Server->IsRunning())
    {
        UE_LOG(LogUnrealConnect, Warning, TEXT("Server is not running"));
        return;
    }

    Server->Stop();
    Server.Reset();
    UE_LOG(LogUnrealConnect, Log, TEXT("HTTP server stopped"));
}

void FUnrealConnectModule::RegisterTabSpawner()
{
    FGlobalTabmanager::Get()->RegisterNomadTabSpawner(
        ServerTabName,
        FOnSpawnTab::CreateRaw(this, &FUnrealConnectModule::OnSpawnTab))
        .SetDisplayName(LOCTEXT("TabTitle", "UnrealConnect Server"))
        .SetTooltipText(LOCTEXT("TabTooltip", "Control the UnrealConnect HTTP server and view logs"))
        .SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Details"))
        .SetMenuType(ETabSpawnerMenuType::Enabled);
}

void FUnrealConnectModule::UnregisterTabSpawner()
{
    FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(ServerTabName);
}

TSharedRef<SDockTab> FUnrealConnectModule::OnSpawnTab(const FSpawnTabArgs& Args)
{
    return SNew(SDockTab)
        .TabRole(ETabRole::NomadTab)
        .Label(LOCTEXT("TabTitle", "UnrealConnect Server"))
        [
            SNew(SUCServerPanel)
        ];
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FUnrealConnectModule, UnrealConnect)
