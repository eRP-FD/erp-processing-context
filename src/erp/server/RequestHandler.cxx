/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2026
 * (C) Copyright IBM Corp. 2021, 2026
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/server/RequestHandler.hxx"

#include "erp/database/PostgresBackendTask.hxx"
#include "erp/database/push/PushEventDataCollector.hxx"
#include "erp/database/push/PushExporterDatabaseException.hxx"
#include "erp/model/push/Channels.hxx"
#include "erp/pc/PcServiceContext.hxx"
#include "erp/service/push/PushEventCreator.hxx"

#include <fmt/format.h>
#include <mutex>

HandlerResult RequestHandler::handleRequest(ServerRequest& request, AccessLog& accessLog)
{
    auto result = PartialRequestHandler::handleRequest(request, accessLog);
    if (not result.success)
    {
        return result;
    }
    std::optional<RequestHandlerManager::MatchingHandler> matchingHandler = result.handler;

    // Hand over request processing to a request handler that has been registered for the method and path
    // of the request URL. In most cases that will be the VAU request handler that unwraps a TEE encrypted
    // request and then processes the inner HTTP request.
    // Other sources of incoming requests will be the HSM.
    ServerResponse response;

    // Create the session object on the stack so it is ensured that the object is destroyed after
    // leaving this method.
    SessionContext session(mServiceContext, request, response, accessLog);
    matchingHandler.value().handlerContext->handler->preHandleRequestHook(session);
    matchingHandler.value().handlerContext->handler->handleRequest(session);

    const auto pushEventFeatureEnabled = Configuration::instance().getBoolValue(ConfigurationKey::FEATURE_PUSH_EVENT);
    if (pushEventFeatureEnabled && session.pushEventDataCollector().isReadyForPushEvent())
    {
        // Push events are generated after the response is written.
        auto postCallback = std::bind(&PushEventCreator::tryPostCreatePushEvent, std::ref(mServiceContext.getPushEventCreator()),
                      session.pushEventDataCollector(), std::ref(mServiceContext));
        return {result.success, std::nullopt, std::move(postCallback), std::move(response)};
    }

    return {result.success, std::nullopt, {}, response};
}
