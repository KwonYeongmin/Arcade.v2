#include "Handlers/UCEditorHandler.h"
#include "Editor.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Selection.h"
#include "EngineUtils.h"
#include "EditorViewportClient.h"
#include "LevelEditorViewport.h"
#include "LevelEditor.h"
#include "Misc/OutputDeviceNull.h"
#include "Misc/OutputDevice.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "GameFramework/WorldSettings.h"
#include "HttpPath.h"
#include "Logging/MessageLog.h"
#include "AssetRegistry/AssetRegistryModule.h"

void UCEditorHandler::RegisterRoutes(TSharedRef<IHttpRouter> Router)
{
    Router->BindRoute(FHttpPath(TEXT("/editor/command")), EHttpServerRequestVerbs::VERB_POST,
        FHttpRequestHandler::CreateLambda([this](const FHttpServerRequest& Req, const FHttpResultCallback& OnComplete) -> bool { return HandleRunCommand(Req, OnComplete); }));

    Router->BindRoute(FHttpPath(TEXT("/editor/python")), EHttpServerRequestVerbs::VERB_POST,
        FHttpRequestHandler::CreateLambda([this](const FHttpServerRequest& Req, const FHttpResultCallback& OnComplete) -> bool { return HandleRunPython(Req, OnComplete); }));

    Router->BindRoute(FHttpPath(TEXT("/editor/undo")), EHttpServerRequestVerbs::VERB_POST,
        FHttpRequestHandler::CreateLambda([this](const FHttpServerRequest& Req, const FHttpResultCallback& OnComplete) -> bool { return HandleUndo(Req, OnComplete); }));

    Router->BindRoute(FHttpPath(TEXT("/editor/redo")), EHttpServerRequestVerbs::VERB_POST,
        FHttpRequestHandler::CreateLambda([this](const FHttpServerRequest& Req, const FHttpResultCallback& OnComplete) -> bool { return HandleRedo(Req, OnComplete); }));

    Router->BindRoute(FHttpPath(TEXT("/editor/screenshot")), EHttpServerRequestVerbs::VERB_POST,
        FHttpRequestHandler::CreateLambda([this](const FHttpServerRequest& Req, const FHttpResultCallback& OnComplete) -> bool { return HandleScreenshot(Req, OnComplete); }));

    Router->BindRoute(FHttpPath(TEXT("/editor/refresh-content")), EHttpServerRequestVerbs::VERB_POST,
        FHttpRequestHandler::CreateLambda([this](const FHttpServerRequest& Req, const FHttpResultCallback& OnComplete) -> bool { return HandleRefreshContent(Req, OnComplete); }));

    Router->BindRoute(FHttpPath(TEXT("/editor/selection")), EHttpServerRequestVerbs::VERB_GET,
        FHttpRequestHandler::CreateLambda([this](const FHttpServerRequest& Req, const FHttpResultCallback& OnComplete) -> bool { return HandleGetSelection(Req, OnComplete); }));

    Router->BindRoute(FHttpPath(TEXT("/editor/select")), EHttpServerRequestVerbs::VERB_POST,
        FHttpRequestHandler::CreateLambda([this](const FHttpServerRequest& Req, const FHttpResultCallback& OnComplete) -> bool { return HandleSelectActors(Req, OnComplete); }));

    Router->BindRoute(FHttpPath(TEXT("/editor/world")), EHttpServerRequestVerbs::VERB_GET,
        FHttpRequestHandler::CreateLambda([this](const FHttpServerRequest& Req, const FHttpResultCallback& OnComplete) -> bool { return HandleGetWorldSettings(Req, OnComplete); }));

    Router->BindRoute(FHttpPath(TEXT("/editor/world")), EHttpServerRequestVerbs::VERB_PUT,
        FHttpRequestHandler::CreateLambda([this](const FHttpServerRequest& Req, const FHttpResultCallback& OnComplete) -> bool { return HandleSetWorldSettings(Req, OnComplete); }));

    Router->BindRoute(FHttpPath(TEXT("/editor/log")), EHttpServerRequestVerbs::VERB_GET,
        FHttpRequestHandler::CreateLambda([this](const FHttpServerRequest& Req, const FHttpResultCallback& OnComplete) -> bool { return HandleGetLog(Req, OnComplete); }));
}

