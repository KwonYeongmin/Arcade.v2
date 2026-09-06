// Plugins/UnrealConnect/Source/UnrealConnect/Private/Handlers/UCGASDiscoveryHandler.cpp
#include "Handlers/UCGASDiscoveryHandler.h"
#include "HttpPath.h"
#include "HttpServerRequest.h"
#include "HttpServerResponse.h"
#include "IHttpRouter.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Async/Async.h"

#include "AbilitySystem/Profiler/GASProfilerSubsystem.h"
#include "AbilitySystem/Profiler/GASProfilerTypes.h"

// ── Shared helpers ────────────────────────────────────────────────────────────

namespace
{
    FString SerializeJson(TSharedPtr<FJsonObject> Obj)
    {
        FString Out;
        TSharedRef<TJsonWriter<>> W = TJsonWriterFactory<>::Create(&Out);
        FJsonSerializer::Serialize(Obj.ToSharedRef(), W);
        return Out;
    }

    void SendJsonResponse(
        const TSharedPtr<FHttpResultCallback>& SharedComplete,
        const FString& Json,
        EHttpServerResponseCodes Code = EHttpServerResponseCodes::Ok)
    {
        auto Resp = FHttpServerResponse::Create(Json, TEXT("application/json"));
        Resp->Code = Code;
        Resp->Headers.Add(TEXT("Access-Control-Allow-Origin"),  { TEXT("*") });
        Resp->Headers.Add(TEXT("Access-Control-Allow-Methods"), { TEXT("GET, POST, OPTIONS") });
        Resp->Headers.Add(TEXT("Access-Control-Allow-Headers"), { TEXT("Content-Type") });
        (*SharedComplete)(MoveTemp(Resp));
    }

    FString MakeError(const FString& Msg)
    {
        TSharedPtr<FJsonObject> O = MakeShared<FJsonObject>();
        O->SetBoolField(TEXT("success"), false);
        O->SetStringField(TEXT("error"), Msg);
        return SerializeJson(O);
    }

    // Find the first PIE world currently running
    UWorld* FindPIEWorld()
    {
        if (!GEngine) return nullptr;
        for (const FWorldContext& Ctx : GEngine->GetWorldContexts())
        {
            if (Ctx.WorldType == EWorldType::PIE && Ctx.World())
                return Ctx.World();
        }
        return nullptr;
    }

    // Serialize FGASProfilerSnapshot → JSON data object
    TSharedPtr<FJsonObject> SnapshotToJson(const FGASProfilerSnapshot& Snap)
    {
        TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();

        // Attributes
        TArray<TSharedPtr<FJsonValue>> AttrArr;
        for (const FGASAttributeInfo& A : Snap.Attributes)
        {
            TSharedPtr<FJsonObject> O = MakeShared<FJsonObject>();
            O->SetStringField(TEXT("Name"),         A.Name);
            O->SetNumberField(TEXT("CurrentValue"),  A.CurrentValue);
            O->SetNumberField(TEXT("BaseValue"),     A.BaseValue);
            AttrArr.Add(MakeShared<FJsonValueObject>(O));
        }
        Data->SetArrayField(TEXT("Attributes"), AttrArr);

        // ActiveAbilities
        TArray<TSharedPtr<FJsonValue>> AbilArr;
        for (const FGASAbilityInfo& A : Snap.ActiveAbilities)
        {
            TSharedPtr<FJsonObject> O = MakeShared<FJsonObject>();
            O->SetStringField(TEXT("ClassName"),        A.ClassName);
            O->SetStringField(TEXT("ActivationGroup"),  A.ActivationGroup);
            O->SetStringField(TEXT("OwnedTags"),        A.OwnedTags);
            O->SetBoolField(  TEXT("bIsActive"),        A.bIsActive);
            AbilArr.Add(MakeShared<FJsonValueObject>(O));
        }
        Data->SetArrayField(TEXT("ActiveAbilities"), AbilArr);

        // ActiveEffects
        TArray<TSharedPtr<FJsonValue>> EffArr;
        for (const FGASEffectInfo& E : Snap.ActiveEffects)
        {
            TSharedPtr<FJsonObject> O = MakeShared<FJsonObject>();
            O->SetStringField(TEXT("ClassName"),     E.ClassName);
            O->SetNumberField(TEXT("Duration"),      E.Duration);
            O->SetNumberField(TEXT("TimeRemaining"), E.TimeRemaining);
            O->SetNumberField(TEXT("StackCount"),    (double)E.StackCount);
            EffArr.Add(MakeShared<FJsonValueObject>(O));
        }
        Data->SetArrayField(TEXT("ActiveEffects"), EffArr);

        // EventHistory
        TArray<TSharedPtr<FJsonValue>> EvtArr;
        for (const FGASEventEntry& E : Snap.EventHistory)
        {
            TSharedPtr<FJsonObject> O = MakeShared<FJsonObject>();
            O->SetNumberField(TEXT("Timestamp"),    E.Timestamp);
            O->SetStringField(TEXT("EventType"),    E.EventType);
            O->SetStringField(TEXT("AbilityOrTag"), E.AbilityOrTag);
            O->SetStringField(TEXT("Detail"),       E.Detail);
            EvtArr.Add(MakeShared<FJsonValueObject>(O));
        }
        Data->SetArrayField(TEXT("EventHistory"), EvtArr);

        return Data;
    }

