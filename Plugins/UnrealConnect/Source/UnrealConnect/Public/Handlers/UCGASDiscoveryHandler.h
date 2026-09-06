// Plugins/UnrealConnect/Source/UnrealConnect/Public/Handlers/UCGASDiscoveryHandler.h
#pragma once

#include "CoreMinimal.h"
#include "UCBaseHandler.h"

// GAS Profiler endpoints (all under /gas-profiler/):
//   GET  /actor-path       — UObject path of AGASProfilerActor in PIE world
//   GET  /snapshot         — full FGASProfilerSnapshot as JSON
//   POST /force-activate   — { "tag": "..." }
//   POST /set-attribute    — { "name": "...", "value": 0.0 }
//   POST /clear-log        — clears event history
class UNREALCONNECT_API UCGASDiscoveryHandler : public UCBaseHandler
{
public:
    void RegisterRoutes(TSharedRef<IHttpRouter> Router) override;
};
