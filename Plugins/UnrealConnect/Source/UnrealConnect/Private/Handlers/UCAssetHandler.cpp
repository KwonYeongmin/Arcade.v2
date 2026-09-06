#include "Handlers/UCAssetHandler.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/AssetData.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "ObjectTools.h"
#include "PackageTools.h"
#include "EditorAssetLibrary.h"
#include "HttpPath.h"
#include "UObject/SavePackage.h"
#include "Misc/PackageName.h"

void UCAssetHandler::RegisterRoutes(TSharedRef<IHttpRouter> Router)
{
    Router->BindRoute(FHttpPath(TEXT("/assets")), EHttpServerRequestVerbs::VERB_GET,
        FHttpRequestHandler::CreateLambda([this](const FHttpServerRequest& Req, const FHttpResultCallback& OnComplete) -> bool { return HandleListAssets(Req, OnComplete); }));

    Router->BindRoute(FHttpPath(TEXT("/assets/import")), EHttpServerRequestVerbs::VERB_POST,
        FHttpRequestHandler::CreateLambda([this](const FHttpServerRequest& Req, const FHttpResultCallback& OnComplete) -> bool { return HandleImportAsset(Req, OnComplete); }));

    Router->BindRoute(FHttpPath(TEXT("/assets/folder")), EHttpServerRequestVerbs::VERB_POST,
        FHttpRequestHandler::CreateLambda([this](const FHttpServerRequest& Req, const FHttpResultCallback& OnComplete) -> bool { return HandleCreateFolder(Req, OnComplete); }));

    Router->BindRoute(FHttpPath(TEXT("/assets/classes")), EHttpServerRequestVerbs::VERB_GET,
        FHttpRequestHandler::CreateLambda([this](const FHttpServerRequest& Req, const FHttpResultCallback& OnComplete) -> bool { return HandleListAssetClasses(Req, OnComplete); }));

    Router->BindRoute(FHttpPath(TEXT("/assets/:path")), EHttpServerRequestVerbs::VERB_DELETE,
        FHttpRequestHandler::CreateLambda([this](const FHttpServerRequest& Req, const FHttpResultCallback& OnComplete) -> bool { return HandleDeleteAsset(Req, OnComplete); }));

    Router->BindRoute(FHttpPath(TEXT("/assets/:path/duplicate")), EHttpServerRequestVerbs::VERB_POST,
        FHttpRequestHandler::CreateLambda([this](const FHttpServerRequest& Req, const FHttpResultCallback& OnComplete) -> bool { return HandleDuplicateAsset(Req, OnComplete); }));

    Router->BindRoute(FHttpPath(TEXT("/assets/:path/rename")), EHttpServerRequestVerbs::VERB_POST,
        FHttpRequestHandler::CreateLambda([this](const FHttpServerRequest& Req, const FHttpResultCallback& OnComplete) -> bool { return HandleRenameAsset(Req, OnComplete); }));

    Router->BindRoute(FHttpPath(TEXT("/assets/:path/metadata")), EHttpServerRequestVerbs::VERB_GET,
        FHttpRequestHandler::CreateLambda([this](const FHttpServerRequest& Req, const FHttpResultCallback& OnComplete) -> bool { return HandleGetMetadata(Req, OnComplete); }));

    Router->BindRoute(FHttpPath(TEXT("/assets/:path/metadata")), EHttpServerRequestVerbs::VERB_PUT,
        FHttpRequestHandler::CreateLambda([this](const FHttpServerRequest& Req, const FHttpResultCallback& OnComplete) -> bool { return HandleSetMetadata(Req, OnComplete); }));
}