bool UCEditorHandler::HandleRunCommand(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
{
    TSharedPtr<FJsonObject> Body = ParseJsonBody(Request);

    RunOnGameThread(OnComplete, [Body]() -> FString
    {
        FString Command = Body->GetStringField(TEXT("command"));
        if (Command.IsEmpty()) return MakeErrorResponse(TEXT("command is required"));

        UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
        if (!World) return MakeErrorResponse(TEXT("No editor world"));

        FOutputDeviceNull OutputDevice;
        bool bResult = GEditor->Exec(World, *Command, OutputDevice);

        TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
        Data->SetStringField(TEXT("command"), Command);
        Data->SetBoolField(TEXT("executed"), bResult);
        return MakeSuccessResponse(Data);
    });
    return true;
}

bool UCEditorHandler::HandleRunPython(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
{
    TSharedPtr<FJsonObject> Body = ParseJsonBody(Request);

    RunOnGameThread(OnComplete, [Body]() -> FString
    {
        FString Script = Body->GetStringField(TEXT("script"));
        if (Script.IsEmpty()) return MakeErrorResponse(TEXT("script is required"));

        FString IntermediateDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectIntermediateDir());
        FString TempScript  = IntermediateDir / TEXT("uc_temp_script.py");
        FString TempOutput  = IntermediateDir / TEXT("uc_temp_output.txt");

        // Indent user script so it sits inside the try block
        TArray<FString> Lines;
        Script.ParseIntoArrayLines(Lines, false);
        FString IndentedScript;
        for (const FString& Line : Lines)
            IndentedScript += TEXT("    ") + Line + TEXT("\n");

        // Wrap to redirect stdout/stderr to a file
        FString Wrapped = FString::Printf(
            TEXT("import sys as _sys\n")
            TEXT("_uc_f = open(r\"%s\", \"w\", encoding=\"utf-8\")\n")
            TEXT("_sys.stdout = _uc_f\n")
            TEXT("_sys.stderr = _uc_f\n")
            TEXT("try:\n")
            TEXT("%s")
            TEXT("except Exception as _e:\n")
            TEXT("    print(f\"EXCEPTION: {_e}\")\n")
            TEXT("finally:\n")
            TEXT("    _uc_f.flush()\n")
            TEXT("    _uc_f.close()\n")
            TEXT("    _sys.stdout = _sys.__stdout__\n")
            TEXT("    _sys.stderr = _sys.__stderr__\n"),
            *TempOutput, *IndentedScript
        );

        FFileHelper::SaveStringToFile(Wrapped, *TempScript, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);

        UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
        FOutputDeviceNull NullDevice;
        FString PythonCmd = FString::Printf(TEXT("py \"%s\""), *TempScript);
        bool bResult = GEditor->Exec(World, *PythonCmd, NullDevice);

        IFileManager::Get().Delete(*TempScript);

        FString OutputStr;
        FFileHelper::LoadFileToString(OutputStr, *TempOutput);
        IFileManager::Get().Delete(*TempOutput);

        TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
        Data->SetBoolField(TEXT("executed"), bResult);
        Data->SetStringField(TEXT("output"), OutputStr);
        Data->SetStringField(TEXT("script"), Script.Left(200));
        return MakeSuccessResponse(Data);
    });
    return true;
}

bool UCEditorHandler::HandleUndo(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
{
    RunOnGameThread(OnComplete, []() -> FString
    {
        GEditor->UndoTransaction();
        return MakeSuccessResponse(TEXT("Undo performed"));
    });
    return true;
}

bool UCEditorHandler::HandleRedo(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
{
    RunOnGameThread(OnComplete, []() -> FString
    {
        GEditor->RedoTransaction();
        return MakeSuccessResponse(TEXT("Redo performed"));
    });
    return true;
}

bool UCEditorHandler::HandleScreenshot(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
{
    TSharedPtr<FJsonObject> Body = ParseJsonBody(Request);

    RunOnGameThread(OnComplete, [Body]() -> FString
    {
        FString FileName = TEXT("UnrealConnect_Screenshot");
        Body->TryGetStringField(TEXT("filename"), FileName);

        UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
        FString Command = FString::Printf(TEXT("SHOT FILENAME=%s"), *FileName);
        FOutputDeviceNull OutputDevice;
        GEditor->Exec(World, *Command, OutputDevice);

        TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
        Data->SetStringField(TEXT("filename"), FileName);
        return MakeSuccessResponse(Data);
    });
    return true;
}

bool UCEditorHandler::HandleRefreshContent(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
{
    RunOnGameThread(OnComplete, []() -> FString
    {
        FAssetRegistryModule& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
        AssetRegistry.Get().ScanPathsSynchronous({TEXT("/Game")}, true);
        return MakeSuccessResponse(TEXT("Content browser refreshed"));
    });
    return true;
}

bool UCEditorHandler::HandleGetSelection(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
{
    RunOnGameThread(OnComplete, []() -> FString
    {
        TArray<TSharedPtr<FJsonValue>> ActorArray;
        USelection* Selection = GEditor->GetSelectedActors();

        for (FSelectionIterator It(*Selection); It; ++It)
        {
            AActor* Actor = Cast<AActor>(*It);
            if (!Actor) continue;

            TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
            Obj->SetStringField(TEXT("label"), Actor->GetActorLabel());
            Obj->SetStringField(TEXT("class"), Actor->GetClass()->GetName());
            ActorArray.Add(MakeShared<FJsonValueObject>(Obj));
        }

        TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
        Data->SetArrayField(TEXT("selected_actors"), ActorArray);
        Data->SetNumberField(TEXT("count"), ActorArray.Num());
        return MakeSuccessResponse(Data);
    });
    return true;
}

bool UCEditorHandler::HandleSelectActors(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
{
    TSharedPtr<FJsonObject> Body = ParseJsonBody(Request);

    RunOnGameThread(OnComplete, [Body]() -> FString
    {
        UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
        if (!World) return MakeErrorResponse(TEXT("No editor world"));

        bool bClearPrev = true;
        Body->TryGetBoolField(TEXT("clear_previous"), bClearPrev);

        if (bClearPrev)
        {
            GEditor->SelectNone(true, true);
        }

        const TArray<TSharedPtr<FJsonValue>>* Labels;
        int32 SelectedCount = 0;
        if (Body->TryGetArrayField(TEXT("labels"), Labels))
        {
            for (const auto& LabelVal : *Labels)
            {
                FString Label;
                if (!LabelVal->TryGetString(Label)) continue;

                for (TActorIterator<AActor> It(World); It; ++It)
                {
                    if ((*It)->GetActorLabel() == Label)
                    {
                        GEditor->SelectActor(*It, true, false);
                        SelectedCount++;
                        break;
                    }
                }
            }
            GEditor->NoteSelectionChange();
        }

        TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
        Data->SetNumberField(TEXT("selected_count"), SelectedCount);
        return MakeSuccessResponse(Data);
    });
    return true;
}

bool UCEditorHandler::HandleGetWorldSettings(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
{
    RunOnGameThread(OnComplete, []() -> FString
    {
        UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
        if (!World) return MakeErrorResponse(TEXT("No editor world"));

        AWorldSettings* WS = World->GetWorldSettings();
        if (!WS) return MakeErrorResponse(TEXT("No world settings"));

        TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
        Data->SetNumberField(TEXT("gravity_z"), WS->GetGravityZ());
        Data->SetBoolField(TEXT("enable_world_bounds"), WS->bEnableWorldBoundsChecks);
        Data->SetNumberField(TEXT("kill_z"), WS->KillZ);

        return MakeSuccessResponse(Data);
    });
    return true;
}

bool UCEditorHandler::HandleSetWorldSettings(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
{
    TSharedPtr<FJsonObject> Body = ParseJsonBody(Request);

    RunOnGameThread(OnComplete, [Body]() -> FString
    {
        UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
        if (!World) return MakeErrorResponse(TEXT("No editor world"));

        AWorldSettings* WS = World->GetWorldSettings();
        if (!WS) return MakeErrorResponse(TEXT("No world settings"));

        double GravityZ = 0.0;
        if (Body->TryGetNumberField(TEXT("gravity_z"), GravityZ))
        {
            WS->bGlobalGravitySet = true;
            WS->GlobalGravityZ = (float)GravityZ;
        }

        double KillZ = 0.0;
        if (Body->TryGetNumberField(TEXT("kill_z"), KillZ))
        {
            WS->KillZ = (float)KillZ;
        }

        bool bEnableBounds = false;
        if (Body->TryGetBoolField(TEXT("enable_world_bounds"), bEnableBounds))
        {
            WS->bEnableWorldBoundsChecks = bEnableBounds;
        }

        WS->MarkPackageDirty();
        return MakeSuccessResponse(TEXT("World settings updated"));
    });
    return true;
}

bool UCEditorHandler::HandleGetLog(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
{
    int32 LineCount = 100;
    FString CountStr = Request.QueryParams.FindRef(TEXT("lines"));
    if (!CountStr.IsEmpty()) LineCount = FCString::Atoi(*CountStr);

    RunOnGameThread(OnComplete, [LineCount]() -> FString
    {
        // Get recent output log messages via GLog
        // Since direct log access is complex, we return a placeholder
        TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
        Data->SetStringField(TEXT("note"), TEXT("Use Unreal Editor Output Log for full log access. This endpoint is a placeholder."));
        Data->SetNumberField(TEXT("requested_lines"), LineCount);
        return MakeSuccessResponse(Data);
    });
    return true;
}
