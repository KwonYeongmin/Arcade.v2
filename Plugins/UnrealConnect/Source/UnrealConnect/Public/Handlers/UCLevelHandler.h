#pragma once

#include "UCBaseHandler.h"

/**
 * Level 관련 HTTP 라우트 핸들러
 *
 * GET  /level/info           - 현재 레벨 정보 (이름, 경로, 액터 수, 라이팅 빌드 여부 등)
 * POST /level/save           - 현재 레벨 저장
 * POST /level/open           - 레벨 열기
 * POST /level/new            - 새 레벨 생성
 * GET  /level/streaming      - 스트리밍 레벨 목록
 * POST /level/streaming      - 스트리밍 레벨 추가
 * POST /level/build-lighting - 라이팅 빌드
 * GET  /level/bounds         - 레벨 바운드 (월드 크기)
 * POST /level/save-all       - 모든 더티 에셋 저장
 */
class UNREALCONNECT_API UCLevelHandler : public UCBaseHandler
{
public:
    virtual void RegisterRoutes(TSharedRef<IHttpRouter> Router) override;

private:
    bool HandleGetLevelInfo(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete);
    bool HandleSaveLevel(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete);
    bool HandleOpenLevel(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete);
    bool HandleNewLevel(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete);
    bool HandleGetStreamingLevels(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete);
    bool HandleAddStreamingLevel(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete);
    bool HandleBuildLighting(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete);
    bool HandleGetLevelBounds(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete);
    bool HandleSaveAll(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete);
};
