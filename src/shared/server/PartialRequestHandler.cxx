/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "shared/server/PartialRequestHandler.hxx"
#include "shared/server/handler/RequestHandlerInterface.hxx"
#include "shared/server/handler/RequestHandlerManager.hxx"

namespace
{
[[maybe_unused]] // This seems to be necessary for CLion (clang-tidy?) to stop warning.
std::string getValidatedTarget (const Header& header)
{
    // Request path must be absolute and not contain "..".
    std::string target (header.target());
    if (target.empty()
        || target[0] != '/'
        || target.find("..") != std::string::npos)
    {
        TLOG(ERROR) << "handleRequest has found an error in target '" << target << "'";
        return {};
    }

    return target;
}
};


PartialRequestHandler::PartialRequestHandler (
    const RequestHandlerManager& requestHandlers)
    : mRequestHandlers(requestHandlers)
{
}


std::tuple<bool, std::optional<RequestHandlerManager::MatchingHandler>, ServerResponse> PartialRequestHandler::handleRequest (ServerRequest& request, AccessLog& accessLog)
{
	(void)accessLog; // method signature remains the same, accessLog is used further in the specialization class, not here anymore.
    const Header& header (request.header());
    const std::string target = getValidatedTarget(header);
    if (target.empty())
    {
        TLOG(WARNING) << "invalid request target";
        TVLOG(1) << "invalid request target '" << header.target() << "'";
        return {false, std::nullopt, BaseServerSession::getBadRequestResponse()};
    }

    TVLOG(0) << "handling request to " << target;
    auto matchingHandler = mRequestHandlers.findMatchingHandler(header.method(), target);
    if (matchingHandler.handlerContext == nullptr)
    {
        TLOG(INFO) << "did not find handler for request";
        TVLOG(1) << "did not find a handler for " << header.method() << " " << target;
        return {false, std::nullopt, BaseServerSession::getNotFoundResponse()};
    }

    request.setPathParameters(matchingHandler.handlerContext->pathParameterNames, matchingHandler.pathParameters);
    request.setQueryParameters(std::move(matchingHandler.queryParameters));
    request.setFragment(std::move(matchingHandler.fragment));

    return { true, matchingHandler, ServerResponse() };
}
