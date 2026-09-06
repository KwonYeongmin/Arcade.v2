#pragma once

#include "UCBaseHandler.h"

/**
 * 에디터 커맨드/유틸리티 HTTP 라우트 핸들러
 *
 * POST /editor/command           - 에디터 콘솔 커맨드 실행
 * POST /editor/python            - 언리얼 내부 Python 스크립트 실행
 * POST /editor/undo              - 실행 취소
 * POST /editor/redo              - 다시 실행
 * POST /editor/screenshot        - 뷰포트 스크린샷
 * POST /editor/refresh-content   - 콘텐츠 브라우저 새로고침
 * GET  /editor/selection         - 현재 선택된 액터 목록
 * POST /editor/select            - 액터 선택
 * GET  /editor/world             - 월드 세팅 조회
 * PUT  /editor/world             - 월드 세팅 수정
 * GET  /editor/log               - 에디터 로그 조회
 */
class UNREALCONNECT_API UCEditorHandler : public UCBaseHandler
{
public:
    virtual void RegisterRoutes(TSharedRef<IHttpRouter> Router) override;

private:
    bool HandleRunCommand(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete);
    bool HandleRunPython(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete);
    bool HandleUndo(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete);
    bool HandleRedo(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete);
    bool HandleScreenshot(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete);
    bool HandleRefreshContent(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete);
    bool HandleGetSelection(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete);
    bool HandleSelectActors(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete);
    bool HandleGetWorldSettings(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete);
    bool HandleSetWorldSettings(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete);
    bool HandleGetLog(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete);
};
