#include "Handlers/UCComponentHandler.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Components/ActorComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/BoxComponent.h"
#include "Components/SphereComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/AudioComponent.h"
#include "Particles/ParticleSystemComponent.h"
#include "Components/MeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "EngineUtils.h"
#include "Editor.h"
#include "HttpPath.h"
#include "UObject/UObjectIterator.h"

void UCComponentHandler::RegisterRoutes(TSharedRef<IHttpRouter> Router)
{
    Router->BindRoute(FHttpPath(TEXT("/actors/:label/components")), EHttpServerRequestVerbs::VERB_POST,
        FHttpRequestHandler::CreateLambda([this](const FHttpServerRequest& Req, const FHttpResultCallback& OnComplete) -> bool { return HandleAddComponent(Req, OnComplete); }));

    Router->BindRoute(FHttpPath(TEXT("/actors/:label/components/:component")), EHttpServerRequestVerbs::VERB_DELETE,
        FHttpRequestHandler::CreateLambda([this](const FHttpServerRequest& Req, const FHttpResultCallback& OnComplete) -> bool { return HandleRemoveComponent(Req, OnComplete); }));

    Router->BindRoute(FHttpPath(TEXT("/actors/:label/components/:component/properties")), EHttpServerRequestVerbs::VERB_GET,
        FHttpRequestHandler::CreateLambda([this](const FHttpServerRequest& Req, const FHttpResultCallback& OnComplete) -> bool { return HandleGetComponentProperties(Req, OnComplete); }));

    Router->BindRoute(FHttpPath(TEXT("/actors/:label/components/:component/properties")), EHttpServerRequestVerbs::VERB_PUT,
        FHttpRequestHandler::CreateLambda([this](const FHttpServerRequest& Req, const FHttpResultCallback& OnComplete) -> bool { return HandleSetComponentProperties(Req, OnComplete); }));

    Router->BindRoute(FHttpPath(TEXT("/actors/:label/components/:component/transform")), EHttpServerRequestVerbs::VERB_GET,
        FHttpRequestHandler::CreateLambda([this](const FHttpServerRequest& Req, const FHttpResultCallback& OnComplete) -> bool { return HandleGetComponentTransform(Req, OnComplete); }));

    Router->BindRoute(FHttpPath(TEXT("/actors/:label/components/:component/transform")), EHttpServerRequestVerbs::VERB_PUT,
        FHttpRequestHandler::CreateLambda([this](const FHttpServerRequest& Req, const FHttpResultCallback& OnComplete) -> bool { return HandleSetComponentTransform(Req, OnComplete); }));

    Router->BindRoute(FHttpPath(TEXT("/actors/:label/components/:component/material")), EHttpServerRequestVerbs::VERB_POST,
        FHttpRequestHandler::CreateLambda([this](const FHttpServerRequest& Req, const FHttpResultCallback& OnComplete) -> bool { return HandleApplyMaterialToComponent(Req, OnComplete); }));

    Router->BindRoute(FHttpPath(TEXT("/components/classes")), EHttpServerRequestVerbs::VERB_GET,
        FHttpRequestHandler::CreateLambda([this](const FHttpServerRequest& Req, const FHttpResultCallback& OnComplete) -> bool { return HandleListComponentClasses(Req, OnComplete); }));
}

