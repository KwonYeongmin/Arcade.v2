#include "Handlers/UCBlueprintHandler.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_Event.h"
#include "K2Node_CallFunction.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_FunctionResult.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_IfThenElse.h"
#include "K2Node_PromotableOperator.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "KismetCompiler.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "Factories/BlueprintFactory.h"
#include "HttpPath.h"
#include "UObject/UObjectIterator.h"
#include "UObject/UObjectGlobals.h"
#include "GameFramework/Actor.h"

void UCBlueprintHandler::RegisterRoutes(TSharedRef<IHttpRouter> Router)
{
    Router->BindRoute(FHttpPath(TEXT("/blueprints")), EHttpServerRequestVerbs::VERB_GET,
        FHttpRequestHandler::CreateLambda([this](const FHttpServerRequest& Req, const FHttpResultCallback& OnComplete) -> bool { return HandleListBlueprints(Req, OnComplete); }));

    Router->BindRoute(FHttpPath(TEXT("/blueprints")), EHttpServerRequestVerbs::VERB_POST,
        FHttpRequestHandler::CreateLambda([this](const FHttpServerRequest& Req, const FHttpResultCallback& OnComplete) -> bool { return HandleCreateBlueprint(Req, OnComplete); }));

    Router->BindRoute(FHttpPath(TEXT("/blueprints/:path")), EHttpServerRequestVerbs::VERB_GET,
        FHttpRequestHandler::CreateLambda([this](const FHttpServerRequest& Req, const FHttpResultCallback& OnComplete) -> bool { return HandleReadBlueprint(Req, OnComplete); }));

    Router->BindRoute(FHttpPath(TEXT("/blueprints/:path/compile")), EHttpServerRequestVerbs::VERB_POST,
        FHttpRequestHandler::CreateLambda([this](const FHttpServerRequest& Req, const FHttpResultCallback& OnComplete) -> bool { return HandleCompileBlueprint(Req, OnComplete); }));

    Router->BindRoute(FHttpPath(TEXT("/blueprints/:path/variables")), EHttpServerRequestVerbs::VERB_GET,
        FHttpRequestHandler::CreateLambda([this](const FHttpServerRequest& Req, const FHttpResultCallback& OnComplete) -> bool { return HandleListVariables(Req, OnComplete); }));

    Router->BindRoute(FHttpPath(TEXT("/blueprints/:path/variables")), EHttpServerRequestVerbs::VERB_POST,
        FHttpRequestHandler::CreateLambda([this](const FHttpServerRequest& Req, const FHttpResultCallback& OnComplete) -> bool { return HandleCreateVariable(Req, OnComplete); }));

    Router->BindRoute(FHttpPath(TEXT("/blueprints/:path/variables/:varname")), EHttpServerRequestVerbs::VERB_PUT,
        FHttpRequestHandler::CreateLambda([this](const FHttpServerRequest& Req, const FHttpResultCallback& OnComplete) -> bool { return HandleUpdateVariable(Req, OnComplete); }));

    Router->BindRoute(FHttpPath(TEXT("/blueprints/:path/variables/:varname")), EHttpServerRequestVerbs::VERB_DELETE,
        FHttpRequestHandler::CreateLambda([this](const FHttpServerRequest& Req, const FHttpResultCallback& OnComplete) -> bool { return HandleDeleteVariable(Req, OnComplete); }));

    Router->BindRoute(FHttpPath(TEXT("/blueprints/:path/functions")), EHttpServerRequestVerbs::VERB_GET,
        FHttpRequestHandler::CreateLambda([this](const FHttpServerRequest& Req, const FHttpResultCallback& OnComplete) -> bool { return HandleListFunctions(Req, OnComplete); }));

    Router->BindRoute(FHttpPath(TEXT("/blueprints/:path/functions")), EHttpServerRequestVerbs::VERB_POST,
        FHttpRequestHandler::CreateLambda([this](const FHttpServerRequest& Req, const FHttpResultCallback& OnComplete) -> bool { return HandleCreateFunction(Req, OnComplete); }));

    Router->BindRoute(FHttpPath(TEXT("/blueprints/:path/functions/:funcname")), EHttpServerRequestVerbs::VERB_DELETE,
        FHttpRequestHandler::CreateLambda([this](const FHttpServerRequest& Req, const FHttpResultCallback& OnComplete) -> bool { return HandleDeleteFunction(Req, OnComplete); }));

    Router->BindRoute(FHttpPath(TEXT("/blueprints/:path/graph")), EHttpServerRequestVerbs::VERB_GET,
        FHttpRequestHandler::CreateLambda([this](const FHttpServerRequest& Req, const FHttpResultCallback& OnComplete) -> bool { return HandleGetGraph(Req, OnComplete); }));

    Router->BindRoute(FHttpPath(TEXT("/blueprints/:path/nodes")), EHttpServerRequestVerbs::VERB_POST,
        FHttpRequestHandler::CreateLambda([this](const FHttpServerRequest& Req, const FHttpResultCallback& OnComplete) -> bool { return HandleAddNode(Req, OnComplete); }));

    Router->BindRoute(FHttpPath(TEXT("/blueprints/:path/nodes/:nodeid")), EHttpServerRequestVerbs::VERB_DELETE,
        FHttpRequestHandler::CreateLambda([this](const FHttpServerRequest& Req, const FHttpResultCallback& OnComplete) -> bool { return HandleDeleteNode(Req, OnComplete); }));

    Router->BindRoute(FHttpPath(TEXT("/blueprints/:path/connections")), EHttpServerRequestVerbs::VERB_POST,
        FHttpRequestHandler::CreateLambda([this](const FHttpServerRequest& Req, const FHttpResultCallback& OnComplete) -> bool { return HandleConnectNodes(Req, OnComplete); }));

    Router->BindRoute(FHttpPath(TEXT("/blueprints/:path/events")), EHttpServerRequestVerbs::VERB_POST,
        FHttpRequestHandler::CreateLambda([this](const FHttpServerRequest& Req, const FHttpResultCallback& OnComplete) -> bool { return HandleAddEvent(Req, OnComplete); }));

    Router->BindRoute(FHttpPath(TEXT("/blueprints/:path/nodes/:nodeid/position")), EHttpServerRequestVerbs::VERB_PUT,
        FHttpRequestHandler::CreateLambda([this](const FHttpServerRequest& Req, const FHttpResultCallback& OnComplete) -> bool { return HandleSetNodePosition(Req, OnComplete); }));

    Router->BindRoute(FHttpPath(TEXT("/blueprints/:path/reparent")), EHttpServerRequestVerbs::VERB_POST,
        FHttpRequestHandler::CreateLambda([this](const FHttpServerRequest& Req, const FHttpResultCallback& OnComplete) -> bool { return HandleReparentBlueprint(Req, OnComplete); }));

    Router->BindRoute(FHttpPath(TEXT("/blueprints/:path/functions/:funcname/inputs")), EHttpServerRequestVerbs::VERB_POST,
        FHttpRequestHandler::CreateLambda([this](const FHttpServerRequest& Req, const FHttpResultCallback& OnComplete) -> bool { return HandleAddFunctionInput(Req, OnComplete); }));

    Router->BindRoute(FHttpPath(TEXT("/blueprints/:path/pindefault")), EHttpServerRequestVerbs::VERB_POST,
        FHttpRequestHandler::CreateLambda([this](const FHttpServerRequest& Req, const FHttpResultCallback& OnComplete) -> bool { return HandleSetPinDefault(Req, OnComplete); }));
}

