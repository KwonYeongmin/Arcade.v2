#pragma once

#include "UCBaseHandler.h"

/**
 * Component 관련 HTTP 라우트 핸들러
 *
 * GET    /actors/:label/components                          - 컴포넌트 목록
 * POST   /actors/:label/components                         - 컴포넌트 추가
 * DELETE /actors/:label/components/:component              - 컴포넌트 제거
 * GET    /actors/:label/components/:component/properties   - 컴포넌트 프로퍼티 조회
 * PUT    /actors/:label/components/:component/properties   - 컴포넌트 프로퍼티 설정
 * GET    /actors/:label/components/:component/transform    - 컴포넌트 상대 트랜스폼 조회
 * PUT    /actors/:label/components/:component/transform    - 컴포넌트 상대 트랜스폼 설정
 * POST   /actors/:label/components/:component/material     - 컴포넌트에 머티리얼 적용
 * GET    /components/classes                               - 추가 가능한 컴포넌트 클래스 목록
 */
class UNREALCONNECT_API UCComponentHandler : public UCBaseHandler
{
public:
    virtual void RegisterRoutes(TSharedRef<IHttpRouter> Router) override;

private:
    bool HandleListComponents(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete);
    bool HandleAddComponent(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete);
    bool HandleRemoveComponent(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete);
    bool HandleGetComponentProperties(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete);
    bool HandleSetComponentProperties(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete);
    bool HandleGetComponentTransform(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete);
    bool HandleSetComponentTransform(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete);
    bool HandleApplyMaterialToComponent(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete);
    bool HandleListComponentClasses(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete);

    static UActorComponent* FindComponent(AActor* Actor, const FString& ComponentName);
    static TSharedPtr<FJsonObject> ComponentToJson(UActorComponent* Component);
};