bool UCComponentHandler::HandleListComponents(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
{
    FString Label = Request.PathParams.FindRef(TEXT("label"));

    RunOnGameThread(OnComplete, [Label]() -> FString
    {
        UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
        if (!World) return MakeErrorResponse(TEXT("No editor world"));

        AActor* Actor = nullptr;
        for (TActorIterator<AActor> It(World); It; ++It)
        {
            if ((*It)->GetActorLabel() == Label) { Actor = *It; break; }
        }
        if (!Actor) return MakeErrorResponse(FString::Printf(TEXT("Actor not found: %s"), *Label));

        TArray<TSharedPtr<FJsonValue>> CompArray;
        for (UActorComponent* Comp : Actor->GetComponents())
        {
            if (!Comp) continue;
            CompArray.Add(MakeShared<FJsonValueObject>(ComponentToJson(Comp)));
        }

        TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
        Data->SetArrayField(TEXT("components"), CompArray);
        Data->SetNumberField(TEXT("count"), CompArray.Num());
        return MakeSuccessResponse(Data);
    });
    return true;
}

bool UCComponentHandler::HandleAddComponent(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
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

        FString CompClassName = Body->GetStringField(TEXT("class"));
        FString CompName      = Body->GetStringField(TEXT("name"));
        if (CompClassName.IsEmpty()) return MakeErrorResponse(TEXT("class is required"));

        // Find component class
        UClass* CompClass = nullptr;
        for (TObjectIterator<UClass> ClassIt; ClassIt; ++ClassIt)
        {
            if (ClassIt->IsChildOf(UActorComponent::StaticClass()) && ClassIt->GetName() == CompClassName)
            {
                CompClass = *ClassIt;
                break;
            }
        }
        if (!CompClass) return MakeErrorResponse(FString::Printf(TEXT("Component class not found: %s"), *CompClassName));

        FName NewCompName = CompName.IsEmpty() ? FName(*CompClassName) : FName(*CompName);
        UActorComponent* NewComp = NewObject<UActorComponent>(Actor, CompClass, NewCompName);
        if (!NewComp) return MakeErrorResponse(TEXT("Failed to create component"));

        Actor->AddInstanceComponent(NewComp);
        NewComp->RegisterComponent();

        // Set initial transform if scene component
        if (USceneComponent* SceneComp = Cast<USceneComponent>(NewComp))
        {
            if (Body->HasField(TEXT("location")))
                SceneComp->SetRelativeLocation(JsonToVector(Body->GetObjectField(TEXT("location"))));
            if (Body->HasField(TEXT("rotation")))
                SceneComp->SetRelativeRotation(JsonToRotator(Body->GetObjectField(TEXT("rotation"))));

            // Attach to root if no parent
            if (Actor->GetRootComponent() && SceneComp != Actor->GetRootComponent())
            {
                SceneComp->AttachToComponent(Actor->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
            }
        }

        Actor->MarkPackageDirty();
        return MakeSuccessResponse(ComponentToJson(NewComp));
    });
    return true;
}