bool UCBlueprintHandler::HandleListBlueprints(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
{
    FString PathFilter = Request.QueryParams.FindRef(TEXT("path"));

    RunOnGameThread(OnComplete, [PathFilter]() -> FString
    {
        FAssetRegistryModule& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
        FARFilter Filter;
        Filter.ClassPaths.Add(UBlueprint::StaticClass()->GetClassPathName());
        Filter.bRecursivePaths = true;
        Filter.PackagePaths.Add(!PathFilter.IsEmpty() ? FName(*PathFilter) : FName("/Game"));

        TArray<FAssetData> Assets;
        AssetRegistry.Get().GetAssets(Filter, Assets);

        TArray<TSharedPtr<FJsonValue>> BPArray;
        for (const FAssetData& Asset : Assets)
        {
            TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
            Obj->SetStringField(TEXT("name"), Asset.AssetName.ToString());
            Obj->SetStringField(TEXT("path"), Asset.PackagePath.ToString());
            Obj->SetStringField(TEXT("full_path"), Asset.GetSoftObjectPath().ToString());
            BPArray.Add(MakeShared<FJsonValueObject>(Obj));
        }

        TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
        Data->SetArrayField(TEXT("blueprints"), BPArray);
        Data->SetNumberField(TEXT("count"), BPArray.Num());
        return MakeSuccessResponse(Data);
    });
    return true;
}

bool UCBlueprintHandler::HandleCreateBlueprint(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
{
    TSharedPtr<FJsonObject> Body = ParseJsonBody(Request);
    if (!Body.IsValid())
    {
        auto Resp = JsonResponse(MakeErrorResponse(TEXT("Invalid JSON body")), EHttpServerResponseCodes::BadRequest);
        OnComplete(MoveTemp(Resp));
        return true;
    }

    FString BlueprintName = Body->GetStringField(TEXT("name"));
    FString PackagePath   = Body->GetStringField(TEXT("path"));
    FString ParentClass   = Body->GetStringField(TEXT("parent_class"));

    RunOnGameThread(OnComplete, [BlueprintName, PackagePath, ParentClass]() -> FString
    {
        if (BlueprintName.IsEmpty() || PackagePath.IsEmpty())
        {
            return MakeErrorResponse(TEXT("name and path are required"));
        }

        // Find parent class
        UClass* Parent = AActor::StaticClass();
        if (!ParentClass.IsEmpty())
        {
            for (TObjectIterator<UClass> ClassIt; ClassIt; ++ClassIt)
            {
                if (ClassIt->GetName() == ParentClass)
                {
                    Parent = *ClassIt;
                    break;
                }
            }
        }

        IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
        UBlueprintFactory* Factory = NewObject<UBlueprintFactory>();
        Factory->ParentClass = Parent;

        UObject* Asset = AssetTools.CreateAsset(BlueprintName, PackagePath, UBlueprint::StaticClass(), Factory);
        if (!Asset)
        {
            return MakeErrorResponse(TEXT("Failed to create blueprint"));
        }

        UBlueprint* BP = Cast<UBlueprint>(Asset);
        if (!BP) return MakeErrorResponse(TEXT("Created asset is not a Blueprint"));

        FKismetEditorUtilities::CompileBlueprint(BP);

        TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
        Data->SetStringField(TEXT("name"), BlueprintName);
        Data->SetStringField(TEXT("path"), PackagePath + TEXT("/") + BlueprintName);
        Data->SetStringField(TEXT("parent"), Parent->GetName());
        return MakeSuccessResponse(Data);
    });
    return true;
}

bool UCBlueprintHandler::HandleReadBlueprint(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
{
    FString EncodedPath = Request.PathParams.FindRef(TEXT("path"));

    RunOnGameThread(OnComplete, [EncodedPath]() -> FString
    {
        FString BPPath = DecodeBlueprintPath(EncodedPath);
        UBlueprint* BP = FindBlueprint(BPPath);
        if (!BP) return MakeErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BPPath));

        TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
        Data->SetStringField(TEXT("name"), BP->GetName());
        Data->SetStringField(TEXT("path"), BP->GetPathName());
        Data->SetStringField(TEXT("parent_class"), BP->ParentClass ? BP->ParentClass->GetName() : TEXT(""));

        // Variables
        TArray<TSharedPtr<FJsonValue>> VarArray;
        for (const FBPVariableDescription& Var : BP->NewVariables)
        {
            VarArray.Add(MakeShared<FJsonValueObject>(VariableToJson(Var)));
        }
        Data->SetArrayField(TEXT("variables"), VarArray);

        // Functions
        TArray<TSharedPtr<FJsonValue>> FuncArray;
        for (UEdGraph* Graph : BP->FunctionGraphs)
        {
            TSharedPtr<FJsonObject> FuncObj = MakeShared<FJsonObject>();
            FuncObj->SetStringField(TEXT("name"), Graph->GetName());
            FuncObj->SetNumberField(TEXT("node_count"), Graph->Nodes.Num());
            FuncArray.Add(MakeShared<FJsonValueObject>(FuncObj));
        }
        Data->SetArrayField(TEXT("functions"), FuncArray);

        // Event Graphs
        TArray<TSharedPtr<FJsonValue>> EventArray;
        for (UEdGraph* Graph : BP->UbergraphPages)
        {
            TSharedPtr<FJsonObject> EventObj = MakeShared<FJsonObject>();
            EventObj->SetStringField(TEXT("name"), Graph->GetName());
            EventObj->SetNumberField(TEXT("node_count"), Graph->Nodes.Num());
            EventArray.Add(MakeShared<FJsonValueObject>(EventObj));
        }
        Data->SetArrayField(TEXT("event_graphs"), EventArray);

        return MakeSuccessResponse(Data);
    });
    return true;
}

bool UCBlueprintHandler::HandleCompileBlueprint(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
{
    FString EncodedPath = Request.PathParams.FindRef(TEXT("path"));

    RunOnGameThread(OnComplete, [EncodedPath]() -> FString
    {
        FString BPPath = DecodeBlueprintPath(EncodedPath);
        UBlueprint* BP = FindBlueprint(BPPath);
        if (!BP) return MakeErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BPPath));

        FKismetEditorUtilities::CompileBlueprint(BP);

        TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
        Data->SetStringField(TEXT("name"), BP->GetName());
        Data->SetBoolField(TEXT("compiled"), true);
        Data->SetStringField(TEXT("status"), BP->Status == BS_UpToDate ? TEXT("UpToDate") : TEXT("Error"));
        return MakeSuccessResponse(Data);
    });
    return true;
}

