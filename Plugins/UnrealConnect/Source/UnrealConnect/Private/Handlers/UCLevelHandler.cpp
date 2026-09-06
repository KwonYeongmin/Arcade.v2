#include "Handlers/UCLevelHandler.h"
#include "Engine/World.h"
#include "Engine/Level.h"
#include "Engine/LevelStreaming.h"
#include "Editor.h"
#include "EditorLevelUtils.h"
#include "FileHelpers.h"
#include "LevelEditor.h"
#include "HttpPath.h"
#include "EngineUtils.h"
#include "Engine/LevelStreamingDynamic.h"

void UCLevelHandler::RegisterRoutes(TSharedRef<IHttpRouter> Router)
{
    Router->BindRoute(FHttpPath(TEXT("/level/info")), EHttpServerRequestVerbs::VERB_GET,
        FHttpRequestHandler::CreateLambda([this](const FHttpServerRequest& Req, const FHttpResultCallback& OnComplete) -> bool { return HandleGetLevelInfo(Req, OnComplete); }));

    Router->BindRoute(FHttpPath(TEXT("/level/save")), EHttpServerRequestVerbs::VERB_POST,
        FHttpRequestHandler::CreateLambda([this](const FHttpServerRequest& Req, const FHttpResultCallback& OnComplete) -> bool { return HandleSaveLevel(Req, OnComplete); }));

    Router->BindRoute(FHttpPath(TEXT("/level/open")), EHttpServerRequestVerbs::VERB_POST,
        FHttpRequestHandler::CreateLambda([this](const FHttpServerRequest& Req, const FHttpResultCallback& OnComplete) -> bool { return HandleOpenLevel(Req, OnComplete); }));

    Router->BindRoute(FHttpPath(TEXT("/level/new")), EHttpServerRequestVerbs::VERB_POST,
        FHttpRequestHandler::CreateLambda([this](const FHttpServerRequest& Req, const FHttpResultCallback& OnComplete) -> bool { return HandleNewLevel(Req, OnComplete); }));

    Router->BindRoute(FHttpPath(TEXT("/level/streaming")), EHttpServerRequestVerbs::VERB_GET,
        FHttpRequestHandler::CreateLambda([this](const FHttpServerRequest& Req, const FHttpResultCallback& OnComplete) -> bool { return HandleGetStreamingLevels(Req, OnComplete); }));

    Router->BindRoute(FHttpPath(TEXT("/level/streaming")), EHttpServerRequestVerbs::VERB_POST,
        FHttpRequestHandler::CreateLambda([this](const FHttpServerRequest& Req, const FHttpResultCallback& OnComplete) -> bool { return HandleAddStreamingLevel(Req, OnComplete); }));

    Router->BindRoute(FHttpPath(TEXT("/level/build-lighting")), EHttpServerRequestVerbs::VERB_POST,
        FHttpRequestHandler::CreateLambda([this](const FHttpServerRequest& Req, const FHttpResultCallback& OnComplete) -> bool { return HandleBuildLighting(Req, OnComplete); }));

    Router->BindRoute(FHttpPath(TEXT("/level/bounds")), EHttpServerRequestVerbs::VERB_GET,
        FHttpRequestHandler::CreateLambda([this](const FHttpServerRequest& Req, const FHttpResultCallback& OnComplete) -> bool { return HandleGetLevelBounds(Req, OnComplete); }));

    Router->BindRoute(FHttpPath(TEXT("/level/save-all")), EHttpServerRequestVerbs::VERB_POST,
        FHttpRequestHandler::CreateLambda([this](const FHttpServerRequest& Req, const FHttpResultCallback& OnComplete) -> bool { return HandleSaveAll(Req, OnComplete); }));
}