bool UCComponentHandler::HandleRemoveComponent(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
{
    FString Label     = Request.PathParams.FindRef(TEXT("label"));
    FString CompName  = Request.PathParams.FindRef(TEXT("component"));

    RunOnGameThread(OnComplete, [Label, CompName]() -> FString
    {
        UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
        if (!World) return MakeErrorResponse(TEXT("No editor world"));

        AActor* Actor = nullptr;
        for (TActorIterator<AActor> It(World); It; ++It)
        {
            if ((*It)->GetActorLabel() == Label) { Actor = *It; break; }
        }
        if (!Actor) return MakeErrorResponse(FString::Printf(TEXT("Actor not found: %s"), *Label));

        UActorComponent* Comp = FindComponent(Actor, CompName);
        if (!Comp) return MakeErrorResponse(FString::Printf(TEXT("Component not found: %s"), *CompName));

        Comp->DestroyComponent();
        Actor->MarkPackageDirty();
        return MakeSuccessResponse(FString::Printf(TEXT("Component '%s' removed"), *CompName));
    });
    return true;
}

bool UCComponentHandler::HandleGetComponentProperties(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
{
    FString Label    = Request.PathParams.FindRef(TEXT("label"));
    FString CompName = Request.PathParams.FindRef(TEXT("component"));

    RunOnGameThread(OnComplete, [Label, CompName]() -> FString
    {
        UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
        if (!World) return MakeErrorResponse(TEXT("No editor world"));

        AActor* Actor = nullptr;
        for (TActorIterator<AActor> It(World); It; ++It)
        {
            if ((*It)->GetActorLabel() == Label) { Actor = *It; break; }
        }
        if (!Actor) return MakeErrorResponse(FString::Printf(TEXT("Actor not found: %s"), *Label));

        UActorComponent* Comp = FindComponent(Actor, CompName);
        if (!Comp) return MakeErrorResponse(FString::Printf(TEXT("Component not found: %s"), *CompName));

        TSharedPtr<FJsonObject> Props = MakeShared<FJsonObject>();
        for (TFieldIterator<FProperty> PropIt(Comp->GetClass()); PropIt; ++PropIt)
        {
            FProperty* Prop = *PropIt;
            if (!Prop->HasAnyPropertyFlags(CPF_Edit | CPF_BlueprintVisible)) continue;

            FString ValueStr;
            const void* PropPtr = Prop->ContainerPtrToValuePtr<void>(Comp);
            Prop->ExportTextItem_Direct(ValueStr, PropPtr, nullptr, nullptr, PPF_None);
            Props->SetStringField(Prop->GetName(), ValueStr);
        }
        return MakeSuccessResponse(Props);
    });
    return true;
}

bool UCComponentHandler::HandleSetComponentProperties(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
{
    FString Label    = Request.PathParams.FindRef(TEXT("label"));
    FString CompName = Request.PathParams.FindRef(TEXT("component"));
    TSharedPtr<FJsonObject> Body = ParseJsonBody(Request);

    RunOnGameThread(OnComplete, [Label, CompName, Body]() -> FString
    {
        UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
        if (!World) return MakeErrorResponse(TEXT("No editor world"));

        AActor* Actor = nullptr;
        for (TActorIterator<AActor> It(World); It; ++It)
        {
            if ((*It)->GetActorLabel() == Label) { Actor = *It; break; }
        }
        if (!Actor) return MakeErrorResponse(FString::Printf(TEXT("Actor not found: %s"), *Label));

        UActorComponent* Comp = FindComponent(Actor, CompName);
        if (!Comp) return MakeErrorResponse(FString::Printf(TEXT("Component not found: %s"), *CompName));

        TArray<FString> SetFields;
        for (auto& Pair : Body->Values)
        {
            FProperty* Prop = Comp->GetClass()->FindPropertyByName(FName(*Pair.Key));
            if (!Prop) continue;
            FString ValueStr;
            Pair.Value->TryGetString(ValueStr);
            void* PropPtr = Prop->ContainerPtrToValuePtr<void>(Comp);
            Prop->ImportText_Direct(*ValueStr, PropPtr, Comp, PPF_None);
            SetFields.Add(FString(Pair.Key.ToView()));
        }
        Comp->MarkPackageDirty();

        TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
        TArray<TSharedPtr<FJsonValue>> FieldArr;
        for (const FString& F : SetFields) FieldArr.Add(MakeShared<FJsonValueString>(F));
        Data->SetArrayField(TEXT("set_fields"), FieldArr);
        return MakeSuccessResponse(Data);
    });
    return true;
}

bool UCComponentHandler::HandleGetComponentTransform(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
{
    FString Label    = Request.PathParams.FindRef(TEXT("label"));
    FString CompName = Request.PathParams.FindRef(TEXT("component"));

    RunOnGameThread(OnComplete, [Label, CompName]() -> FString
    {
        UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
        if (!World) return MakeErrorResponse(TEXT("No editor world"));

        AActor* Actor = nullptr;
        for (TActorIterator<AActor> It(World); It; ++It)
        {
            if ((*It)->GetActorLabel() == Label) { Actor = *It; break; }
        }
        if (!Actor) return MakeErrorResponse(FString::Printf(TEXT("Actor not found: %s"), *Label));

        UActorComponent* Comp = FindComponent(Actor, CompName);
        if (!Comp) return MakeErrorResponse(FString::Printf(TEXT("Component not found: %s"), *CompName));

        USceneComponent* SceneComp = Cast<USceneComponent>(Comp);
        if (!SceneComp) return MakeErrorResponse(TEXT("Component is not a SceneComponent"));

        TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
        Data->SetObjectField(TEXT("relative_location"), VectorToJson(SceneComp->GetRelativeLocation()));
        Data->SetObjectField(TEXT("relative_rotation"), RotatorToJson(SceneComp->GetRelativeRotation()));
        Data->SetObjectField(TEXT("relative_scale"), VectorToJson(SceneComp->GetRelativeScale3D()));
        Data->SetObjectField(TEXT("world_location"), VectorToJson(SceneComp->GetComponentLocation()));
        Data->SetObjectField(TEXT("world_rotation"), RotatorToJson(SceneComp->GetComponentRotation()));
        return MakeSuccessResponse(Data);
    });
    return true;
}

bool UCComponentHandler::HandleSetComponentTransform(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
{
    FString Label    = Request.PathParams.FindRef(TEXT("label"));
    FString CompName = Request.PathParams.FindRef(TEXT("component"));
    TSharedPtr<FJsonObject> Body = ParseJsonBody(Request);

    RunOnGameThread(OnComplete, [Label, CompName, Body]() -> FString
    {
        UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
        if (!World) return MakeErrorResponse(TEXT("No editor world"));

        AActor* Actor = nullptr;
        for (TActorIterator<AActor> It(World); It; ++It)
        {
            if ((*It)->GetActorLabel() == Label) { Actor = *It; break; }
        }
        if (!Actor) return MakeErrorResponse(FString::Printf(TEXT("Actor not found: %s"), *Label));

        UActorComponent* Comp = FindComponent(Actor, CompName);
        if (!Comp) return MakeErrorResponse(FString::Printf(TEXT("Component not found: %s"), *CompName));

        USceneComponent* SceneComp = Cast<USceneComponent>(Comp);
        if (!SceneComp) return MakeErrorResponse(TEXT("Component is not a SceneComponent"));

        if (Body->HasField(TEXT("location")))
            SceneComp->SetRelativeLocation(JsonToVector(Body->GetObjectField(TEXT("location"))));
        if (Body->HasField(TEXT("rotation")))
            SceneComp->SetRelativeRotation(JsonToRotator(Body->GetObjectField(TEXT("rotation"))));
        if (Body->HasField(TEXT("scale")))
            SceneComp->SetRelativeScale3D(JsonToVector(Body->GetObjectField(TEXT("scale")), FVector::OneVector));

        Comp->MarkPackageDirty();
        return MakeSuccessResponse(TEXT("Component transform updated"));
    });
    return true;
}

bool UCComponentHandler::HandleApplyMaterialToComponent(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
{
    FString Label    = Request.PathParams.FindRef(TEXT("label"));
    FString CompName = Request.PathParams.FindRef(TEXT("component"));
    TSharedPtr<FJsonObject> Body = ParseJsonBody(Request);

    RunOnGameThread(OnComplete, [Label, CompName, Body]() -> FString
    {
        UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
        if (!World) return MakeErrorResponse(TEXT("No editor world"));

        AActor* Actor = nullptr;
        for (TActorIterator<AActor> It(World); It; ++It)
        {
            if ((*It)->GetActorLabel() == Label) { Actor = *It; break; }
        }
        if (!Actor) return MakeErrorResponse(FString::Printf(TEXT("Actor not found: %s"), *Label));

        UActorComponent* Comp = FindComponent(Actor, CompName);
        if (!Comp) return MakeErrorResponse(FString::Printf(TEXT("Component not found: %s"), *CompName));

        UMeshComponent* MeshComp = Cast<UMeshComponent>(Comp);
        if (!MeshComp) return MakeErrorResponse(TEXT("Component is not a MeshComponent"));

        FString MatPath = Body->GetStringField(TEXT("material_path"));
        int32 SlotIndex = 0;
        Body->TryGetNumberField(TEXT("slot_index"), SlotIndex);

        UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr, *MatPath);
        if (!Material) return MakeErrorResponse(FString::Printf(TEXT("Material not found: %s"), *MatPath));

        MeshComp->SetMaterial(SlotIndex, Material);
        MeshComp->MarkPackageDirty();

        return MakeSuccessResponse(TEXT("Material applied to component"));
    });
    return true;
}

bool UCComponentHandler::HandleListComponentClasses(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
{
    RunOnGameThread(OnComplete, []() -> FString
    {
        TArray<TSharedPtr<FJsonValue>> ClassArray;

        for (TObjectIterator<UClass> ClassIt; ClassIt; ++ClassIt)
        {
            UClass* Class = *ClassIt;
            if (!Class->IsChildOf(UActorComponent::StaticClass())) continue;
            if (Class->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated)) continue;
            if (Class->GetName().StartsWith(TEXT("SKEL_"))) continue;

            TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
            Obj->SetStringField(TEXT("name"), Class->GetName());
            Obj->SetStringField(TEXT("parent"), Class->GetSuperClass() ? Class->GetSuperClass()->GetName() : TEXT(""));
            bool bIsScene = Class->IsChildOf(USceneComponent::StaticClass());
            Obj->SetBoolField(TEXT("is_scene_component"), bIsScene);
            ClassArray.Add(MakeShared<FJsonValueObject>(Obj));
        }

        TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
        Data->SetArrayField(TEXT("classes"), ClassArray);
        Data->SetNumberField(TEXT("count"), ClassArray.Num());
        return MakeSuccessResponse(Data);
    });
    return true;
}

UActorComponent* UCComponentHandler::FindComponent(AActor* Actor, const FString& ComponentName)
{
    for (UActorComponent* Comp : Actor->GetComponents())
    {
        if (Comp && Comp->GetName() == ComponentName) return Comp;
    }
    return nullptr;
}

TSharedPtr<FJsonObject> UCComponentHandler::ComponentToJson(UActorComponent* Comp)
{
    TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
    if (!Comp) return Obj;

    Obj->SetStringField(TEXT("name"), Comp->GetName());
    Obj->SetStringField(TEXT("class"), Comp->GetClass()->GetName());
    Obj->SetBoolField(TEXT("is_active"), Comp->IsActive());

    if (USceneComponent* SceneComp = Cast<USceneComponent>(Comp))
    {
        Obj->SetObjectField(TEXT("relative_location"), VectorToJson(SceneComp->GetRelativeLocation()));
        Obj->SetObjectField(TEXT("relative_rotation"), RotatorToJson(SceneComp->GetRelativeRotation()));
        Obj->SetObjectField(TEXT("relative_scale"), VectorToJson(SceneComp->GetRelativeScale3D()));
        Obj->SetBoolField(TEXT("visible"), SceneComp->IsVisible());
    }

    if (UMeshComponent* MeshComp = Cast<UMeshComponent>(Comp))
    {
        TArray<TSharedPtr<FJsonValue>> MatArr;
        for (int32 i = 0; i < MeshComp->GetNumMaterials(); ++i)
        {
            UMaterialInterface* Mat = MeshComp->GetMaterial(i);
            MatArr.Add(MakeShared<FJsonValueString>(Mat ? Mat->GetPathName() : TEXT("")));
        }
        Obj->SetArrayField(TEXT("materials"), MatArr);
    }

    return Obj;
}