bool UCBlueprintHandler::HandleListVariables(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
{
    FString EncodedPath = Request.PathParams.FindRef(TEXT("path"));

    RunOnGameThread(OnComplete, [EncodedPath]() -> FString
    {
        FString BPPath = DecodeBlueprintPath(EncodedPath);
        UBlueprint* BP = FindBlueprint(BPPath);
        if (!BP) return MakeErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BPPath));

        TArray<TSharedPtr<FJsonValue>> VarArray;
        for (const FBPVariableDescription& Var : BP->NewVariables)
        {
            VarArray.Add(MakeShared<FJsonValueObject>(VariableToJson(Var)));
        }

        TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
        Data->SetArrayField(TEXT("variables"), VarArray);
        Data->SetNumberField(TEXT("count"), VarArray.Num());
        return MakeSuccessResponse(Data);
    });
    return true;
}

bool UCBlueprintHandler::HandleCreateVariable(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
{
    FString EncodedPath = Request.PathParams.FindRef(TEXT("path"));
    TSharedPtr<FJsonObject> Body = ParseJsonBody(Request);

    RunOnGameThread(OnComplete, [EncodedPath, Body]() -> FString
    {
        FString BPPath = DecodeBlueprintPath(EncodedPath);
        UBlueprint* BP = FindBlueprint(BPPath);
        if (!BP) return MakeErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BPPath));

        FString VarName = Body->GetStringField(TEXT("name"));
        FString VarType = Body->GetStringField(TEXT("type"));
        bool bIsPublic = false;
        Body->TryGetBoolField(TEXT("is_public"), bIsPublic);

        if (VarName.IsEmpty()) return MakeErrorResponse(TEXT("Variable name is required"));

        FEdGraphPinType PinType = PinTypeFromString(VarType);

        FBlueprintEditorUtils::AddMemberVariable(BP, FName(*VarName), PinType);

        // Set default value
        FString DefaultValue;
        if (Body->TryGetStringField(TEXT("default_value"), DefaultValue))
        {
            int32 VarIdx = FBlueprintEditorUtils::FindNewVariableIndex(BP, FName(*VarName));
            if (VarIdx != INDEX_NONE)
            {
                BP->NewVariables[VarIdx].DefaultValue = DefaultValue;
            }
        }

        // Set flags
        if (bIsPublic)
        {
            FBlueprintEditorUtils::SetBlueprintVariableMetaData(BP, FName(*VarName), nullptr, FBlueprintMetadata::MD_ExposeOnSpawn, TEXT("true"));
        }

        FKismetEditorUtilities::CompileBlueprint(BP);

        TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
        Data->SetStringField(TEXT("name"), VarName);
        Data->SetStringField(TEXT("type"), VarType);
        return MakeSuccessResponse(Data);
    });
    return true;
}

bool UCBlueprintHandler::HandleUpdateVariable(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
{
    FString EncodedPath = Request.PathParams.FindRef(TEXT("path"));
    FString VarName     = Request.PathParams.FindRef(TEXT("varname"));
    TSharedPtr<FJsonObject> Body = ParseJsonBody(Request);

    RunOnGameThread(OnComplete, [EncodedPath, VarName, Body]() -> FString
    {
        FString BPPath = DecodeBlueprintPath(EncodedPath);
        UBlueprint* BP = FindBlueprint(BPPath);
        if (!BP) return MakeErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BPPath));

        int32 VarIdx = FBlueprintEditorUtils::FindNewVariableIndex(BP, FName(*VarName));
        if (VarIdx == INDEX_NONE) return MakeErrorResponse(FString::Printf(TEXT("Variable not found: %s"), *VarName));

        FString DefaultValue;
        if (Body->TryGetStringField(TEXT("default_value"), DefaultValue))
        {
            BP->NewVariables[VarIdx].DefaultValue = DefaultValue;
        }

        FString NewName;
        if (Body->TryGetStringField(TEXT("new_name"), NewName) && !NewName.IsEmpty())
        {
            FBlueprintEditorUtils::RenameMemberVariable(BP, FName(*VarName), FName(*NewName));
        }

        FKismetEditorUtilities::CompileBlueprint(BP);
        return MakeSuccessResponse(TEXT("Variable updated"));
    });
    return true;
}

bool UCBlueprintHandler::HandleDeleteVariable(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
{
    FString EncodedPath = Request.PathParams.FindRef(TEXT("path"));
    FString VarName     = Request.PathParams.FindRef(TEXT("varname"));

    RunOnGameThread(OnComplete, [EncodedPath, VarName]() -> FString
    {
        FString BPPath = DecodeBlueprintPath(EncodedPath);
        UBlueprint* BP = FindBlueprint(BPPath);
        if (!BP) return MakeErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BPPath));

        FBlueprintEditorUtils::RemoveMemberVariable(BP, FName(*VarName));
        FKismetEditorUtilities::CompileBlueprint(BP);
        return MakeSuccessResponse(FString::Printf(TEXT("Variable '%s' deleted"), *VarName));
    });
    return true;
}

bool UCBlueprintHandler::HandleListFunctions(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
{
    FString EncodedPath = Request.PathParams.FindRef(TEXT("path"));

    RunOnGameThread(OnComplete, [EncodedPath]() -> FString
    {
        FString BPPath = DecodeBlueprintPath(EncodedPath);
        UBlueprint* BP = FindBlueprint(BPPath);
        if (!BP) return MakeErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BPPath));

        TArray<TSharedPtr<FJsonValue>> FuncArray;
        for (UEdGraph* Graph : BP->FunctionGraphs)
        {
            TSharedPtr<FJsonObject> FuncObj = MakeShared<FJsonObject>();
            FuncObj->SetStringField(TEXT("name"), Graph->GetName());
            FuncObj->SetNumberField(TEXT("node_count"), Graph->Nodes.Num());

            TArray<TSharedPtr<FJsonValue>> NodeArr;
            for (UEdGraphNode* Node : Graph->Nodes)
            {
                NodeArr.Add(MakeShared<FJsonValueObject>(NodeToJson(Node)));
            }
            FuncObj->SetArrayField(TEXT("nodes"), NodeArr);
            FuncArray.Add(MakeShared<FJsonValueObject>(FuncObj));
        }

        TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
        Data->SetArrayField(TEXT("functions"), FuncArray);
        return MakeSuccessResponse(Data);
    });
    return true;
}

