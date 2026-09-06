#pragma once

#include "UCBaseHandler.h"

/**
 * Asset 관련 HTTP 라우트 핸들러
 *
 * GET    /assets                  - 에셋 목록 (쿼리: path, class, filter)
 * POST   /assets/import           - 파일에서 에셋 임포트
 * DELETE /assets/:path            - 에셋 삭제
 * POST   /assets/:path/duplicate  - 에셋 복제
 * POST   /assets/:path/rename     - 에셋 이름 변경/이동
 * GET    /assets/:path/metadata   - 에셋 메타데이터 조회
 * PUT    /assets/:path/metadata   - 에셋 메타데이터 수정
 * POST   /assets/folder           - 새 폴더 생성
 * GET    /assets/classes          - 생성 가능한 에셋 클래스 목록
 */
class UNREALCONNECT_API UCAssetHandler : public UCBaseHandler
{
public:
    virtual void RegisterRoutes(TSharedRef<IHttpRouter> Router) override;

private:
    bool HandleListAssets(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete);
    bool HandleImportAsset(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete);
    bool HandleDeleteAsset(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete);
    bool HandleDuplicateAsset(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete);
    bool HandleRenameAsset(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete);
    bool HandleGetMetadata(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete);
    bool HandleSetMetadata(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete);
    bool HandleCreateFolder(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete);
    bool HandleListAssetClasses(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete);

    static TSharedPtr<FJsonObject> AssetDataToJson(const struct FAssetData& AssetData);
};