bool UCLevelHandler::HandleGetLevelInfo(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
{
    RunOnGameThread(OnComplete, []() -> FString
    {
        UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
        if (!World) return MakeErrorResponse(TEXT("No editor world"));

        TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
        Data->SetStringField(TEXT("name"), World->GetName());
        Data->SetStringField(TEXT("path"), World->GetPathName());
        Data->SetStringField(TEXT("package"), World->GetOutermost()->GetName());

        int32 ActorCount = 0;
        for (TActorIterator<AActor> It(World); It; ++It) ++ActorCount;
        Data->SetNumberField(TEXT("actor_count"), ActorCount);

        Data->SetBoolField(TEXT("is_dirty"), World->GetOutermost()->IsDirty());

        TArray<TSharedPtr<FJsonValue>> StreamingArr;
        for (ULevelStreaming* SL : World->GetStreamingLevels())
        {
            if (!SL) continue;
            TSharedPtr<FJsonObject> SLObj = MakeShared<FJsonObject>();
            SLObj->SetStringField(TEXT("name"), SL->GetWorldAssetPackageName());
            SLObj->SetBoolField(TEXT("visible"), SL->GetShouldBeVisibleInEditor());
            StreamingArr.Add(MakeShared<FJsonValueObject>(SLObj));
        }
        Data->SetArrayField(TEXT("streaming_levels"), StreamingArr);

        return MakeSuccessResponse(Data);
    });
    return true;
}

bool UCLevelHandler::HandleSaveLevel(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
{
    RunOnGameThread(OnComplete, []() -> FString
    {
        UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
        if (!World) return MakeErrorResponse(TEXT("No editor world"));

        bool bSaved = FEditorFileUtils::SaveLevel(World->PersistentLevel);
        if (!bSaved) return MakeErrorResponse(TEXT("Failed to save level"));

        return MakeSuccessResponse(FString::Printf(TEXT("Level '%s' saved"), *World->GetName()));
    });
    return true;
}

bool UCLevelHandler::HandleOpenLevel(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
{
    TSharedPtr<FJsonObject> Body = ParseJsonBody(Request);

    RunOnGameThread(OnComplete, [Body]() -> FString
    {
        FString LevelPath = Body->GetStringField(TEXT("path"));
        if (LevelPath.IsEmpty()) return MakeErrorResponse(TEXT("path is required"));

        bool bOpened = FEditorFileUtils::LoadMap(LevelPath);
        if (!bOpened) return MakeErrorResponse(FString::Printf(TEXT("Failed to open level: %s"), *LevelPath));

        return MakeSuccessResponse(FString::Printf(TEXT("Level '%s' opened"), *LevelPath));
    });
    return true;
}

bool UCLevelHandler::HandleNewLevel(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
{
    TSharedPtr<FJsonObject> Body = ParseJsonBody(Request);

    RunOnGameThread(OnComplete, [Body]() -> FString
    {
        FString TemplatePath;
        Body->TryGetStringField(TEXT("template"), TemplatePath);

        bool bCreated = FEditorFileUtils::LoadMap();
        if (!bCreated) return MakeErrorResponse(TEXT("Failed to create new level"));

        return MakeSuccessResponse(TEXT("New level created"));
    });
    return true;
}

bool UCLevelHandler::HandleGetStreamingLevels(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
{
    RunOnGameThread(OnComplete, []() -> FString
    {
        UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
        if (!World) return MakeErrorResponse(TEXT("No editor world"));

        TArray<TSharedPtr<FJsonValue>> StreamingArr;
        for (ULevelStreaming* SL : World->GetStreamingLevels())
        {
            if (!SL) continue;
            TSharedPtr<FJsonObject> SLObj = MakeShared<FJsonObject>();
            SLObj->SetStringField(TEXT("name"), SL->GetWorldAssetPackageName());
            SLObj->SetStringField(TEXT("class"), SL->GetClass()->GetName());
            SLObj->SetBoolField(TEXT("visible"), SL->GetShouldBeVisibleInEditor());
            SLObj->SetBoolField(TEXT("loaded"), SL->IsLevelLoaded());
            StreamingArr.Add(MakeShared<FJsonValueObject>(SLObj));
        }

        TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
        Data->SetArrayField(TEXT("streaming_levels"), StreamingArr);
        Data->SetNumberField(TEXT("count"), StreamingArr.Num());
        return MakeSuccessResponse(Data);
    });
    return true;
}

bool UCLevelHandler::HandleAddStreamingLevel(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
{
    TSharedPtr<FJsonObject> Body = ParseJsonBody(Request);

    RunOnGameThread(OnComplete, [Body]() -> FString
    {
        UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
        if (!World) return MakeErrorResponse(TEXT("No editor world"));

        FString LevelPath = Body->GetStringField(TEXT("path"));
        if (LevelPath.IsEmpty()) return MakeErrorResponse(TEXT("path is required"));


        ULevelStreaming* StreamingLevel = EditorLevelUtils::AddLevelToWorld(
            World, *LevelPath, ULevelStreamingDynamic::StaticClass());

        if (!StreamingLevel) return MakeErrorResponse(FString::Printf(TEXT("Failed to add streaming level: %s"), *LevelPath));

        return MakeSuccessResponse(FString::Printf(TEXT("Streaming level added: %s"), *LevelPath));
    });
    return true;
}

bool UCLevelHandler::HandleBuildLighting(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
{
    RunOnGameThread(OnComplete, []() -> FString
    {
        GEditor->Exec(GEditor->GetEditorWorldContext().World(), TEXT("BUILDLIGHTING"));
        return MakeSuccessResponse(TEXT("Lighting build initiated"));
    });
    return true;
}

bool UCLevelHandler::HandleGetLevelBounds(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
{
    RunOnGameThread(OnComplete, []() -> FString
    {
        UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
        if (!World) return MakeErrorResponse(TEXT("No editor world"));

        FBox LevelBounds(ForceInit);
        for (TActorIterator<AActor> It(World); It; ++It)
        {
            AActor* Actor = *It;
            if (Actor && !Actor->IsTemporarilyHiddenInEditor(true))
            {
                LevelBounds += Actor->GetComponentsBoundingBox(true);
            }
        }

        TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
        Data->SetObjectField(TEXT("min"), VectorToJson(LevelBounds.Min));
        Data->SetObjectField(TEXT("max"), VectorToJson(LevelBounds.Max));
        Data->SetObjectField(TEXT("center"), VectorToJson(LevelBounds.GetCenter()));
        Data->SetObjectField(TEXT("extent"), VectorToJson(LevelBounds.GetExtent()));
        return MakeSuccessResponse(Data);
    });
    return true;
}

bool UCLevelHandler::HandleSaveAll(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
{
    RunOnGameThread(OnComplete, []() -> FString
    {
        FEditorFileUtils::SaveDirtyPackages(false, true, true);
        return MakeSuccessResponse(TEXT("All dirty assets saved"));
    });
    return true;
}