bool UCBlueprintHandler::HandleCreateFunction(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
{
    FString EncodedPath = Request.PathParams.FindRef(TEXT("path"));
    TSharedPtr<FJsonObject> Body = ParseJsonBody(Request);

    RunOnGameThread(OnComplete, [EncodedPath, Body]() -> FString
    {
        FString BPPath = DecodeBlueprintPath(EncodedPath);
        UBlueprint* BP = FindBlueprint(BPPath);
        if (!BP) return MakeErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BPPath));

        FString FuncName = Body->GetStringField(TEXT("name"));
        if (FuncName.IsEmpty()) return MakeErrorResponse(TEXT("Function name is required"));

        UEdGraph* NewGraph = FBlueprintEditorUtils::CreateNewGraph(
            BP, FName(*FuncName), UEdGraph::StaticClass(), UEdGraphSchema_K2::StaticClass());

        if (!NewGraph) return MakeErrorResponse(TEXT("Failed to create function graph"));

        FBlueprintEditorUtils::AddFunctionGraph<UClass>(BP, NewGraph, false, nullptr);
        FKismetEditorUtilities::CompileBlueprint(BP);

        TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
        Data->SetStringField(TEXT("name"), FuncName);
        Data->SetNumberField(TEXT("node_count"), NewGraph->Nodes.Num());
        return MakeSuccessResponse(Data);
    });
    return true;
}

bool UCBlueprintHandler::HandleDeleteFunction(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
{
    FString EncodedPath = Request.PathParams.FindRef(TEXT("path"));
    FString FuncName    = Request.PathParams.FindRef(TEXT("funcname"));
    // occurrence: "all" (default) deletes all matching, "last" deletes only the last match
    FString Occurrence  = Request.QueryParams.FindRef(TEXT("occurrence"));

    RunOnGameThread(OnComplete, [EncodedPath, FuncName, Occurrence]() -> FString
    {
        FString BPPath = DecodeBlueprintPath(EncodedPath);
        UBlueprint* BP = FindBlueprint(BPPath);
        if (!BP) return MakeErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BPPath));

        TArray<UEdGraph*> ToDelete;
        for (UEdGraph* Graph : BP->FunctionGraphs)
        {
            if (Graph->GetName() == FuncName) ToDelete.Add(Graph);
        }

        if (ToDelete.IsEmpty())
            return MakeErrorResponse(FString::Printf(TEXT("Function not found: %s"), *FuncName));

        TArray<UEdGraph*> DeleteTarget;
        if (Occurrence == TEXT("last"))
            DeleteTarget.Add(ToDelete.Last());
        else
            DeleteTarget = ToDelete;

        for (UEdGraph* G : DeleteTarget)
            FBlueprintEditorUtils::RemoveGraph(BP, G, EGraphRemoveFlags::Recompile);

        FKismetEditorUtilities::CompileBlueprint(BP);
        return MakeSuccessResponse(FString::Printf(TEXT("Deleted %d graph(s) named '%s'"), DeleteTarget.Num(), *FuncName));
    });
    return true;
}

bool UCBlueprintHandler::HandleGetGraph(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
{
    FString EncodedPath = Request.PathParams.FindRef(TEXT("path"));
    FString GraphName   = Request.QueryParams.FindRef(TEXT("graph"));

    RunOnGameThread(OnComplete, [EncodedPath, GraphName]() -> FString
    {
        FString BPPath = DecodeBlueprintPath(EncodedPath);
        UBlueprint* BP = FindBlueprint(BPPath);
        if (!BP) return MakeErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BPPath));

        UEdGraph* TargetGraph = nullptr;
        if (GraphName.IsEmpty() && BP->UbergraphPages.Num() > 0)
        {
            TargetGraph = BP->UbergraphPages[0];
        }
        else
        {
            for (UEdGraph* Graph : BP->UbergraphPages)
            {
                if (Graph->GetName() == GraphName) { TargetGraph = Graph; break; }
            }
            for (UEdGraph* Graph : BP->FunctionGraphs)
            {
                if (Graph->GetName() == GraphName) { TargetGraph = Graph; break; }
            }
            if (!TargetGraph)
            {
                TArray<UEdGraph*> AllGraphs;
                BP->GetAllGraphs(AllGraphs);
                for (UEdGraph* Graph : AllGraphs)
                {
                    if (Graph && Graph->GetName() == GraphName) { TargetGraph = Graph; break; }
                }
            }
        }

        if (!TargetGraph) return MakeErrorResponse(TEXT("Graph not found"));

        TArray<TSharedPtr<FJsonValue>> NodeArray;
        for (UEdGraphNode* Node : TargetGraph->Nodes)
        {
            NodeArray.Add(MakeShared<FJsonValueObject>(NodeToJson(Node)));
        }

        TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
        Data->SetStringField(TEXT("graph"), TargetGraph->GetName());
        Data->SetArrayField(TEXT("nodes"), NodeArray);
        Data->SetNumberField(TEXT("node_count"), NodeArray.Num());
        return MakeSuccessResponse(Data);
    });
    return true;
}

