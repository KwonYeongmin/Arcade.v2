#include "Handlers/UCMaterialHandler.h"
#include "Materials/Material.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstance.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Components/MeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "Factories/MaterialInstanceConstantFactoryNew.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Editor.h"
#include "HttpPath.h"

void UCMaterialHandler::RegisterRoutes(TSharedRef<IHttpRouter> Router)
{
    Router->BindRoute(FHttpPath(TEXT("/materials")), EHttpServerRequestVerbs::VERB_GET,
        FHttpRequestHandler::CreateLambda([this](const FHttpServerRequest& Req, const FHttpResultCallback& OnComplete) -> bool { return HandleListMaterials(Req, OnComplete); }));

    Router->BindRoute(FHttpPath(TEXT("/materials/:path")), EHttpServerRequestVerbs::VERB_GET,
        FHttpRequestHandler::CreateLambda([this](const FHttpServerRequest& Req, const FHttpResultCallback& OnComplete) -> bool { return HandleGetMaterial(Req, OnComplete); }));

    Router->BindRoute(FHttpPath(TEXT("/actors/:label/material")), EHttpServerRequestVerbs::VERB_POST,
        FHttpRequestHandler::CreateLambda([this](const FHttpServerRequest& Req, const FHttpResultCallback& OnComplete) -> bool { return HandleApplyMaterialToActor(Req, OnComplete); }));

    Router->BindRoute(FHttpPath(TEXT("/materials/instances")), EHttpServerRequestVerbs::VERB_POST,
        FHttpRequestHandler::CreateLambda([this](const FHttpServerRequest& Req, const FHttpResultCallback& OnComplete) -> bool { return HandleCreateMaterialInstance(Req, OnComplete); }));

    Router->BindRoute(FHttpPath(TEXT("/materials/instances/:path/parameters")), EHttpServerRequestVerbs::VERB_PUT,
        FHttpRequestHandler::CreateLambda([this](const FHttpServerRequest& Req, const FHttpResultCallback& OnComplete) -> bool { return HandleSetMaterialInstanceParams(Req, OnComplete); }));

    Router->BindRoute(FHttpPath(TEXT("/materials/instances/:path/parameters")), EHttpServerRequestVerbs::VERB_GET,
        FHttpRequestHandler::CreateLambda([this](const FHttpServerRequest& Req, const FHttpResultCallback& OnComplete) -> bool { return HandleGetMaterialInstanceParams(Req, OnComplete); }));
}

