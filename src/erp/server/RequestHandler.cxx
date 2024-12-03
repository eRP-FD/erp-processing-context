/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/server/RequestHandler.hxx"


void RequestHandlerBasicAuthentication::handleBasicAuthentication(
    const BaseSessionContext& session, ConfigurationKey credentialsKey) const
{
    SafeString secret(Configuration::instance().getSafeStringValue(credentialsKey));
    ErpExpect(session.request.header().hasHeader(Header::Authorization), HttpStatus::Unauthorized,
              "Authorization required.");
    auto parts = String::split(session.request.header().header(Header::Authorization).value_or(""), " ");
    if (parts.size() == 2)
    {
        SafeString authSecret(std::move(parts[1]));
        ErpExpect(String::toLower(parts[0]) == "basic" && authSecret == secret, HttpStatus::Unauthorized,
                  "Unauthorized");
    }
    else
    {
        ErpExpect(false, HttpStatus::Unauthorized, "Unauthorized (unsupported authorization content.)");
    }
}


std::tuple<bool, std::optional<RequestHandlerManager::MatchingHandler>, ServerResponse>
RequestHandler::handleRequest(ServerRequest& request, AccessLog& accessLog)
{
    auto result = PartialRequestHandler::handleRequest(request, accessLog);
    bool success = std::get<0>(result);
    if (not success)
    {
        return result;
    }
    std::optional<RequestHandlerManager::MatchingHandler> matchingHandler = std::get<1>(result);

    // Hand over request processing to a request handler that has been registered for the method and path
    // of the request URL. In most cases that will be the VAU request handler that unwraps a TEE encrypted
    // request and then processes the inner HTTP request.
    // Other sources of incoming requests will be the HSM.
    ServerResponse response;

    SessionContext session(mServiceContext, request, response, accessLog);
    matchingHandler.value().handlerContext->handler->preHandleRequestHook(session);
    matchingHandler.value().handlerContext->handler->handleRequest(session);

    return {success, std::nullopt, std::move(response)};
}