bool UCBlueprintHandler::HandleAddNode(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
{
    FString EncodedPath = Request.PathParams.FindRef(TEXT("path"));
    TSharedPtr<FJsonObject> Body = ParseJsonBody(Request);

    RunOnGameThread(OnComplete, [EncodedPath, Body]() -> FString
    {
        FString BPPath = DecodeBlueprintPath(EncodedPath);
        UBlueprint* BP = FindBlueprint(BPPath);
        if (!BP) return MakeErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BPPath));

        FString NodeType  = Body->GetStringField(TEXT("node_type"));
        FString GraphName = Body->GetStringField(TEXT("graph"));
        float PosX = 0.f, PosY = 0.f;
        Body->TryGetNumberField(TEXT("x"), PosX);
        Body->TryGetNumberField(TEXT("y"), PosY);

        UEdGraph* TargetGraph = nullptr;
        if (GraphName.IsEmpty() && BP->UbergraphPages.Num() > 0)
            TargetGraph = BP->UbergraphPages[0];
        else
        {
            for (UEdGraph* G : BP->UbergraphPages) { if (G->GetName() == GraphName) { TargetGraph = G; break; } }
            for (UEdGraph* G : BP->FunctionGraphs) { if (G->GetName() == GraphName) { TargetGraph = G; break; } }
        }
        if (!TargetGraph) return MakeErrorResponse(TEXT("Graph not found"));

        UEdGraphNode* NewNode = nullptr;

        if (NodeType == TEXT("VariableGet"))
        {
            FString VarName = Body->GetStringField(TEXT("variable_name"));
            UK2Node_VariableGet* Node = NewObject<UK2Node_VariableGet>(TargetGraph);
            Node->VariableReference.SetSelfMember(FName(*VarName));
            TargetGraph->AddNode(Node, true, false);
            Node->NodePosX = PosX; Node->NodePosY = PosY;
            Node->AllocateDefaultPins();
            NewNode = Node;
        }
        else if (NodeType == TEXT("VariableSet"))
        {
            FString VarName = Body->GetStringField(TEXT("variable_name"));
            UK2Node_VariableSet* Node = NewObject<UK2Node_VariableSet>(TargetGraph);
            Node->VariableReference.SetSelfMember(FName(*VarName));
            TargetGraph->AddNode(Node, true, false);
            Node->NodePosX = PosX; Node->NodePosY = PosY;
            Node->AllocateDefaultPins();
            NewNode = Node;
        }
        else if (NodeType == TEXT("CallFunction"))
        {
            FString FuncName = Body->GetStringField(TEXT("function_name"));
            FString ClassName = Body->GetStringField(TEXT("class_name"));
            UK2Node_CallFunction* Node = NewObject<UK2Node_CallFunction>(TargetGraph);
            UClass* TargetClass = nullptr;
            for (TObjectIterator<UClass> ClassIt; ClassIt; ++ClassIt)
            {
                if (ClassIt->GetName() == ClassName) { TargetClass = *ClassIt; break; }
            }
            if (TargetClass)
            {
                UFunction* Func = TargetClass->FindFunctionByName(FName(*FuncName));
                if (Func) Node->SetFromFunction(Func);
            }
            TargetGraph->AddNode(Node, true, false);
            Node->NodePosX = PosX; Node->NodePosY = PosY;
            Node->AllocateDefaultPins();
            NewNode = Node;
        }

        else if (NodeType == TEXT("Branch"))
        {
            UK2Node_IfThenElse* Node = NewObject<UK2Node_IfThenElse>(TargetGraph);
            TargetGraph->AddNode(Node, true, false);
            Node->NodePosX = PosX; Node->NodePosY = PosY;
            Node->AllocateDefaultPins();
            NewNode = Node;
        }
        else if (NodeType == TEXT("PromotableOperator"))
        {
            FString FuncName  = Body->GetStringField(TEXT("function_name"));
            FString ClassName = Body->GetStringField(TEXT("class_name"));
            UK2Node_PromotableOperator* Node = NewObject<UK2Node_PromotableOperator>(TargetGraph);
            UClass* TargetClass = nullptr;
            for (TObjectIterator<UClass> ClassIt; ClassIt; ++ClassIt)
            {
                if (ClassIt->GetName() == ClassName) { TargetClass = *ClassIt; break; }
            }
            if (TargetClass)
            {
                UFunction* Func = TargetClass->FindFunctionByName(FName(*FuncName));
                if (Func) Node->SetFromFunction(Func);
            }
            TargetGraph->AddNode(Node, true, false);
            Node->NodePosX = PosX; Node->NodePosY = PosY;
            Node->AllocateDefaultPins();
            NewNode = Node;
        }

        if (!NewNode) return MakeErrorResponse(FString::Printf(TEXT("Unknown node type or creation failed: %s"), *NodeType));

        FKismetEditorUtilities::CompileBlueprint(BP);

        TSharedPtr<FJsonObject> Data = NodeToJson(NewNode);
        return MakeSuccessResponse(Data);
    });
    return true;
}

bool UCBlueprintHandler::HandleDeleteNode(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
{
    FString EncodedPath = Request.PathParams.FindRef(TEXT("path"));
    FString NodeIdStr   = Request.PathParams.FindRef(TEXT("nodeid"));

    RunOnGameThread(OnComplete, [EncodedPath, NodeIdStr]() -> FString
    {
        FString BPPath = DecodeBlueprintPath(EncodedPath);
        UBlueprint* BP = FindBlueprint(BPPath);
        if (!BP) return MakeErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BPPath));

        auto NodeMatchesId = [](UEdGraphNode* Node, const FString& Id) -> bool
        {
            return Node->NodeGuid.ToString() == Id
                || Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString() == Id
                || Node->GetNodeTitle(ENodeTitleType::ListView).ToString() == Id;
        };

        // Search all graphs for node by GUID or title
        for (UEdGraph* Graph : BP->UbergraphPages)
        {
            for (UEdGraphNode* Node : Graph->Nodes)
            {
                if (NodeMatchesId(Node, NodeIdStr))
                {
                    FBlueprintEditorUtils::RemoveNode(BP, Node, true);
                    FKismetEditorUtilities::CompileBlueprint(BP);
                    return MakeSuccessResponse(TEXT("Node deleted"));
                }
            }
        }
        for (UEdGraph* Graph : BP->FunctionGraphs)
        {
            for (UEdGraphNode* Node : Graph->Nodes)
            {
                if (NodeMatchesId(Node, NodeIdStr))
                {
                    FBlueprintEditorUtils::RemoveNode(BP, Node, true);
                    FKismetEditorUtilities::CompileBlueprint(BP);
                    return MakeSuccessResponse(TEXT("Node deleted"));
                }
            }
        }

        return MakeErrorResponse(FString::Printf(TEXT("Node not found: %s"), *NodeIdStr));
    });
    return true;
}