bool UCAssetHandler::HandleListAssets(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
{
    FString PathFilter  = Request.QueryParams.FindRef(TEXT("path"));
    FString ClassFilter = Request.QueryParams.FindRef(TEXT("class"));
    FString NameFilter  = Request.QueryParams.FindRef(TEXT("filter"));

    RunOnGameThread(OnComplete, [PathFilter, ClassFilter, NameFilter]() -> FString
    {
        FAssetRegistryModule& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
        FARFilter Filter;
        Filter.bRecursivePaths = true;
        Filter.bRecursiveClasses = true;
        Filter.PackagePaths.Add(!PathFilter.IsEmpty() ? FName(*PathFilter) : FName("/Game"));

        if (!ClassFilter.IsEmpty())
        {
            // Try to find the class
            for (TObjectIterator<UClass> ClassIt; ClassIt; ++ClassIt)
            {
                if (ClassIt->GetName() == ClassFilter)
                {
                    Filter.ClassPaths.Add(ClassIt->GetClassPathName());
                    break;
                }
            }
        }

        TArray<FAssetData> Assets;
        AssetRegistry.Get().GetAssets(Filter, Assets);

        TArray<TSharedPtr<FJsonValue>> AssetArray;
        for (const FAssetData& Asset : Assets)
        {
            if (!NameFilter.IsEmpty() && !Asset.AssetName.ToString().Contains(NameFilter)) continue;
            AssetArray.Add(MakeShared<FJsonValueObject>(AssetDataToJson(Asset)));
        }

        TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
        Data->SetArrayField(TEXT("assets"), AssetArray);
        Data->SetNumberField(TEXT("count"), AssetArray.Num());
        return MakeSuccessResponse(Data);
    });
    return true;
}

bool UCAssetHandler::HandleImportAsset(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
{
    TSharedPtr<FJsonObject> Body = ParseJsonBody(Request);

    RunOnGameThread(OnComplete, [Body]() -> FString
    {
        FString SourcePath  = Body->GetStringField(TEXT("source_path"));
        FString DestPath    = Body->GetStringField(TEXT("dest_path"));

        if (SourcePath.IsEmpty() || DestPath.IsEmpty())
        {
            return MakeErrorResponse(TEXT("source_path and dest_path are required"));
        }

        IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
        TArray<FString> Files = {SourcePath};
        TArray<UObject*> Imported = AssetTools.ImportAssets(Files, DestPath);

        if (Imported.Num() == 0) return MakeErrorResponse(TEXT("Import failed"));

        TArray<TSharedPtr<FJsonValue>> ImportedArr;
        for (UObject* Obj : Imported)
        {
            ImportedArr.Add(MakeShared<FJsonValueString>(Obj->GetPathName()));
        }

        TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
        Data->SetArrayField(TEXT("imported"), ImportedArr);
        Data->SetNumberField(TEXT("count"), Imported.Num());
        return MakeSuccessResponse(Data);
    });
    return true;
}

bool UCAssetHandler::HandleDeleteAsset(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
{
    FString EncodedPath = Request.PathParams.FindRef(TEXT("path"));

    RunOnGameThread(OnComplete, [EncodedPath]() -> FString
    {
        FString AssetPath = EncodedPath.Replace(TEXT("__"), TEXT("/"));
        if (!AssetPath.StartsWith(TEXT("/"))) AssetPath = TEXT("/") + AssetPath;

        bool bDeleted = UEditorAssetLibrary::DeleteAsset(AssetPath);
        if (!bDeleted) return MakeErrorResponse(FString::Printf(TEXT("Failed to delete: %s"), *AssetPath));

        return MakeSuccessResponse(FString::Printf(TEXT("Asset deleted: %s"), *AssetPath));
    });
    return true;
}

bool UCAssetHandler::HandleDuplicateAsset(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
{
    FString EncodedPath = Request.PathParams.FindRef(TEXT("path"));
    TSharedPtr<FJsonObject> Body = ParseJsonBody(Request);

    RunOnGameThread(OnComplete, [EncodedPath, Body]() -> FString
    {
        FString AssetPath = EncodedPath.Replace(TEXT("__"), TEXT("/"));
        if (!AssetPath.StartsWith(TEXT("/"))) AssetPath = TEXT("/") + AssetPath;

        FString DestPath = Body->GetStringField(TEXT("dest_path"));
        if (DestPath.IsEmpty()) return MakeErrorResponse(TEXT("dest_path is required"));

        UObject* Duplicate = UEditorAssetLibrary::DuplicateAsset(AssetPath, DestPath);
        if (!Duplicate) return MakeErrorResponse(FString::Printf(TEXT("Failed to duplicate: %s"), *AssetPath));

        TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
        Data->SetStringField(TEXT("source"), AssetPath);
        Data->SetStringField(TEXT("duplicate"), Duplicate->GetPathName());
        return MakeSuccessResponse(Data);
    });
    return true;
}

bool UCAssetHandler::HandleRenameAsset(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
{
    FString EncodedPath = Request.PathParams.FindRef(TEXT("path"));
    TSharedPtr<FJsonObject> Body = ParseJsonBody(Request);

    RunOnGameThread(OnComplete, [EncodedPath, Body]() -> FString
    {
        FString AssetPath = EncodedPath.Replace(TEXT("__"), TEXT("/"));
        if (!AssetPath.StartsWith(TEXT("/"))) AssetPath = TEXT("/") + AssetPath;

        FString NewPath = Body->GetStringField(TEXT("new_path"));
        if (NewPath.IsEmpty()) return MakeErrorResponse(TEXT("new_path is required"));

        bool bRenamed = UEditorAssetLibrary::RenameAsset(AssetPath, NewPath);
        if (!bRenamed) return MakeErrorResponse(FString::Printf(TEXT("Failed to rename: %s"), *AssetPath));

        TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
        Data->SetStringField(TEXT("old_path"), AssetPath);
        Data->SetStringField(TEXT("new_path"), NewPath);
        return MakeSuccessResponse(Data);
    });
    return true;
}

bool UCAssetHandler::HandleGetMetadata(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
{
    FString EncodedPath = Request.PathParams.FindRef(TEXT("path"));

    RunOnGameThread(OnComplete, [EncodedPath]() -> FString
    {
        FString AssetPath = EncodedPath.Replace(TEXT("__"), TEXT("/"));
        if (!AssetPath.StartsWith(TEXT("/"))) AssetPath = TEXT("/") + AssetPath;

        FAssetRegistryModule& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
        FAssetData AssetData = AssetRegistry.Get().GetAssetByObjectPath(FSoftObjectPath(AssetPath));
        if (!AssetData.IsValid()) return MakeErrorResponse(FString::Printf(TEXT("Asset not found: %s"), *AssetPath));

        TSharedPtr<FJsonObject> Data = AssetDataToJson(AssetData);

        // Tag metadata
        TSharedPtr<FJsonObject> TagsObj = MakeShared<FJsonObject>();
        AssetData.TagsAndValues.ForEach([&TagsObj](const TPair<FName, FAssetTagValueRef>& Tag)
        {
            TagsObj->SetStringField(Tag.Key.ToString(), Tag.Value.AsString());
        });
        Data->SetObjectField(TEXT("tags"), TagsObj);

        return MakeSuccessResponse(Data);
    });
    return true;
}

bool UCAssetHandler::HandleSetMetadata(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
{
    FString EncodedPath = Request.PathParams.FindRef(TEXT("path"));
    TSharedPtr<FJsonObject> Body = ParseJsonBody(Request);

    RunOnGameThread(OnComplete, [EncodedPath, Body]() -> FString
    {
        // Asset metadata (tags) editing requires loading the asset and modifying UMetaData
        // This is complex and asset-type specific; we return a note
        TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
        Data->SetStringField(TEXT("note"), TEXT("Metadata editing requires asset-specific implementation. Use editor scripting for complex metadata."));
        return MakeSuccessResponse(Data);
    });
    return true;
}

bool UCAssetHandler::HandleCreateFolder(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
{
    TSharedPtr<FJsonObject> Body = ParseJsonBody(Request);

    RunOnGameThread(OnComplete, [Body]() -> FString
    {
        FString FolderPath = Body->GetStringField(TEXT("path"));
        if (FolderPath.IsEmpty()) return MakeErrorResponse(TEXT("path is required"));

        bool bCreated = UEditorAssetLibrary::MakeDirectory(FolderPath);
        if (!bCreated) return MakeErrorResponse(FString::Printf(TEXT("Failed to create folder: %s"), *FolderPath));

        return MakeSuccessResponse(FString::Printf(TEXT("Folder created: %s"), *FolderPath));
    });
    return true;
}

bool UCAssetHandler::HandleListAssetClasses(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
{
    RunOnGameThread(OnComplete, []() -> FString
    {
        IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
        TArray<TWeakPtr<IAssetTypeActions>> AssetTypes;
        AssetTools.GetAssetTypeActionsList(AssetTypes);

        TArray<TSharedPtr<FJsonValue>> ClassArray;
        for (const auto& WeakType : AssetTypes)
        {
            auto Type = WeakType.Pin();
            if (!Type.IsValid()) continue;

            TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
            Obj->SetStringField(TEXT("name"), Type->GetName().ToString());
            Obj->SetStringField(TEXT("class"), Type->GetSupportedClass() ? Type->GetSupportedClass()->GetName() : TEXT(""));
            ClassArray.Add(MakeShared<FJsonValueObject>(Obj));
        }

        TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
        Data->SetArrayField(TEXT("classes"), ClassArray);
        Data->SetNumberField(TEXT("count"), ClassArray.Num());
        return MakeSuccessResponse(Data);
    });
    return true;
}

TSharedPtr<FJsonObject> UCAssetHandler::AssetDataToJson(const FAssetData& AssetData)
{
    TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
    Obj->SetStringField(TEXT("name"), AssetData.AssetName.ToString());
    Obj->SetStringField(TEXT("path"), AssetData.PackagePath.ToString());
    Obj->SetStringField(TEXT("full_path"), AssetData.GetSoftObjectPath().ToString());
    Obj->SetStringField(TEXT("class"), AssetData.AssetClassPath.GetAssetName().ToString());
    Obj->SetStringField(TEXT("package_name"), AssetData.PackageName.ToString());
    return Obj;
}
