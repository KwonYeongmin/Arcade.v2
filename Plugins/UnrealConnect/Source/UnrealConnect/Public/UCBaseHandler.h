#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonReader.h"
#include "HttpServerRequest.h"
#include "HttpServerResponse.h"
#include "IHttpRouter.h"

// UE5.7: BindRoute expects FHttpRequestHandler - use this alias so lambdas convert correctly
using FUCRouteHandler = FHttpRequestHandler;

// Helper macro to dispatch to game thread and call OnComplete when done
#define UC_GAME_THREAD_RESPONSE(OnComplete, Body) \
    do { \
        auto _SharedComplete = MakeShared<FHttpResultCallback>(MoveTemp(const_cast<FHttpResultCallback&>(OnComplete))); \
        FString _Body = (Body); \
        AsyncTask(ENamedThreads::GameThread, [_SharedComplete, _Body]() mutable { \
            auto _Resp = FHttpServerResponse::Create(_Body, TEXT("application/json")); \
            (*_SharedComplete)(MoveTemp(_Resp)); \
        }); \
    } while(0)

class UNREALCONNECT_API UCBaseHandler
{
public:
    virtual ~UCBaseHandler() = default;
    virtual void RegisterRoutes(TSharedRef<IHttpRouter> Router) = 0;

protected:
    // Parse JSON body from request
    static TSharedPtr<FJsonObject> ParseJsonBody(const FHttpServerRequest& Request);

    // Build success JSON response string
    static FString MakeSuccessResponse(TSharedPtr<FJsonObject> Data);
    static FString MakeSuccessResponse(const FString& Message);

    // Build error JSON response string
    static FString MakeErrorResponse(const FString& Error, int32 Code = 400);

    // Create HTTP response with JSON content
    static TUniquePtr<FHttpServerResponse> JsonResponse(const FString& Json, EHttpServerResponseCodes Code = EHttpServerResponseCodes::Ok);

    // Safely run a task on the game thread and complete the request
    static void RunOnGameThread(
        const FHttpResultCallback& OnComplete,
        TFunction<FString()> Task
    );

    // Get editor world
    static UWorld* GetEditorWorld();

    // Serialize vector to JSON object
    static TSharedPtr<FJsonObject> VectorToJson(const FVector& V);
    static TSharedPtr<FJsonObject> RotatorToJson(const FRotator& R);
    static TSharedPtr<FJsonObject> TransformToJson(const FTransform& T);

    // Deserialize JSON to vector/rotator
    static FVector JsonToVector(TSharedPtr<FJsonObject> Obj, const FVector& Default = FVector::ZeroVector);
    static FRotator JsonToRotator(TSharedPtr<FJsonObject> Obj, const FRotator& Default = FRotator::ZeroRotator);
};
