#include "UCBaseHandler.h"
#include "Editor.h"
#include "Engine/World.h"
#include "Async/Async.h"

TSharedPtr<FJsonObject> UCBaseHandler::ParseJsonBody(const FHttpServerRequest& Request)
{
    if (Request.Body.Num() == 0)
    {
        return MakeShared<FJsonObject>();
    }

    FString BodyString = FString(FUTF8ToTCHAR((const ANSICHAR*)Request.Body.GetData(), Request.Body.Num()));
    TSharedPtr<FJsonObject> JsonObject;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(BodyString);

    if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
    {
        return nullptr;
    }
    return JsonObject;
}

FString UCBaseHandler::MakeSuccessResponse(TSharedPtr<FJsonObject> Data)
{
    TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
    Root->SetBoolField(TEXT("success"), true);
    if (Data.IsValid())
    {
        Root->SetObjectField(TEXT("data"), Data);
    }

    FString Output;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Output);
    FJsonSerializer::Serialize(Root.ToSharedRef(), Writer);
    return Output;
}

FString UCBaseHandler::MakeSuccessResponse(const FString& Message)
{
    TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
    Root->SetBoolField(TEXT("success"), true);
    Root->SetStringField(TEXT("message"), Message);

    FString Output;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Output);
    FJsonSerializer::Serialize(Root.ToSharedRef(), Writer);
    return Output;
}

FString UCBaseHandler::MakeErrorResponse(const FString& Error, int32 Code)
{
    TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
    Root->SetBoolField(TEXT("success"), false);
    Root->SetStringField(TEXT("error"), Error);
    Root->SetNumberField(TEXT("code"), Code);

    FString Output;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Output);
    FJsonSerializer::Serialize(Root.ToSharedRef(), Writer);
    return Output;
}

TUniquePtr<FHttpServerResponse> UCBaseHandler::JsonResponse(const FString& Json, EHttpServerResponseCodes Code)
{
    auto Response = FHttpServerResponse::Create(Json, TEXT("application/json"));
    Response->Code = Code;
    Response->Headers.Add(TEXT("Access-Control-Allow-Origin"), {TEXT("*")});
    Response->Headers.Add(TEXT("Access-Control-Allow-Methods"), {TEXT("GET, POST, PUT, DELETE, OPTIONS")});
    Response->Headers.Add(TEXT("Access-Control-Allow-Headers"), {TEXT("Content-Type")});
    return Response;
}

void UCBaseHandler::RunOnGameThread(const FHttpResultCallback& OnComplete, TFunction<FString()> Task)
{
    auto SharedComplete = MakeShared<FHttpResultCallback>(OnComplete);
    AsyncTask(ENamedThreads::GameThread, [SharedComplete, Task = MoveTemp(Task)]() mutable
    {
        FString ResultJson;
        {
            ResultJson = Task();
        }
        auto Response = FHttpServerResponse::Create(ResultJson, TEXT("application/json"));
        Response->Headers.Add(TEXT("Access-Control-Allow-Origin"), {TEXT("*")});
        (*SharedComplete)(MoveTemp(Response));
    });
}

UWorld* UCBaseHandler::GetEditorWorld()
{
    if (GEditor)
    {
        return GEditor->GetEditorWorldContext().World();
    }
    return nullptr;
}

TSharedPtr<FJsonObject> UCBaseHandler::VectorToJson(const FVector& V)
{
    TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
    Obj->SetNumberField(TEXT("x"), V.X);
    Obj->SetNumberField(TEXT("y"), V.Y);
    Obj->SetNumberField(TEXT("z"), V.Z);
    return Obj;
}

TSharedPtr<FJsonObject> UCBaseHandler::RotatorToJson(const FRotator& R)
{
    TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
    Obj->SetNumberField(TEXT("pitch"), R.Pitch);
    Obj->SetNumberField(TEXT("yaw"), R.Yaw);
    Obj->SetNumberField(TEXT("roll"), R.Roll);
    return Obj;
}

TSharedPtr<FJsonObject> UCBaseHandler::TransformToJson(const FTransform& T)
{
    TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
    Obj->SetObjectField(TEXT("location"), VectorToJson(T.GetLocation()));
    Obj->SetObjectField(TEXT("rotation"), RotatorToJson(T.GetRotation().Rotator()));
    Obj->SetObjectField(TEXT("scale"), VectorToJson(T.GetScale3D()));
    return Obj;
}

FVector UCBaseHandler::JsonToVector(TSharedPtr<FJsonObject> Obj, const FVector& Default)
{
    if (!Obj.IsValid()) return Default;
    return FVector(
        Obj->GetNumberField(TEXT("x")),
        Obj->GetNumberField(TEXT("y")),
        Obj->GetNumberField(TEXT("z"))
    );
}

FRotator UCBaseHandler::JsonToRotator(TSharedPtr<FJsonObject> Obj, const FRotator& Default)
{
    if (!Obj.IsValid()) return Default;
    return FRotator(
        Obj->GetNumberField(TEXT("pitch")),
        Obj->GetNumberField(TEXT("yaw")),
        Obj->GetNumberField(TEXT("roll"))
    );
}