bool UCBlueprintHandler::HandleConnectNodes(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
{
    FString EncodedPath = Request.PathParams.FindRef(TEXT("path"));
    TSharedPtr<FJsonObject> Body = ParseJsonBody(Request);

    RunOnGameThread(OnComplete, [EncodedPath, Body]() -> FString
    {
        FString BPPath = DecodeBlueprintPath(EncodedPath);
        UBlueprint* BP = FindBlueprint(BPPath);
        if (!BP) return MakeErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BPPath));

        FString SourceNodeId  = Body->GetStringField(TEXT("source_node"));
        FString SourcePinName = Body->GetStringField(TEXT("source_pin"));
        FString TargetNodeId  = Body->GetStringField(TEXT("target_node"));
        FString TargetPinName = Body->GetStringField(TEXT("target_pin"));
        FString GraphName     = Body->GetStringField(TEXT("graph"));

        // Gather all graphs
        TArray<UEdGraph*> AllGraphs;
        AllGraphs.Append(BP->UbergraphPages);
        AllGraphs.Append(BP->FunctionGraphs);

        UEdGraphNode* SourceNode = nullptr;
        UEdGraphNode* TargetNode = nullptr;

        auto NodeMatchesId = [](UEdGraphNode* Node, const FString& Id) -> bool
        {
            return Node->NodeGuid.ToString() == Id
                || Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString() == Id
                || Node->GetNodeTitle(ENodeTitleType::ListView).ToString() == Id;
        };

        for (UEdGraph* Graph : AllGraphs)
        {
            if (!GraphName.IsEmpty() && Graph->GetName() != GraphName) continue;
            for (UEdGraphNode* Node : Graph->Nodes)
            {
                if (NodeMatchesId(Node, SourceNodeId)) SourceNode = Node;
                if (NodeMatchesId(Node, TargetNodeId)) TargetNode = Node;
            }
            if (SourceNode && TargetNode) break;
        }

        if (!SourceNode) return MakeErrorResponse(FString::Printf(TEXT("Source node not found: %s"), *SourceNodeId));
        if (!TargetNode) return MakeErrorResponse(FString::Printf(TEXT("Target node not found: %s"), *TargetNodeId));

        UEdGraphPin* SourcePin = SourceNode->FindPin(FName(*SourcePinName), EGPD_Output);
        UEdGraphPin* TargetPin = TargetNode->FindPin(FName(*TargetPinName), EGPD_Input);

        if (!SourcePin) return MakeErrorResponse(FString::Printf(TEXT("Source pin not found: %s"), *SourcePinName));
        if (!TargetPin) return MakeErrorResponse(FString::Printf(TEXT("Target pin not found: %s"), *TargetPinName));

        const UEdGraphSchema* Schema = SourceNode->GetGraph()->GetSchema();
        FPinConnectionResponse Response = Schema->CanCreateConnection(SourcePin, TargetPin);
        if (Response.Response == CONNECT_RESPONSE_DISALLOW)
        {
            return MakeErrorResponse(FString::Printf(TEXT("Cannot connect pins: %s"), *Response.Message.ToString()));
        }

        Schema->TryCreateConnection(SourcePin, TargetPin);
        FKismetEditorUtilities::CompileBlueprint(BP);

        return MakeSuccessResponse(TEXT("Nodes connected"));
    });
    return true;
}

bool UCBlueprintHandler::HandleAddEvent(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
{
    FString EncodedPath = Request.PathParams.FindRef(TEXT("path"));
    TSharedPtr<FJsonObject> Body = ParseJsonBody(Request);

    RunOnGameThread(OnComplete, [EncodedPath, Body]() -> FString
    {
        FString BPPath = DecodeBlueprintPath(EncodedPath);
        UBlueprint* BP = FindBlueprint(BPPath);
        if (!BP) return MakeErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BPPath));

        FString EventName = Body->GetStringField(TEXT("event_name"));
        float PosX = 0.f, PosY = 0.f;
        Body->TryGetNumberField(TEXT("x"), PosX);
        Body->TryGetNumberField(TEXT("y"), PosY);

        if (BP->UbergraphPages.Num() == 0) return MakeErrorResponse(TEXT("No event graph found"));
        UEdGraph* EventGraph = BP->UbergraphPages[0];

        // 이미 그래프에 같은 이름의 이벤트 노드가 있으면 재사용 (FindEventNode 대체)
        UK2Node_Event* EventNode = nullptr;
        for (UEdGraphNode* Node : EventGraph->Nodes)
        {
            UK2Node_Event* Candidate = Cast<UK2Node_Event>(Node);
            if (Candidate && Candidate->EventReference.GetMemberName() == FName(*EventName))
            {
                EventNode = Candidate;
                break;
            }
        }

        // 빌트인 이벤트 (BeginPlay, Tick 등) 추가 시도
        // UE5.7: AddDefaultEventNode 시그니처 → (BP, Graph, EventName, EventClass, int32& OutNodePosY)
        if (!EventNode)
        {
            UClass* BPClass = BP->GeneratedClass ? BP->GeneratedClass : BP->ParentClass;
            if (BPClass)
            {
                UFunction* EventFunc = BPClass->FindFunctionByName(FName(*EventName));
                if (EventFunc && EventFunc->HasAnyFunctionFlags(FUNC_BlueprintEvent))
                {
                    int32 NodePosY = (int32)PosY;
                    EventNode = FKismetEditorUtilities::AddDefaultEventNode(
                        BP, EventGraph, FName(*EventName), nullptr, NodePosY);
                }
            }
        }

        // 커스텀 이벤트로 생성 (빌트인이 아닌 경우)
        if (!EventNode)
        {
            int32 NodePosY = (int32)PosY;
            UK2Node_CustomEvent* CustomEvent = Cast<UK2Node_CustomEvent>(
                FKismetEditorUtilities::AddDefaultEventNode(
                    BP, EventGraph, FName(*EventName), nullptr, NodePosY));

            if (!CustomEvent)
            {
                // 직접 CustomEvent 노드 생성
                UK2Node_CustomEvent* NewCustomEvent = NewObject<UK2Node_CustomEvent>(EventGraph);
                NewCustomEvent->CustomFunctionName = FName(*EventName);
                EventGraph->AddNode(NewCustomEvent, true, false);
                NewCustomEvent->NodePosX = (int32)PosX;
                NewCustomEvent->NodePosY = (int32)PosY;
                NewCustomEvent->AllocateDefaultPins();
                EventNode = NewCustomEvent;
            }
            else
            {
                EventNode = CustomEvent;
            }
        }

        if (!EventNode) return MakeErrorResponse(TEXT("Failed to create event node"));

        EventNode->NodePosX = PosX;
        EventNode->NodePosY = PosY;
        FKismetEditorUtilities::CompileBlueprint(BP);

        return MakeSuccessResponse(NodeToJson(EventNode));
    });
    return true;
}

bool UCBlueprintHandler::HandleSetNodePosition(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
{
    FString EncodedPath = Request.PathParams.FindRef(TEXT("path"));
    FString NodeId      = Request.PathParams.FindRef(TEXT("nodeid"));
    TSharedPtr<FJsonObject> Body = ParseJsonBody(Request);

    RunOnGameThread(OnComplete, [EncodedPath, NodeId, Body]() -> FString
    {
        FString BPPath = DecodeBlueprintPath(EncodedPath);
        UBlueprint* BP = FindBlueprint(BPPath);
        if (!BP) return MakeErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BPPath));

        float PosX = 0.f, PosY = 0.f;
        Body->TryGetNumberField(TEXT("x"), PosX);
        Body->TryGetNumberField(TEXT("y"), PosY);

        TArray<UEdGraph*> AllGraphs;
        AllGraphs.Append(BP->UbergraphPages);
        AllGraphs.Append(BP->FunctionGraphs);

        auto NodeMatchesId = [](UEdGraphNode* Node, const FString& Id) -> bool
        {
            return Node->NodeGuid.ToString() == Id
                || Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString() == Id
                || Node->GetNodeTitle(ENodeTitleType::ListView).ToString() == Id;
        };

        for (UEdGraph* Graph : AllGraphs)
        {
            for (UEdGraphNode* Node : Graph->Nodes)
            {
                if (NodeMatchesId(Node, NodeId))
                {
                    Node->NodePosX = PosX;
                    Node->NodePosY = PosY;
                    return MakeSuccessResponse(TEXT("Node position updated"));
                }
            }
        }
        return MakeErrorResponse(FString::Printf(TEXT("Node not found: %s"), *NodeId));
    });
    return true;
}