    // Build a CORS-only OPTIONS response (preflight)
    TUniquePtr<FHttpServerResponse> MakeCORSResponse()
    {
        auto Resp = FHttpServerResponse::Create(TEXT(""), TEXT("text/plain"));
        Resp->Code = EHttpServerResponseCodes::NoContent;
        Resp->Headers.Add(TEXT("Access-Control-Allow-Origin"),  { TEXT("*") });
        Resp->Headers.Add(TEXT("Access-Control-Allow-Methods"), { TEXT("GET, POST, OPTIONS") });
        Resp->Headers.Add(TEXT("Access-Control-Allow-Headers"), { TEXT("Content-Type") });
        return Resp;
    }
}

// ── Route registration ────────────────────────────────────────────────────────

void UCGASDiscoveryHandler::RegisterRoutes(TSharedRef<IHttpRouter> Router)
{
    // ─────────────────────────────────────────────────────────────────────────
    // GET /gas-profiler/actor-path
    // Returns the UObject path of AGASProfilerActor in the current PIE world.
    // ─────────────────────────────────────────────────────────────────────────
    Router->BindRoute(
        FHttpPath(TEXT("/gas-profiler/actor-path")),
        EHttpServerRequestVerbs::VERB_GET,
        FUCRouteHandler::CreateLambda([](const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete) -> bool
        {
            auto SC = MakeShared<FHttpResultCallback>(MoveTemp(const_cast<FHttpResultCallback&>(OnComplete)));

            AsyncTask(ENamedThreads::GameThread, [SC]()
            {
                UWorld* PIEWorld = FindPIEWorld();
                if (!PIEWorld)
                {
                    SendJsonResponse(SC, TEXT("PIE not running"), EHttpServerResponseCodes::NotFound);
                    return;
                }

                for (TActorIterator<AActor> It(PIEWorld); It; ++It)
                {
#if WITH_EDITOR
                    if (It->GetActorLabel() == TEXT("GASProfiler"))
#else
                    if (It->GetFName() == FName(TEXT("GASProfiler")))
#endif
                    {
                        TSharedPtr<FJsonObject> Payload = MakeShared<FJsonObject>();
                        Payload->SetStringField(TEXT("objectPath"), It->GetPathName());

                        TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
                        Root->SetBoolField(TEXT("success"), true);
                        Root->SetObjectField(TEXT("data"), Payload);

                        SendJsonResponse(SC, SerializeJson(Root), EHttpServerResponseCodes::Ok);
                        return;
                    }
                }

                SendJsonResponse(SC, TEXT("GASProfiler actor not found. Is PIE running?"), EHttpServerResponseCodes::NotFound);
            });

            return true;
        })
    );

    // ─────────────────────────────────────────────────────────────────────────
    // GET /gas-profiler/snapshot
    // Returns the current GASProfilerSnapshot as JSON.
    // ─────────────────────────────────────────────────────────────────────────
    Router->BindRoute(
        FHttpPath(TEXT("/gas-profiler/snapshot")),
        EHttpServerRequestVerbs::VERB_GET,
        FUCRouteHandler::CreateLambda([](const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete) -> bool
        {
            auto SC = MakeShared<FHttpResultCallback>(MoveTemp(const_cast<FHttpResultCallback&>(OnComplete)));

            AsyncTask(ENamedThreads::GameThread, [SC]()
            {
                UWorld* PIEWorld = FindPIEWorld();
                if (!PIEWorld)
                {
                    SendJsonResponse(SC, TEXT("PIE not running"), EHttpServerResponseCodes::NotFound);
                    return;
                }

                UGASProfilerSubsystem* Sub = PIEWorld->GetSubsystem<UGASProfilerSubsystem>();
                if (!Sub)
                {
                    SendJsonResponse(SC, TEXT("GASProfilerSubsystem not found"), EHttpServerResponseCodes::NotFound);
                    return;
                }

                FGASProfilerSnapshot Snap = Sub->GetSnapshot();

                TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
                Root->SetBoolField(TEXT("success"), true);
                Root->SetObjectField(TEXT("data"), SnapshotToJson(Snap));

                SendJsonResponse(SC, SerializeJson(Root), EHttpServerResponseCodes::Ok);
            });

            return true;
        })
    );

    // ─────────────────────────────────────────────────────────────────────────
    // OPTIONS /gas-profiler/force-activate  (CORS preflight)
    // POST    /gas-profiler/force-activate  { "tag": "Lyra.Ability.Jump" }
    // ─────────────────────────────────────────────────────────────────────────
    Router->BindRoute(
        FHttpPath(TEXT("/gas-profiler/force-activate")),
        EHttpServerRequestVerbs::VERB_OPTIONS,
        FUCRouteHandler::CreateLambda([](const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete) -> bool
        {
            OnComplete(MakeCORSResponse());
            return true;
        })
    );

    Router->BindRoute(
        FHttpPath(TEXT("/gas-profiler/force-activate")),
        EHttpServerRequestVerbs::VERB_POST,
        FUCRouteHandler::CreateLambda([](const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete) -> bool
        {
            auto SC = MakeShared<FHttpResultCallback>(MoveTemp(const_cast<FHttpResultCallback&>(OnComplete)));

            // Parse body on the calling thread (body is already available)
            FString Tag;
            {
                const TArray<uint8>& Body = Request.Body;
                FString BodyStr = FString(UTF8_TO_TCHAR(reinterpret_cast<const ANSICHAR*>(Body.GetData())));
                TSharedPtr<FJsonObject> Json;
                TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(BodyStr);
                if (FJsonSerializer::Deserialize(Reader, Json) && Json.IsValid())
                {
                    Json->TryGetStringField(TEXT("tag"), Tag);
                }
            }

            AsyncTask(ENamedThreads::GameThread, [SC, Tag]()
            {
                UWorld* PIEWorld = FindPIEWorld();
                if (!PIEWorld)
                {
                    SendJsonResponse(SC, TEXT("PIE not running"), EHttpServerResponseCodes::NotFound);
                    return;
                }

                UGASProfilerSubsystem* Sub = PIEWorld->GetSubsystem<UGASProfilerSubsystem>();
                if (!Sub)
                {
                    SendJsonResponse(SC, TEXT("GASProfilerSubsystem not found"), EHttpServerResponseCodes::NotFound);
                    return;
                }

                Sub->ForceActivateAbility(Tag);

                TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
                Root->SetBoolField(TEXT("success"), true);
                Root->SetStringField(TEXT("message"), FString::Printf(TEXT("ForceActivate sent: %s"), *Tag));
                SendJsonResponse(SC, SerializeJson(Root), EHttpServerResponseCodes::Ok);
            });

            return true;
        })
    );

    // ─────────────────────────────────────────────────────────────────────────
    // OPTIONS /gas-profiler/set-attribute  (CORS preflight)
    // POST    /gas-profiler/set-attribute  { "name": "LyraHealthSet.Health", "value": 100.0 }
    // ─────────────────────────────────────────────────────────────────────────
    Router->BindRoute(
        FHttpPath(TEXT("/gas-profiler/set-attribute")),
        EHttpServerRequestVerbs::VERB_OPTIONS,
        FUCRouteHandler::CreateLambda([](const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete) -> bool
        {
            OnComplete(MakeCORSResponse());
            return true;
        })
    );

    Router->BindRoute(
        FHttpPath(TEXT("/gas-profiler/set-attribute")),
        EHttpServerRequestVerbs::VERB_POST,
        FUCRouteHandler::CreateLambda([](const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete) -> bool
        {
            auto SC = MakeShared<FHttpResultCallback>(MoveTemp(const_cast<FHttpResultCallback&>(OnComplete)));

            FString AttrName;
            float AttrValue = 0.f;
            {
                const TArray<uint8>& Body = Request.Body;
                FString BodyStr = FString(UTF8_TO_TCHAR(reinterpret_cast<const ANSICHAR*>(Body.GetData())));
                TSharedPtr<FJsonObject> Json;
                TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(BodyStr);
                if (FJsonSerializer::Deserialize(Reader, Json) && Json.IsValid())
                {
                    Json->TryGetStringField(TEXT("name"), AttrName);
                    double Tmp = 0.0;
                    if (Json->TryGetNumberField(TEXT("value"), Tmp))
                        AttrValue = (float)Tmp;
                }
            }

            AsyncTask(ENamedThreads::GameThread, [SC, AttrName, AttrValue]()
            {
                UWorld* PIEWorld = FindPIEWorld();
                if (!PIEWorld)
                {
                   SendJsonResponse(SC, TEXT("PIE not running"), EHttpServerResponseCodes::NotFound);
                    return;
                }

                UGASProfilerSubsystem* Sub = PIEWorld->GetSubsystem<UGASProfilerSubsystem>();
                if (!Sub)
                {
                    SendJsonResponse(SC, TEXT("GASProfilerSubsystem not found"), EHttpServerResponseCodes::NotFound);
                    return;
                }

                Sub->SetAttributeValue(AttrName, AttrValue);

                TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
                Root->SetBoolField(TEXT("success"), true);
                Root->SetStringField(TEXT("message"), FString::Printf(TEXT("SetAttribute: %s = %f"), *AttrName, AttrValue));
                SendJsonResponse(SC, SerializeJson(Root), EHttpServerResponseCodes::Ok);
            });

            return true;
        })
    );

    // ─────────────────────────────────────────────────────────────────────────
    // OPTIONS /gas-profiler/clear-log  (CORS preflight)
    // POST    /gas-profiler/clear-log  (no body)
    // ─────────────────────────────────────────────────────────────────────────
    Router->BindRoute(
        FHttpPath(TEXT("/gas-profiler/clear-log")),
        EHttpServerRequestVerbs::VERB_OPTIONS,
        FUCRouteHandler::CreateLambda([](const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete) -> bool
        {
            OnComplete(MakeCORSResponse());
            return true;
        })
    );

    Router->BindRoute(
        FHttpPath(TEXT("/gas-profiler/clear-log")),
        EHttpServerRequestVerbs::VERB_POST,
        FUCRouteHandler::CreateLambda([](const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete) -> bool
        {
            auto SC = MakeShared<FHttpResultCallback>(MoveTemp(const_cast<FHttpResultCallback&>(OnComplete)));

            AsyncTask(ENamedThreads::GameThread, [SC]()
            {
                UWorld* PIEWorld = FindPIEWorld();
                if (!PIEWorld)
                {
                    SendJsonResponse(SC, TEXT("PIE not running"), EHttpServerResponseCodes::NotFound);
                    return;
                }

                UGASProfilerSubsystem* Sub = PIEWorld->GetSubsystem<UGASProfilerSubsystem>();
                if (!Sub)
                {
                    SendJsonResponse(SC, TEXT("GASProfilerSubsystem not found"), EHttpServerResponseCodes::NotFound);
                    return;
                }

                Sub->ClearEventHistory();

                TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
                Root->SetBoolField(TEXT("success"), true);
                Root->SetStringField(TEXT("message"), TEXT("Event history cleared"));
                SendJsonResponse(SC, SerializeJson(Root), EHttpServerResponseCodes::Ok);
            });

            return true;
        })
    );
}
