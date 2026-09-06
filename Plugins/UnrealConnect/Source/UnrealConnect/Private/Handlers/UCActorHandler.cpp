#include "Handlers/UCActorHandler.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "Editor.h"
#include "EditorLevelUtils.h"
#include "GameFramework/Actor.h"
#include "Components/ActorComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "HttpPath.h"
#include "HttpServerModule.h"
#include "Kismet/GameplayStatics.h"
#include "Selection.h"
#include "UObject/UObjectIterator.h"

void UCActorHandler::RegisterRoutes(TSharedRef<IHttpRouter> Router)
{
    Router->BindRoute(FHttpPath(TEXT("/actors")), EHttpServerRequestVerbs::VERB_GET,
        FHttpRequestHandler::CreateLambda([this](const FHttpServerRequest& Req, const FHttpResultCallback& OnComplete) -> bool { return HandleListActors(Req, OnComplete); }));

    Router->BindRoute(FHttpPath(TEXT("/actors/spawn")), EHttpServerRequestVerbs::VERB_POST,
        FHttpRequestHandler::CreateLambda([this](const FHttpServerRequest& Req, const FHttpResultCallback& OnComplete) -> bool { return HandleSpawnActor(Req, OnComplete); }));

    Router->BindRoute(FHttpPath(TEXT("/actors/classes")), EHttpServerRequestVerbs::VERB_GET,
        FHttpRequestHandler::CreateLambda([this](const FHttpServerRequest& Req, const FHttpResultCallback& OnComplete) -> bool { return HandleListActorClasses(Req, OnComplete); }));

    Router->BindRoute(FHttpPath(TEXT("/actors/:label")), EHttpServerRequestVerbs::VERB_DELETE,
        FHttpRequestHandler::CreateLambda([this](const FHttpServerRequest& Req, const FHttpResultCallback& OnComplete) -> bool { return HandleDeleteActor(Req, OnComplete); }));

    Router->BindRoute(FHttpPath(TEXT("/actors/:label/transform")), EHttpServerRequestVerbs::VERB_GET,
        FHttpRequestHandler::CreateLambda([this](const FHttpServerRequest& Req, const FHttpResultCallback& OnComplete) -> bool { return HandleGetTransform(Req, OnComplete); }));

    Router->BindRoute(FHttpPath(TEXT("/actors/:label/transform")), EHttpServerRequestVerbs::VERB_PUT,
        FHttpRequestHandler::CreateLambda([this](const FHttpServerRequest& Req, const FHttpResultCallback& OnComplete) -> bool { return HandleSetTransform(Req, OnComplete); }));

    Router->BindRoute(FHttpPath(TEXT("/actors/:label/properties")), EHttpServerRequestVerbs::VERB_GET,
        FHttpRequestHandler::CreateLambda([this](const FHttpServerRequest& Req, const FHttpResultCallback& OnComplete) -> bool { return HandleGetProperties(Req, OnComplete); }));

    Router->BindRoute(FHttpPath(TEXT("/actors/:label/properties")), EHttpServerRequestVerbs::VERB_PUT,
        FHttpRequestHandler::CreateLambda([this](const FHttpServerRequest& Req, const FHttpResultCallback& OnComplete) -> bool { return HandleSetProperties(Req, OnComplete); }));

    Router->BindRoute(FHttpPath(TEXT("/actors/:label/components")), EHttpServerRequestVerbs::VERB_GET,
        FHttpRequestHandler::CreateLambda([this](const FHttpServerRequest& Req, const FHttpResultCallback& OnComplete) -> bool { return HandleListComponents(Req, OnComplete); }));

    Router->BindRoute(FHttpPath(TEXT("/actors/:label/visibility")), EHttpServerRequestVerbs::VERB_PUT,
        FHttpRequestHandler::CreateLambda([this](const FHttpServerRequest& Req, const FHttpResultCallback& OnComplete) -> bool { return HandleSetVisibility(Req, OnComplete); }));

    Router->BindRoute(FHttpPath(TEXT("/actors/:label/mobility")), EHttpServerRequestVerbs::VERB_PUT,
        FHttpRequestHandler::CreateLambda([this](const FHttpServerRequest& Req, const FHttpResultCallback& OnComplete) -> bool { return HandleSetMobility(Req, OnComplete); }));

    Router->BindRoute(FHttpPath(TEXT("/actors/:label/tags")), EHttpServerRequestVerbs::VERB_GET,
        FHttpRequestHandler::CreateLambda([this](const FHttpServerRequest& Req, const FHttpResultCallback& OnComplete) -> bool { return HandleGetTags(Req, OnComplete); }));

    Router->BindRoute(FHttpPath(TEXT("/actors/:label/tags")), EHttpServerRequestVerbs::VERB_PUT,
        FHttpRequestHandler::CreateLambda([this](const FHttpServerRequest& Req, const FHttpResultCallback& OnComplete) -> bool { return HandleSetTags(Req, OnComplete); }));

    Router->BindRoute(FHttpPath(TEXT("/actors/:label/duplicate")), EHttpServerRequestVerbs::VERB_POST,
        FHttpRequestHandler::CreateLambda([this](const FHttpServerRequest& Req, const FHttpResultCallback& OnComplete) -> bool { return HandleDuplicateActor(Req, OnComplete); }));
}