bool UCBlueprintHandler::HandleReparentBlueprint(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
{
    FString EncodedPath = Request.PathParams.FindRef(TEXT("path"));
    TSharedPtr<FJsonObject> Body = ParseJsonBody(Request);

    RunOnGameThread(OnComplete, [EncodedPath, Body]() -> FString
    {
        if (!Body.IsValid()) return MakeErrorResponse(TEXT("Invalid or missing request body"));

        FString BPPath = DecodeBlueprintPath(EncodedPath);
        UBlueprint* BP = FindBlueprint(BPPath);
        if (!BP) return MakeErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BPPath));

        FString NewParentName = Body->GetStringField(TEXT("parent_class"));
        if (NewParentName.IsEmpty()) return MakeErrorResponse(TEXT("parent_class is required"));

        UClass* NewParent = nullptr;
        for (TObjectIterator<UClass> ClassIt; ClassIt; ++ClassIt)
        {
            if (ClassIt->GetName() == NewParentName) { NewParent = *ClassIt; break; }
        }
        if (!NewParent) return MakeErrorResponse(FString::Printf(TEXT("Class not found: %s"), *NewParentName));

        BP->Modify();
        BP->ParentClass = NewParent;
        FBlueprintEditorUtils::RefreshAllNodes(BP);
        FKismetEditorUtilities::CompileBlueprint(BP);

        return MakeSuccessResponse(FString::Printf(TEXT("Blueprint reparented to %s"), *NewParentName));
    });
    return true;
}

bool UCBlueprintHandler::HandleAddFunctionInput(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
{
    FString EncodedPath  = Request.PathParams.FindRef(TEXT("path"));
    FString FuncNameParam = Request.PathParams.FindRef(TEXT("funcname"));
    TSharedPtr<FJsonObject> Body = ParseJsonBody(Request);

    RunOnGameThread(OnComplete, [EncodedPath, FuncNameParam, Body]() -> FString
    {
        FString BPPath = DecodeBlueprintPath(EncodedPath);
        UBlueprint* BP = FindBlueprint(BPPath);
        if (!BP) return MakeErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BPPath));

        FString ParamName = Body->GetStringField(TEXT("name"));
        FString ParamType = Body->GetStringField(TEXT("type"));
        if (ParamName.IsEmpty()) return MakeErrorResponse(TEXT("Parameter name is required"));

        UEdGraph* FuncGraph = nullptr;
        for (UEdGraph* G : BP->FunctionGraphs)
            if (G->GetName() == FuncNameParam) { FuncGraph = G; break; }
        if (!FuncGraph) return MakeErrorResponse(FString::Printf(TEXT("Function not found: %s"), *FuncNameParam));

        UK2Node_FunctionEntry* FuncEntry = nullptr;
        for (UEdGraphNode* Node : FuncGraph->Nodes)
        {
            FuncEntry = Cast<UK2Node_FunctionEntry>(Node);
            if (FuncEntry) break;
        }
        if (!FuncEntry) return MakeErrorResponse(TEXT("FunctionEntry node not found"));

        FEdGraphPinType PinType = PinTypeFromString(ParamType);
        FuncEntry->Modify();
        FuncEntry->CreateUserDefinedPin(FName(*ParamName), PinType, EGPD_Output);
        FuncGraph->Modify();
        FBlueprintEditorUtils::MarkBlueprintAsModified(BP);
        FKismetEditorUtilities::CompileBlueprint(BP);

        TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
        Data->SetStringField(TEXT("param_name"), ParamName);
        Data->SetStringField(TEXT("param_type"), ParamType);
        return MakeSuccessResponse(Data);
    });
    return true;
}

bool UCBlueprintHandler::HandleSetPinDefault(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
{
    FString EncodedPath = Request.PathParams.FindRef(TEXT("path"));
    TSharedPtr<FJsonObject> Body = ParseJsonBody(Request);

    RunOnGameThread(OnComplete, [EncodedPath, Body]() -> FString
    {
        FString BPPath = DecodeBlueprintPath(EncodedPath);
        UBlueprint* BP = FindBlueprint(BPPath);
        if (!BP) return MakeErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BPPath));

        FString GraphName    = Body->GetStringField(TEXT("graph"));
        FString NodeTitle    = Body->GetStringField(TEXT("node"));
        FString PinName      = Body->GetStringField(TEXT("pin"));
        FString DefaultValue = Body->GetStringField(TEXT("value"));

        UEdGraph* TargetGraph = nullptr;
        for (UEdGraph* G : BP->UbergraphPages) { if (G->GetName() == GraphName) { TargetGraph = G; break; } }
        for (UEdGraph* G : BP->FunctionGraphs)  { if (G->GetName() == GraphName) { TargetGraph = G; break; } }
        if (!TargetGraph) return MakeErrorResponse(TEXT("Graph not found"));

        for (UEdGraphNode* Node : TargetGraph->Nodes)
        {
            FString Full = Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString();
            FString List = Node->GetNodeTitle(ENodeTitleType::ListView).ToString();
            if (Full != NodeTitle && List != NodeTitle && Node->NodeGuid.ToString() != NodeTitle) continue;

            for (UEdGraphPin* Pin : Node->Pins)
            {
                if (Pin->PinName.ToString() != PinName) continue;
                Pin->Modify();
                Pin->DefaultValue = DefaultValue;
                FBlueprintEditorUtils::MarkBlueprintAsModified(BP);
                FKismetEditorUtilities::CompileBlueprint(BP);
                return MakeSuccessResponse(FString::Printf(TEXT("Set %s.%s = %s"), *NodeTitle, *PinName, *DefaultValue));
            }
            return MakeErrorResponse(FString::Printf(TEXT("Pin not found: %s"), *PinName));
        }
        return MakeErrorResponse(FString::Printf(TEXT("Node not found: %s"), *NodeTitle));
    });
    return true;
}

// ---- Helpers ----

