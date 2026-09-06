#pragma once

#include "UCBaseHandler.h"

/**
 * Material 관련 HTTP 라우트 핸들러
 *
 * GET  /materials                              - 모든 머티리얼 목록
 * GET  /materials/:path                        - 머티리얼 상세 (파라미터 포함)
 * POST /actors/:label/material                 - 액터에 머티리얼 적용
 * POST /materials/instances                    - 머티리얼 인스턴스 생성
 * PUT  /materials/instances/:path/parameters   - 머티리얼 인스턴스 파라미터 설정
 * GET  /materials/instances/:path/parameters   - 머티리얼 인스턴스 파라미터 조회
 */
class UNREALCONNECT_API UCMaterialHandler : public UCBaseHandler
{
public:
    virtual void RegisterRoutes(TSharedRef<IHttpRouter> Router) override;

private:
    bool HandleListMaterials(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete);
    bool HandleGetMaterial(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete);
    bool HandleApplyMaterialToActor(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete);
    bool HandleCreateMaterialInstance(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete);
    bool HandleSetMaterialInstanceParams(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete);
    bool HandleGetMaterialInstanceParams(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete);

    static TSharedPtr<FJsonObject> MaterialToJson(class UMaterialInterface* Material);
};