bool UCMaterialHandler::HandleListMaterials(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
{
    FString PathFilter = Request.QueryParams.FindRef(TEXT("path"));

    RunOnGameThread(OnComplete, [PathFilter]() -> FString
    {
        FAssetRegistryModule& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
        FARFilter Filter;
        Filter.ClassPaths.Add(UMaterial::StaticClass()->GetClassPathName());
        Filter.ClassPaths.Add(UMaterialInstance::StaticClass()->GetClassPathName());
        Filter.ClassPaths.Add(UMaterialInstanceConstant::StaticClass()->GetClassPathName());
        Filter.bRecursivePaths = true;
        Filter.bRecursiveClasses = true;
        Filter.PackagePaths.Add(!PathFilter.IsEmpty() ? FName(*PathFilter) : FName("/Game"));

        TArray<FAssetData> Assets;
        AssetRegistry.Get().GetAssets(Filter, Assets);

        TArray<TSharedPtr<FJsonValue>> MatArray;
        for (const FAssetData& Asset : Assets)
        {
            TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
            Obj->SetStringField(TEXT("name"), Asset.AssetName.ToString());
            Obj->SetStringField(TEXT("path"), Asset.PackagePath.ToString());
            Obj->SetStringField(TEXT("full_path"), Asset.GetSoftObjectPath().ToString());
            Obj->SetStringField(TEXT("class"), Asset.AssetClassPath.GetAssetName().ToString());
            MatArray.Add(MakeShared<FJsonValueObject>(Obj));
        }

        TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
        Data->SetArrayField(TEXT("materials"), MatArray);
        Data->SetNumberField(TEXT("count"), MatArray.Num());
        return MakeSuccessResponse(Data);
    });
    return true;
}

bool UCMaterialHandler::HandleGetMaterial(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
{
    FString EncodedPath = Request.PathParams.FindRef(TEXT("path"));

    RunOnGameThread(OnComplete, [EncodedPath]() -> FString
    {
        FString MatPath = EncodedPath.Replace(TEXT("__"), TEXT("/"));
        if (!MatPath.StartsWith(TEXT("/"))) MatPath = TEXT("/") + MatPath;

        UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr, *MatPath);
        if (!Material) return MakeErrorResponse(FString::Printf(TEXT("Material not found: %s"), *MatPath));

        return MakeSuccessResponse(MaterialToJson(Material));
    });
    return true;
}

bool UCMaterialHandler::HandleApplyMaterialToActor(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
{
    FString Label = Request.PathParams.FindRef(TEXT("label"));
    TSharedPtr<FJsonObject> Body = ParseJsonBody(Request);

    RunOnGameThread(OnComplete, [Label, Body]() -> FString
    {
        UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
        if (!World) return MakeErrorResponse(TEXT("No editor world"));

        AActor* Actor = nullptr;
        for (TActorIterator<AActor> It(World); It; ++It)
        {
            if ((*It)->GetActorLabel() == Label) { Actor = *It; break; }
        }
        if (!Actor) return MakeErrorResponse(FString::Printf(TEXT("Actor not found: %s"), *Label));

        FString MatPath = Body->GetStringField(TEXT("material_path"));
        int32 MaterialIndex = 0;
        Body->TryGetNumberField(TEXT("material_index"), MaterialIndex);

        UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr, *MatPath);
        if (!Material) return MakeErrorResponse(FString::Printf(TEXT("Material not found: %s"), *MatPath));

        int32 AppliedCount = 0;
        for (UActorComponent* Comp : Actor->GetComponents())
        {
            if (UMeshComponent* Mesh = Cast<UMeshComponent>(Comp))
            {
                if (MaterialIndex < 0)
                {
                    // Apply to all slots
                    for (int32 i = 0; i < Mesh->GetNumMaterials(); ++i)
                    {
                        Mesh->SetMaterial(i, Material);
                    }
                    AppliedCount += Mesh->GetNumMaterials();
                }
                else
                {
                    Mesh->SetMaterial(MaterialIndex, Material);
                    AppliedCount++;
                }
            }
        }

        if (AppliedCount == 0) return MakeErrorResponse(TEXT("No mesh components found on actor"));

        TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
        Data->SetStringField(TEXT("actor"), Label);
        Data->SetStringField(TEXT("material"), MatPath);
        Data->SetNumberField(TEXT("slots_affected"), AppliedCount);
        return MakeSuccessResponse(Data);
    });
    return true;
}

bool UCMaterialHandler::HandleCreateMaterialInstance(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
{
    TSharedPtr<FJsonObject> Body = ParseJsonBody(Request);

    RunOnGameThread(OnComplete, [Body]() -> FString
    {
        FString ParentPath    = Body->GetStringField(TEXT("parent_path"));
        FString InstanceName  = Body->GetStringField(TEXT("name"));
        FString PackagePath   = Body->GetStringField(TEXT("path"));

        UMaterialInterface* Parent = LoadObject<UMaterialInterface>(nullptr, *ParentPath);
        if (!Parent) return MakeErrorResponse(FString::Printf(TEXT("Parent material not found: %s"), *ParentPath));

        IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
        UMaterialInstanceConstantFactoryNew* Factory = NewObject<UMaterialInstanceConstantFactoryNew>();
        Factory->InitialParent = Parent;

        UObject* Asset = AssetTools.CreateAsset(InstanceName, PackagePath, UMaterialInstanceConstant::StaticClass(), Factory);
        if (!Asset) return MakeErrorResponse(TEXT("Failed to create material instance"));

        TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
        Data->SetStringField(TEXT("name"), InstanceName);
        Data->SetStringField(TEXT("path"), PackagePath + TEXT("/") + InstanceName);
        Data->SetStringField(TEXT("parent"), ParentPath);
        return MakeSuccessResponse(Data);
    });
    return true;
}

bool UCMaterialHandler::HandleSetMaterialInstanceParams(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
{
    FString EncodedPath = Request.PathParams.FindRef(TEXT("path"));
    TSharedPtr<FJsonObject> Body = ParseJsonBody(Request);

    RunOnGameThread(OnComplete, [EncodedPath, Body]() -> FString
    {
        FString MatPath = EncodedPath.Replace(TEXT("__"), TEXT("/"));
        if (!MatPath.StartsWith(TEXT("/"))) MatPath = TEXT("/") + MatPath;

        UMaterialInstanceConstant* MatInst = LoadObject<UMaterialInstanceConstant>(nullptr, *MatPath);
        if (!MatInst) return MakeErrorResponse(FString::Printf(TEXT("Material instance not found: %s"), *MatPath));

        // Scalar params
        const TSharedPtr<FJsonObject>* ScalarParams;
        if (Body->TryGetObjectField(TEXT("scalar"), ScalarParams))
        {
            for (auto& Pair : (*ScalarParams)->Values)
            {
                double Val = 0.0;
                Pair.Value->TryGetNumber(Val);
                MatInst->SetScalarParameterValueEditorOnly(FName(*Pair.Key), (float)Val);
            }
        }

        // Vector params
        const TSharedPtr<FJsonObject>* VectorParams;
        if (Body->TryGetObjectField(TEXT("vector"), VectorParams))
        {
            for (auto& Pair : (*VectorParams)->Values)
            {
                const TSharedPtr<FJsonObject>* ColorObj;
                if (Pair.Value->TryGetObject(ColorObj))
                {
                    FLinearColor Color;
                    (*ColorObj)->TryGetNumberField(TEXT("r"), Color.R);
                    (*ColorObj)->TryGetNumberField(TEXT("g"), Color.G);
                    (*ColorObj)->TryGetNumberField(TEXT("b"), Color.B);
                    (*ColorObj)->TryGetNumberField(TEXT("a"), Color.A);
                    MatInst->SetVectorParameterValueEditorOnly(FName(*Pair.Key), Color);
                }
            }
        }

        MatInst->MarkPackageDirty();

        return MakeSuccessResponse(TEXT("Material instance parameters updated"));
    });
    return true;
}

bool UCMaterialHandler::HandleGetMaterialInstanceParams(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
{
    FString EncodedPath = Request.PathParams.FindRef(TEXT("path"));

    RunOnGameThread(OnComplete, [EncodedPath]() -> FString
    {
        FString MatPath = EncodedPath.Replace(TEXT("__"), TEXT("/"));
        if (!MatPath.StartsWith(TEXT("/"))) MatPath = TEXT("/") + MatPath;

        UMaterialInstanceConstant* MatInst = LoadObject<UMaterialInstanceConstant>(nullptr, *MatPath);
        if (!MatInst) return MakeErrorResponse(FString::Printf(TEXT("Material instance not found: %s"), *MatPath));

        TSharedPtr<FJsonObject> ScalarObj = MakeShared<FJsonObject>();
        TArray<FMaterialParameterInfo> ScalarParams;
        TArray<FGuid> Guids;
        MatInst->GetAllScalarParameterInfo(ScalarParams, Guids);
        for (const FMaterialParameterInfo& Param : ScalarParams)
        {
            float Val = 0.f;
            MatInst->GetScalarParameterValue(Param, Val);
            ScalarObj->SetNumberField(Param.Name.ToString(), Val);
        }

        TSharedPtr<FJsonObject> VectorObj = MakeShared<FJsonObject>();
        TArray<FMaterialParameterInfo> VectorParams;
        MatInst->GetAllVectorParameterInfo(VectorParams, Guids);
        for (const FMaterialParameterInfo& Param : VectorParams)
        {
            FLinearColor Color;
            MatInst->GetVectorParameterValue(Param, Color);
            TSharedPtr<FJsonObject> ColorObj = MakeShared<FJsonObject>();
            ColorObj->SetNumberField(TEXT("r"), Color.R);
            ColorObj->SetNumberField(TEXT("g"), Color.G);
            ColorObj->SetNumberField(TEXT("b"), Color.B);
            ColorObj->SetNumberField(TEXT("a"), Color.A);
            VectorObj->SetObjectField(Param.Name.ToString(), ColorObj);
        }

        TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
        Data->SetObjectField(TEXT("scalar"), ScalarObj);
        Data->SetObjectField(TEXT("vector"), VectorObj);
        return MakeSuccessResponse(Data);
    });
    return true;
}

TSharedPtr<FJsonObject> UCMaterialHandler::MaterialToJson(UMaterialInterface* Material)
{
    TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
    if (!Material) return Obj;

    Obj->SetStringField(TEXT("name"), Material->GetName());
    Obj->SetStringField(TEXT("path"), Material->GetPathName());
    Obj->SetStringField(TEXT("class"), Material->GetClass()->GetName());

    TArray<FMaterialParameterInfo> ScalarParams;
    TArray<FGuid> Guids;
    Material->GetAllScalarParameterInfo(ScalarParams, Guids);
    TArray<TSharedPtr<FJsonValue>> ScalarArr;
    for (const FMaterialParameterInfo& P : ScalarParams)
    {
        ScalarArr.Add(MakeShared<FJsonValueString>(P.Name.ToString()));
    }
    Obj->SetArrayField(TEXT("scalar_params"), ScalarArr);

    TArray<FMaterialParameterInfo> VectorParams;
    Material->GetAllVectorParameterInfo(VectorParams, Guids);
    TArray<TSharedPtr<FJsonValue>> VectorArr;
    for (const FMaterialParameterInfo& P : VectorParams)
    {
        VectorArr.Add(MakeShared<FJsonValueString>(P.Name.ToString()));
    }
    Obj->SetArrayField(TEXT("vector_params"), VectorArr);

    return Obj;
}