UBlueprint* UCBlueprintHandler::FindBlueprint(const FString& PackagePath)
{
    return LoadObject<UBlueprint>(nullptr, *PackagePath);
}

TSharedPtr<FJsonObject> UCBlueprintHandler::VariableToJson(const FBPVariableDescription& Var)
{
    TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
    Obj->SetStringField(TEXT("name"), Var.VarName.ToString());
    Obj->SetStringField(TEXT("type"), PinTypeToString(Var.VarType));
    Obj->SetStringField(TEXT("default_value"), Var.DefaultValue);
    Obj->SetStringField(TEXT("tooltip"), Var.MetaDataArray.Num() > 0 ? Var.MetaDataArray[0].DataValue : TEXT(""));
    return Obj;
}

TSharedPtr<FJsonObject> UCBlueprintHandler::NodeToJson(UEdGraphNode* Node)
{
    TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
    if (!Node) return Obj;

    Obj->SetStringField(TEXT("id"), Node->NodeGuid.ToString());
    Obj->SetStringField(TEXT("title"), Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString());
    Obj->SetStringField(TEXT("class"), Node->GetClass()->GetName());
    Obj->SetNumberField(TEXT("x"), Node->NodePosX);
    Obj->SetNumberField(TEXT("y"), Node->NodePosY);

    // Pins
    TArray<TSharedPtr<FJsonValue>> PinArray;
    for (UEdGraphPin* Pin : Node->Pins)
    {
        TSharedPtr<FJsonObject> PinObj = MakeShared<FJsonObject>();
        PinObj->SetStringField(TEXT("name"), Pin->PinName.ToString());
        PinObj->SetStringField(TEXT("direction"), Pin->Direction == EGPD_Input ? TEXT("Input") : TEXT("Output"));
        PinObj->SetStringField(TEXT("type"), Pin->PinType.PinCategory.ToString());
        PinObj->SetStringField(TEXT("default_value"), Pin->DefaultValue);

        TArray<TSharedPtr<FJsonValue>> LinkedToArray;
        for (UEdGraphPin* LinkedPin : Pin->LinkedTo)
        {
            if (!LinkedPin || !LinkedPin->GetOwningNode()) continue;
            TSharedPtr<FJsonObject> LinkObj = MakeShared<FJsonObject>();
            LinkObj->SetStringField(TEXT("node_id"), LinkedPin->GetOwningNode()->NodeGuid.ToString());
            LinkObj->SetStringField(TEXT("node_title"), LinkedPin->GetOwningNode()->GetNodeTitle(ENodeTitleType::FullTitle).ToString());
            LinkObj->SetStringField(TEXT("pin_name"), LinkedPin->PinName.ToString());
            LinkedToArray.Add(MakeShared<FJsonValueObject>(LinkObj));
        }
        PinObj->SetArrayField(TEXT("linked_to"), LinkedToArray);

        PinArray.Add(MakeShared<FJsonValueObject>(PinObj));
    }
    Obj->SetArrayField(TEXT("pins"), PinArray);

    return Obj;
}

FEdGraphPinType UCBlueprintHandler::PinTypeFromString(const FString& TypeStr)
{
    FEdGraphPinType PinType;
    const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();

    if (TypeStr == TEXT("bool"))       PinType.PinCategory = UEdGraphSchema_K2::PC_Boolean;
    else if (TypeStr == TEXT("int"))   PinType.PinCategory = UEdGraphSchema_K2::PC_Int;
    else if (TypeStr == TEXT("int64")) PinType.PinCategory = UEdGraphSchema_K2::PC_Int64;
    else if (TypeStr == TEXT("float")) PinType.PinCategory = UEdGraphSchema_K2::PC_Real;
    else if (TypeStr == TEXT("string")) PinType.PinCategory = UEdGraphSchema_K2::PC_String;
    else if (TypeStr == TEXT("name"))   PinType.PinCategory = UEdGraphSchema_K2::PC_Name;
    else if (TypeStr == TEXT("text"))   PinType.PinCategory = UEdGraphSchema_K2::PC_Text;
    else if (TypeStr == TEXT("vector"))
    {
        PinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
        PinType.PinSubCategoryObject = TBaseStructure<FVector>::Get();
    }
    else if (TypeStr == TEXT("rotator"))
    {
        PinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
        PinType.PinSubCategoryObject = TBaseStructure<FRotator>::Get();
    }
    else if (TypeStr == TEXT("transform"))
    {
        PinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
        PinType.PinSubCategoryObject = TBaseStructure<FTransform>::Get();
    }
    else if (TypeStr == TEXT("object"))
    {
        PinType.PinCategory = UEdGraphSchema_K2::PC_Object;
    }
    else
    {
        // Try to resolve as a named UScriptStruct (e.g. "AnimUpdateContext", "AnimNodeReference")
        if (UScriptStruct* FoundStruct = FindFirstObject<UScriptStruct>(*TypeStr))
        {
            PinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
            PinType.PinSubCategoryObject = FoundStruct;
        }
        else
        {
            PinType.PinCategory = UEdGraphSchema_K2::PC_String;
        }
    }
    return PinType;
}

FString UCBlueprintHandler::PinTypeToString(const FEdGraphPinType& PinType)
{
    if (PinType.PinCategory == UEdGraphSchema_K2::PC_Boolean)  return TEXT("bool");
    if (PinType.PinCategory == UEdGraphSchema_K2::PC_Int)      return TEXT("int");
    if (PinType.PinCategory == UEdGraphSchema_K2::PC_Int64)    return TEXT("int64");
    if (PinType.PinCategory == UEdGraphSchema_K2::PC_Real)     return TEXT("float");
    if (PinType.PinCategory == UEdGraphSchema_K2::PC_String)   return TEXT("string");
    if (PinType.PinCategory == UEdGraphSchema_K2::PC_Name)     return TEXT("name");
    if (PinType.PinCategory == UEdGraphSchema_K2::PC_Text)     return TEXT("text");
    if (PinType.PinCategory == UEdGraphSchema_K2::PC_Object)   return TEXT("object");
    if (PinType.PinCategory == UEdGraphSchema_K2::PC_Struct)
    {
        if (PinType.PinSubCategoryObject.IsValid())
            return PinType.PinSubCategoryObject->GetName();
    }
    return PinType.PinCategory.ToString();
}

FString UCBlueprintHandler::DecodeBlueprintPath(const FString& EncodedPath)
{
    // Replace URL-encoded slashes and convert to UE asset path
    FString Decoded = EncodedPath.Replace(TEXT("__"), TEXT("/"));
    if (!Decoded.StartsWith(TEXT("/"))) Decoded = TEXT("/") + Decoded;
    return Decoded;
}