bool UCActorHandler::HandleListActors(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
{
    FString ClassFilter = Request.QueryParams.FindRef(TEXT("class"));
    FString NameFilter  = Request.QueryParams.FindRef(TEXT("name"));

    RunOnGameThread(OnComplete, [ClassFilter, NameFilter]() -> FString
    {
        UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
        if (!World) return MakeErrorResponse(TEXT("No editor world"));

        TArray<TSharedPtr<FJsonValue>> ActorArray;

        for (TActorIterator<AActor> It(World); It; ++It)
        {
            AActor* Actor = *It;
            if (!Actor || Actor->IsTemporarilyHiddenInEditor(true)) continue;

            if (!ClassFilter.IsEmpty() && !Actor->GetClass()->GetName().Contains(ClassFilter)) continue;
            if (!NameFilter.IsEmpty() && !Actor->GetActorLabel().Contains(NameFilter)) continue;

            ActorArray.Add(MakeShared<FJsonValueObject>(ActorToJson(Actor)));
        }

        TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
        Data->SetArrayField(TEXT("actors"), ActorArray);
        Data->SetNumberField(TEXT("count"), ActorArray.Num());
        return MakeSuccessResponse(Data);
    });
    return true;
}

bool UCActorHandler::HandleSpawnActor(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
{
    TSharedPtr<FJsonObject> Body = ParseJsonBody(Request);
    if (!Body.IsValid())
    {
        auto Resp = JsonResponse(MakeErrorResponse(TEXT("Invalid JSON body")), EHttpServerResponseCodes::BadRequest);
        OnComplete(MoveTemp(Resp));
        return true;
    }

    FString ClassName = Body->GetStringField(TEXT("class_name"));
    TSharedPtr<FJsonObject> LocJson = Body->GetObjectField(TEXT("location"));
    TSharedPtr<FJsonObject> RotJson = Body->GetObjectField(TEXT("rotation"));
    TSharedPtr<FJsonObject> ScaleJson = Body->GetObjectField(TEXT("scale"));
    FString Label = Body->GetStringField(TEXT("label"));

    RunOnGameThread(OnComplete, [ClassName, LocJson, RotJson, ScaleJson, Label]() -> FString
    {
        UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
        if (!World) return MakeErrorResponse(TEXT("No editor world"));

        // Find class
        UClass* ActorClass = nullptr;
        for (TObjectIterator<UClass> ClassIt; ClassIt; ++ClassIt)
        {
            if (ClassIt->IsChildOf(AActor::StaticClass()) && ClassIt->GetName() == ClassName)
            {
                ActorClass = *ClassIt;
                break;
            }
        }

        if (!ActorClass)
        {
            // Try finding as blueprint
            FString BlueprintPath = FString::Printf(TEXT("/Game/%s.%s_C"), *ClassName, *ClassName);
            ActorClass = LoadObject<UClass>(nullptr, *BlueprintPath);
        }

        if (!ActorClass)
        {
            return MakeErrorResponse(FString::Printf(TEXT("Class not found: %s"), *ClassName));
        }

        FVector Location = JsonToVector(LocJson);
        FRotator Rotation = JsonToRotator(RotJson);
        FVector Scale = ScaleJson.IsValid() ? JsonToVector(ScaleJson, FVector::OneVector) : FVector::OneVector;

        FActorSpawnParameters SpawnParams;
        SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

        AActor* NewActor = World->SpawnActor<AActor>(ActorClass, Location, Rotation, SpawnParams);
        if (!NewActor)
        {
            return MakeErrorResponse(TEXT("Failed to spawn actor"));
        }

        if (!Label.IsEmpty())
        {
            NewActor->SetActorLabel(Label);
        }
        NewActor->SetActorScale3D(Scale);
        GEditor->SelectActor(NewActor, true, true);

        TSharedPtr<FJsonObject> Data = ActorToJson(NewActor, true);
        return MakeSuccessResponse(Data);
    });
    return true;
}

bool UCActorHandler::HandleDeleteActor(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
{
    FString Label = Request.PathParams.FindRef(TEXT("label"));

    RunOnGameThread(OnComplete, [Label]() -> FString
    {
        UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
        if (!World) return MakeErrorResponse(TEXT("No editor world"));

        AActor* Actor = FindActorByLabel(World, Label);
        if (!Actor) return MakeErrorResponse(FString::Printf(TEXT("Actor not found: %s"), *Label));

        World->DestroyActor(Actor);
        return MakeSuccessResponse(FString::Printf(TEXT("Actor '%s' deleted"), *Label));
    });
    return true;
}

bool UCActorHandler::HandleGetTransform(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
{
    FString Label = Request.PathParams.FindRef(TEXT("label"));

    RunOnGameThread(OnComplete, [Label]() -> FString
    {
        UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
        if (!World) return MakeErrorResponse(TEXT("No editor world"));

        AActor* Actor = FindActorByLabel(World, Label);
        if (!Actor) return MakeErrorResponse(FString::Printf(TEXT("Actor not found: %s"), *Label));

        TSharedPtr<FJsonObject> Data = TransformToJson(Actor->GetActorTransform());
        return MakeSuccessResponse(Data);
    });
    return true;
}

bool UCActorHandler::HandleSetTransform(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
{
    FString Label = Request.PathParams.FindRef(TEXT("label"));
    TSharedPtr<FJsonObject> Body = ParseJsonBody(Request);

    RunOnGameThread(OnComplete, [Label, Body]() -> FString
    {
        UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
        if (!World) return MakeErrorResponse(TEXT("No editor world"));

        AActor* Actor = FindActorByLabel(World, Label);
        if (!Actor) return MakeErrorResponse(FString::Printf(TEXT("Actor not found: %s"), *Label));

        if (Body->HasField(TEXT("location")))
        {
            Actor->SetActorLocation(JsonToVector(Body->GetObjectField(TEXT("location"))));
        }
        if (Body->HasField(TEXT("rotation")))
        {
            Actor->SetActorRotation(JsonToRotator(Body->GetObjectField(TEXT("rotation"))));
        }
        if (Body->HasField(TEXT("scale")))
        {
            Actor->SetActorScale3D(JsonToVector(Body->GetObjectField(TEXT("scale")), FVector::OneVector));
        }

        TSharedPtr<FJsonObject> Data = TransformToJson(Actor->GetActorTransform());
        return MakeSuccessResponse(Data);
    });
    return true;
}

bool UCActorHandler::HandleGetProperties(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
{
    FString Label = Request.PathParams.FindRef(TEXT("label"));

    RunOnGameThread(OnComplete, [Label]() -> FString
    {
        UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
        if (!World) return MakeErrorResponse(TEXT("No editor world"));

        AActor* Actor = FindActorByLabel(World, Label);
        if (!Actor) return MakeErrorResponse(FString::Printf(TEXT("Actor not found: %s"), *Label));

        TSharedPtr<FJsonObject> Props = MakeShared<FJsonObject>();
        UClass* Class = Actor->GetClass();

        for (TFieldIterator<FProperty> PropIt(Class); PropIt; ++PropIt)
        {
            FProperty* Prop = *PropIt;
            if (!Prop->HasAnyPropertyFlags(CPF_Edit | CPF_BlueprintVisible)) continue;

            FString ValueStr;
            const void* PropPtr = Prop->ContainerPtrToValuePtr<void>(Actor);
            Prop->ExportTextItem_Direct(ValueStr, PropPtr, nullptr, nullptr, PPF_None);
            Props->SetStringField(Prop->GetName(), ValueStr);
        }

        return MakeSuccessResponse(Props);
    });
    return true;
}

bool UCActorHandler::HandleSetProperties(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
{
    FString Label = Request.PathParams.FindRef(TEXT("label"));
    TSharedPtr<FJsonObject> Body = ParseJsonBody(Request);

    RunOnGameThread(OnComplete, [Label, Body]() -> FString
    {
        UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
        if (!World) return MakeErrorResponse(TEXT("No editor world"));

        AActor* Actor = FindActorByLabel(World, Label);
        if (!Actor) return MakeErrorResponse(FString::Printf(TEXT("Actor not found: %s"), *Label));

        UClass* Class = Actor->GetClass();
        TArray<FString> SetFields;

        for (auto& Pair : Body->Values)
        {
            FProperty* Prop = Class->FindPropertyByName(FName(*Pair.Key));
            if (!Prop) continue;

            FString ValueStr;
            Pair.Value->TryGetString(ValueStr);
            void* PropPtr = Prop->ContainerPtrToValuePtr<void>(Actor);
            Prop->ImportText_Direct(*ValueStr, PropPtr, Actor, PPF_None);
            SetFields.Add(FString(Pair.Key.ToView()));
        }

        Actor->MarkPackageDirty();
        TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
        TArray<TSharedPtr<FJsonValue>> FieldArr;
        for (const FString& F : SetFields) FieldArr.Add(MakeShared<FJsonValueString>(F));
        Data->SetArrayField(TEXT("set_fields"), FieldArr);
        return MakeSuccessResponse(Data);
    });
    return true;
}

bool UCActorHandler::HandleListComponents(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
{
    FString Label = Request.PathParams.FindRef(TEXT("label"));

    RunOnGameThread(OnComplete, [Label]() -> FString
    {
        UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
        if (!World) return MakeErrorResponse(TEXT("No editor world"));

        AActor* Actor = FindActorByLabel(World, Label);
        if (!Actor) return MakeErrorResponse(FString::Printf(TEXT("Actor not found: %s"), *Label));

        TArray<TSharedPtr<FJsonValue>> CompArray;
        for (UActorComponent* Comp : Actor->GetComponents())
        {
            if (!Comp) continue;
            TSharedPtr<FJsonObject> CompObj = MakeShared<FJsonObject>();
            CompObj->SetStringField(TEXT("name"), Comp->GetName());
            CompObj->SetStringField(TEXT("class"), Comp->GetClass()->GetName());

            if (USceneComponent* SceneComp = Cast<USceneComponent>(Comp))
            {
                CompObj->SetObjectField(TEXT("relative_location"), VectorToJson(SceneComp->GetRelativeLocation()));
                CompObj->SetObjectField(TEXT("relative_rotation"), RotatorToJson(SceneComp->GetRelativeRotation()));
                CompObj->SetObjectField(TEXT("relative_scale"), VectorToJson(SceneComp->GetRelativeScale3D()));
            }
            CompArray.Add(MakeShared<FJsonValueObject>(CompObj));
        }

        TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
        Data->SetArrayField(TEXT("components"), CompArray);
        Data->SetNumberField(TEXT("count"), CompArray.Num());
        return MakeSuccessResponse(Data);
    });
    return true;
}

bool UCActorHandler::HandleSetVisibility(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
{
    FString Label = Request.PathParams.FindRef(TEXT("label"));
    TSharedPtr<FJsonObject> Body = ParseJsonBody(Request);

    RunOnGameThread(OnComplete, [Label, Body]() -> FString
    {
        UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
        if (!World) return MakeErrorResponse(TEXT("No editor world"));

        AActor* Actor = FindActorByLabel(World, Label);
        if (!Actor) return MakeErrorResponse(FString::Printf(TEXT("Actor not found: %s"), *Label));

        bool bVisible = true;
        Body->TryGetBoolField(TEXT("visible"), bVisible);
        Actor->SetIsTemporarilyHiddenInEditor(!bVisible);
        Actor->MarkPackageDirty();

        return MakeSuccessResponse(FString::Printf(TEXT("Visibility set to %s"), bVisible ? TEXT("true") : TEXT("false")));
    });
    return true;
}

bool UCActorHandler::HandleSetMobility(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
{
    FString Label = Request.PathParams.FindRef(TEXT("label"));
    TSharedPtr<FJsonObject> Body = ParseJsonBody(Request);

    RunOnGameThread(OnComplete, [Label, Body]() -> FString
    {
        UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
        if (!World) return MakeErrorResponse(TEXT("No editor world"));

        AActor* Actor = FindActorByLabel(World, Label);
        if (!Actor) return MakeErrorResponse(FString::Printf(TEXT("Actor not found: %s"), *Label));

        FString MobilityStr;
        Body->TryGetStringField(TEXT("mobility"), MobilityStr);

        EComponentMobility::Type Mobility = EComponentMobility::Static;
        if (MobilityStr == TEXT("Movable")) Mobility = EComponentMobility::Movable;
        else if (MobilityStr == TEXT("Stationary")) Mobility = EComponentMobility::Stationary;

        if (USceneComponent* Root = Actor->GetRootComponent())
        {
            Root->SetMobility(Mobility);
            Actor->MarkPackageDirty();
        }
        return MakeSuccessResponse(FString::Printf(TEXT("Mobility set to %s"), *MobilityStr));
    });
    return true;
}

bool UCActorHandler::HandleGetTags(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
{
    FString Label = Request.PathParams.FindRef(TEXT("label"));

    RunOnGameThread(OnComplete, [Label]() -> FString
    {
        UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
        if (!World) return MakeErrorResponse(TEXT("No editor world"));

        AActor* Actor = FindActorByLabel(World, Label);
        if (!Actor) return MakeErrorResponse(FString::Printf(TEXT("Actor not found: %s"), *Label));

        TArray<TSharedPtr<FJsonValue>> TagArray;
        for (const FName& Tag : Actor->Tags)
        {
            TagArray.Add(MakeShared<FJsonValueString>(Tag.ToString()));
        }

        TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
        Data->SetArrayField(TEXT("tags"), TagArray);
        return MakeSuccessResponse(Data);
    });
    return true;
}

bool UCActorHandler::HandleSetTags(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
{
    FString Label = Request.PathParams.FindRef(TEXT("label"));
    TSharedPtr<FJsonObject> Body = ParseJsonBody(Request);

    RunOnGameThread(OnComplete, [Label, Body]() -> FString
    {
        UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
        if (!World) return MakeErrorResponse(TEXT("No editor world"));

        AActor* Actor = FindActorByLabel(World, Label);
        if (!Actor) return MakeErrorResponse(FString::Printf(TEXT("Actor not found: %s"), *Label));

        const TArray<TSharedPtr<FJsonValue>>* TagArray;
        if (Body->TryGetArrayField(TEXT("tags"), TagArray))
        {
            Actor->Tags.Empty();
            for (const auto& TagVal : *TagArray)
            {
                FString TagStr;
                if (TagVal->TryGetString(TagStr))
                {
                    Actor->Tags.Add(FName(*TagStr));
                }
            }
            Actor->MarkPackageDirty();
        }
        return MakeSuccessResponse(TEXT("Tags updated"));
    });
    return true;
}

bool UCActorHandler::HandleDuplicateActor(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
{
    FString Label = Request.PathParams.FindRef(TEXT("label"));
    TSharedPtr<FJsonObject> Body = ParseJsonBody(Request);

    RunOnGameThread(OnComplete, [Label, Body]() -> FString
    {
        UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
        if (!World) return MakeErrorResponse(TEXT("No editor world"));

        AActor* Actor = FindActorByLabel(World, Label);
        if (!Actor) return MakeErrorResponse(FString::Printf(TEXT("Actor not found: %s"), *Label));

        FVector Offset = FVector::ZeroVector;
        if (Body->HasField(TEXT("offset")))
        {
            Offset = JsonToVector(Body->GetObjectField(TEXT("offset")));
        }

        FActorSpawnParameters SpawnParams;
        SpawnParams.Template = Actor;
        AActor* NewActor = World->SpawnActor<AActor>(Actor->GetClass(), Actor->GetActorTransform(), SpawnParams);
        if (!NewActor) return MakeErrorResponse(TEXT("Failed to duplicate actor"));

        NewActor->SetActorLocation(Actor->GetActorLocation() + Offset);

        FString NewLabel;
        Body->TryGetStringField(TEXT("new_label"), NewLabel);
        if (!NewLabel.IsEmpty()) NewActor->SetActorLabel(NewLabel);

        return MakeSuccessResponse(ActorToJson(NewActor, true));
    });
    return true;
}

bool UCActorHandler::HandleListActorClasses(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
{
    RunOnGameThread(OnComplete, []() -> FString
    {
        TArray<TSharedPtr<FJsonValue>> ClassArray;

        for (TObjectIterator<UClass> ClassIt; ClassIt; ++ClassIt)
        {
            UClass* Class = *ClassIt;
            if (!Class->IsChildOf(AActor::StaticClass())) continue;
            if (Class->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated)) continue;
            if (Class->GetName().StartsWith(TEXT("SKEL_"))) continue;

            TSharedPtr<FJsonObject> ClassObj = MakeShared<FJsonObject>();
            ClassObj->SetStringField(TEXT("name"), Class->GetName());
            ClassObj->SetStringField(TEXT("path"), Class->GetPathName());
            ClassObj->SetStringField(TEXT("parent"), Class->GetSuperClass() ? Class->GetSuperClass()->GetName() : TEXT(""));
            ClassArray.Add(MakeShared<FJsonValueObject>(ClassObj));
        }

        TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
        Data->SetArrayField(TEXT("classes"), ClassArray);
        Data->SetNumberField(TEXT("count"), ClassArray.Num());
        return MakeSuccessResponse(Data);
    });
    return true;
}

AActor* UCActorHandler::FindActorByLabel(UWorld* World, const FString& Label)
{
    for (TActorIterator<AActor> It(World); It; ++It)
    {
        if ((*It)->GetActorLabel() == Label)
        {
            return *It;
        }
    }
    return nullptr;
}

TSharedPtr<FJsonObject> UCActorHandler::ActorToJson(AActor* Actor, bool bDetailed)
{
    TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
    Obj->SetStringField(TEXT("label"), Actor->GetActorLabel());
    Obj->SetStringField(TEXT("name"), Actor->GetName());
    Obj->SetStringField(TEXT("class"), Actor->GetClass()->GetName());
    Obj->SetObjectField(TEXT("location"), VectorToJson(Actor->GetActorLocation()));

    if (bDetailed)
    {
        Obj->SetObjectField(TEXT("transform"), TransformToJson(Actor->GetActorTransform()));
        Obj->SetBoolField(TEXT("hidden"), Actor->IsTemporarilyHiddenInEditor());
        Obj->SetStringField(TEXT("path"), Actor->GetPathName());

        TArray<TSharedPtr<FJsonValue>> TagArray;
        for (const FName& Tag : Actor->Tags)
        {
            TagArray.Add(MakeShared<FJsonValueString>(Tag.ToString()));
        }
        Obj->SetArrayField(TEXT("tags"), TagArray);
    }

    return Obj;
}
