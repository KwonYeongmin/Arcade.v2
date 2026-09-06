#pragma once

#include "UCBaseHandler.h"

/**
 * Blueprint 관련 HTTP 라우트 핸들러
 *
 * GET    /blueprints                          - 프로젝트의 모든 블루프린트 목록
 * POST   /blueprints                          - 새 블루프린트 생성
 * GET    /blueprints/:path                    - 블루프린트 상세 조회 (변수, 함수, 이벤트)
 * POST   /blueprints/:path/compile            - 컴파일
 * GET    /blueprints/:path/variables          - 변수 목록
 * POST   /blueprints/:path/variables          - 변수 생성
 * PUT    /blueprints/:path/variables/:name    - 변수 수정 (기본값, 타입 등)
 * DELETE /blueprints/:path/variables/:name    - 변수 삭제
 * GET    /blueprints/:path/functions          - 함수 목록
 * POST   /blueprints/:path/functions          - 함수 생성
 * GET    /blueprints/:path/graph              - 그래프 노드/연결 목록
 * POST   /blueprints/:path/nodes             - 노드 추가
 * DELETE /blueprints/:path/nodes/:id          - 노드 삭제
 * POST   /blueprints/:path/connections        - 노드 연결
 * POST   /blueprints/:path/events             - 이벤트 노드 추가
 * PUT    /blueprints/:path/nodes/:id/position - 노드 위치 설정
 * POST   /blueprints/:path/reparent           - 부모 클래스 변경
 */
class UNREALCONNECT_API UCBlueprintHandler : public UCBaseHandler
{
public:
    virtual void RegisterRoutes(TSharedRef<IHttpRouter> Router) override;

private:
    bool HandleListBlueprints(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete);
    bool HandleCreateBlueprint(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete);
    bool HandleReadBlueprint(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete);
    bool HandleCompileBlueprint(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete);

    bool HandleListVariables(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete);
    bool HandleCreateVariable(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete);
    bool HandleUpdateVariable(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete);
    bool HandleDeleteVariable(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete);

    bool HandleListFunctions(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete);
    bool HandleCreateFunction(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete);
    bool HandleDeleteFunction(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete);

    bool HandleGetGraph(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete);
    bool HandleAddNode(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete);
    bool HandleDeleteNode(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete);
    bool HandleConnectNodes(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete);
    bool HandleAddEvent(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete);
    bool HandleSetNodePosition(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete);
    bool HandleReparentBlueprint(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete);
    bool HandleAddFunctionInput(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete);
    bool HandleSetPinDefault(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete);

    // Helpers
    static class UBlueprint* FindBlueprint(const FString& PackagePath);
    static TSharedPtr<FJsonObject> VariableToJson(const struct FBPVariableDescription& Var);
    static TSharedPtr<FJsonObject> NodeToJson(class UEdGraphNode* Node);
    static FEdGraphPinType PinTypeFromString(const FString& TypeStr);
    static FString PinTypeToString(const FEdGraphPinType& PinType);
    static FString DecodeBlueprintPath(const FString& EncodedPath);
};
