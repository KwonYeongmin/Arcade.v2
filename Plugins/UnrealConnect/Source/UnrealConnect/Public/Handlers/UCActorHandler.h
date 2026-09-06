#pragma once

#include "UCBaseHandler.h"

/**
 * Actor 관련 HTTP 라우트 핸들러
 *
 * GET    /actors                        - 레벨의 모든 액터 목록 (쿼리: class, name)
 * POST   /actors/spawn                  - 액터 스폰
 * DELETE /actors/:label                 - 액터 삭제
 * GET    /actors/:label/transform       - 트랜스폼 조회
 * PUT    /actors/:label/transform       - 트랜스폼 설정
 * GET    /actors/:label/properties      - 프로퍼티 조회
 * PUT    /actors/:label/properties      - 프로퍼티 설정
 * GET    /actors/:label/components      - 컴포넌트 목록
 * PUT    /actors/:label/visibility      - 가시성 설정
 * PUT    /actors/:label/mobility        - 모빌리티 설정
 * GET    /actors/:label/tags            - 태그 조회
 * PUT    /actors/:label/tags            - 태그 설정
 * POST   /actors/:label/duplicate       - 액터 복제
 * GET    /actors/classes                - 스폰 가능한 클래스 목록
 */
class UNREALCONNECT_API UCActorHandler : public UCBaseHandler
{
public:
    virtual void RegisterRoutes(TSharedRef<IHttpRouter> Router) override;

private:
    // GET /actors
    bool HandleListActors(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete);
    // POST /actors/spawn
    bool HandleSpawnActor(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete);
    // DELETE /actors/:label
    bool HandleDeleteActor(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete);
    // GET /actors/:label/transform
    bool HandleGetTransform(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete);
    // PUT /actors/:label/transform
    bool HandleSetTransform(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete);
    // GET /actors/:label/properties
    bool HandleGetProperties(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete);
    // PUT /actors/:label/properties
    bool HandleSetProperties(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete);
    // GET /actors/:label/components
    bool HandleListComponents(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete);
    // PUT /actors/:label/visibility
    bool HandleSetVisibility(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete);
    // PUT /actors/:label/mobility
    bool HandleSetMobility(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete);
    // GET/PUT /actors/:label/tags
    bool HandleGetTags(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete);
    bool HandleSetTags(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete);
    // POST /actors/:label/duplicate
    bool HandleDuplicateActor(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete);
    // GET /actors/classes
    bool HandleListActorClasses(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete);

    // Helper: find actor by label in editor world
    static AActor* FindActorByLabel(UWorld* World, const FString& Label);
    // Helper: serialize actor to JSON
    static TSharedPtr<FJsonObject> ActorToJson(AActor* Actor, bool bDetailed = false);
};
