#pragma once

#include "CoreMinimal.h"
#include "IHttpRouter.h"
#include "HttpRouteHandle.h"

class UCBaseHandler;

class UNREALCONNECT_API FUnrealConnectServer
{
public:
    FUnrealConnectServer();
    ~FUnrealConnectServer();

    bool Start(uint32 Port = 8765);
    void Stop();

    bool IsRunning() const { return bIsRunning; }
    uint32 GetPort() const { return ListenPort; }

private:
    void RegisterPingRoute(TSharedRef<IHttpRouter> Router);
    void AddCorsHeaders(TUniquePtr<FHttpServerResponse>& Response);

    TSharedPtr<IHttpRouter> HttpRouter;
    TArray<TSharedPtr<UCBaseHandler>> Handlers;
    TArray<FHttpRouteHandle> RouteHandles;

    uint32 ListenPort = 8765;
    bool bIsRunning = false;
};
