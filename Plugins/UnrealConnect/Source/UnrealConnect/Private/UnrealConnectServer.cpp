#include "UnrealConnectServer.h"
#include "UCLogDevice.h"
#include "UCBaseHandler.h"
#include "Handlers/UCActorHandler.h"
#include "Handlers/UCBlueprintHandler.h"
#include "Handlers/UCMaterialHandler.h"
#include "Handlers/UCLevelHandler.h"
#include "Handlers/UCEditorHandler.h"
#include "Handlers/UCAssetHandler.h"
#include "Handlers/UCComponentHandler.h"
#include "Handlers/UCGASDiscoveryHandler.h"
#include "HttpServerModule.h"
#include "HttpServerResponse.h"
#include "HttpPath.h"

FUnrealConnectServer::FUnrealConnectServer()
{
}

FUnrealConnectServer::~FUnrealConnectServer()
{
    Stop();
}

bool FUnrealConnectServer::Start(uint32 Port)
{
    ListenPort = Port;

    FHttpServerModule& HttpServerModule = FHttpServerModule::Get();
    HttpRouter = HttpServerModule.GetHttpRouter(Port);

    if (!HttpRouter.IsValid())
    {
        UE_LOG(LogUnrealConnect, Error, TEXT("[UnrealConnect] Failed to get HTTP router for port %d"), Port);
        return false;
    }

    // Register ping/health route
    RegisterPingRoute(HttpRouter.ToSharedRef());

    // Register all domain handlers
    Handlers.Add(MakeShared<UCActorHandler>());
    Handlers.Add(MakeShared<UCBlueprintHandler>());
    Handlers.Add(MakeShared<UCMaterialHandler>());
    Handlers.Add(MakeShared<UCLevelHandler>());
    Handlers.Add(MakeShared<UCEditorHandler>());
    Handlers.Add(MakeShared<UCAssetHandler>());
    Handlers.Add(MakeShared<UCComponentHandler>());
    Handlers.Add(MakeShared<UCGASDiscoveryHandler>());

    for (auto& Handler : Handlers)
    {
        Handler->RegisterRoutes(HttpRouter.ToSharedRef());
    }

    HttpServerModule.StartAllListeners();
    bIsRunning = true;
    return true;
}

void FUnrealConnectServer::Stop()
{
    if (bIsRunning)
    {
        FHttpServerModule::Get().StopAllListeners();
        RouteHandles.Empty();
        Handlers.Empty();
        HttpRouter.Reset();
        bIsRunning = false;
    }
}

void FUnrealConnectServer::RegisterPingRoute(TSharedRef<IHttpRouter> Router)
{
    Router->BindRoute(
        FHttpPath(TEXT("/ping")),
        EHttpServerRequestVerbs::VERB_GET,
        FHttpRequestHandler::CreateLambda([](const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
        {
            TSharedPtr<FJsonObject> Json = MakeShared<FJsonObject>();
            Json->SetStringField(TEXT("status"), TEXT("ok"));
            Json->SetStringField(TEXT("server"), TEXT("UnrealConnect"));
            Json->SetStringField(TEXT("version"), TEXT("1.0"));

            FString Output;
            TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Output);
            FJsonSerializer::Serialize(Json.ToSharedRef(), Writer);

            auto Response = FHttpServerResponse::Create(Output, TEXT("application/json"));
            Response->Headers.Add(TEXT("Access-Control-Allow-Origin"), {TEXT("*")});
            OnComplete(MoveTemp(Response));
            return true;
        })
    );
}
